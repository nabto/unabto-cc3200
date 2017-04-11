#include <stream_audio.h>

#include <unabto/unabto_stream.h>
#include <unabto/unabto_memory.h>
#include "adpcm_audio.h"

/**
 * Stream audio server
 *
 * The audio server first receives an ``audio'' command, then it sends a
 * ``+'' if it accepts the request. When the command is accepted the
 * state is switched to an audio state.
 *
 * recv: audio\n
 * send: +\n
 * play audio data until close
 */

typedef enum {
    STREAM_STATE_IDLE,
    STREAM_STATE_READ_COMMAND,
    STREAM_STATE_COMMAND_FAIL,
    STREAM_STATE_COMMAND_OK,
    STREAM_STATE_FORWARDING,
    STREAM_STATE_CLOSING
} stream_state;

const char* audioString = "audio";

typedef struct {
    uint16_t commandLength;
    bool commandOk;
    stream_state state;
} audio_stream;

audio_stream audio_streams[NABTO_MEMORY_STREAM_MAX_STREAMS];

void stream_audio_init() {
    memset(audio_streams, 0, sizeof(audio_streams));
}

void unabto_stream_accept(unabto_stream* stream) {
    audio_stream* echo = &audio_streams[unabto_stream_index(stream)];
    UNABTO_ASSERT(echo->state == STREAM_STATE_IDLE);
    memset(echo, 0, sizeof(audio_stream));
    echo->state = STREAM_STATE_READ_COMMAND;
}

void unabto_stream_event(unabto_stream* stream, unabto_stream_event_type type) {
    audio_stream* audio = &audio_streams[unabto_stream_index(stream)];

    if (audio->state == STREAM_STATE_IDLE) {
        return;
    }

    if (audio->state == STREAM_STATE_READ_COMMAND) {
        const uint8_t* buf;
        unabto_stream_hint hint;
        size_t readLength = unabto_stream_read(stream, &buf, &hint);

        if (readLength > 0) {
            size_t i;
            size_t ackLength = 0;

            for (i = 0; i < readLength; i++) {
                ackLength++;
                if (audio->commandLength == strlen(audioString)) {
                    if (buf[i] == '\n') {
                        audio->state = STREAM_STATE_COMMAND_OK;
                        break;
                    }
                } else {
                    if (buf[i] != audioString[audio->commandLength] ||
                        audio->commandLength > strlen(audioString)) {
                        audio->state = STREAM_STATE_COMMAND_FAIL;
                        ackLength = readLength;
                        break;
                    }
                    audio->commandLength++;
                }
            }
            if (!unabto_stream_ack(stream, buf, ackLength, &hint)) {
                audio->state = STREAM_STATE_CLOSING;
            }

        } else {
            if (hint != UNABTO_STREAM_HINT_OK) {
                audio->state = STREAM_STATE_CLOSING;
            }
        }
    }

    if (audio->state == STREAM_STATE_COMMAND_FAIL) {
        const char* failString = "-\n";
        unabto_stream_hint hint;
        unabto_stream_write(stream, (uint8_t*)failString, strlen(failString),
                            &hint);
        audio->state = STREAM_STATE_CLOSING;
    }

    if (audio->state == STREAM_STATE_COMMAND_OK) {
        const char* okString = "+\n";
        unabto_stream_hint hint;
        size_t wrote = unabto_stream_write(stream, (uint8_t*)okString,
                                           strlen(okString), &hint);
        if (wrote != strlen(okString)) {
            audio->state = STREAM_STATE_CLOSING;
        } else {
        	adpcm_reset_decoder();
        	adpcm_reset_encoder();
            audio->state = STREAM_STATE_FORWARDING;
        }
    }

    if (audio->state == STREAM_STATE_FORWARDING) {
        const uint8_t* buf;
        unabto_stream_hint hint;
        size_t readLength = unabto_stream_read(stream, &buf, &hint);

        if (readLength > 0) {
        	size_t playLength = adpcm_decode_and_play((const int8_t*)buf, readLength);

        	// ack consumed bytes
        	if (!unabto_stream_ack(stream, buf, playLength, &hint)) {
				audio->state = STREAM_STATE_CLOSING;
			}

        } else {
            if (hint != UNABTO_STREAM_HINT_OK) {
                audio->state = STREAM_STATE_CLOSING;
            }
        }

        // send recorded audio
        if(adpcm_record_and_encode((int8_t*)buf, readLength)) { // todo use another buffer
			size_t writeLength =
				unabto_stream_write(stream, buf, readLength, &hint);
			if (writeLength <= 0 && hint != UNABTO_STREAM_HINT_OK) {
				audio->state = STREAM_STATE_CLOSING;
			}
		}
    }

    if (audio->state == STREAM_STATE_CLOSING) {
        if (unabto_stream_close(stream)) {
            unabto_stream_release(stream);
            audio->state = STREAM_STATE_IDLE;
        }
    }
}
