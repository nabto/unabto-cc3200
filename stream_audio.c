#include <stream_audio.h>

#include <unabto/unabto_stream.h>
#include <unabto/unabto_memory.h>
#include "adpcm_audio.h"
#include "audio_config.h"
#include "circ_buff.h"

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

extern tCircularBuffer *pRecordBuffer;
extern tCircularBuffer *pPlayBuffer;

typedef enum {
    STREAM_STATE_IDLE,
    STREAM_STATE_READ_COMMAND,
    STREAM_STATE_COMMAND_FAIL,
    STREAM_STATE_COMMAND_OK,
    STREAM_STATE_STREAMING,
    STREAM_STATE_CLOSING
} stream_state;

const char* audioString = "audio";

typedef struct {
    uint16_t commandLength;
    bool commandOk;
    stream_state state;
    unabto_stream* stream;
} audio_stream;

audio_stream my_audio_stream;

uint8_t encodedBuf[PACKET_SIZE];

void stream_audio_init() {
	my_audio_stream.state = STREAM_STATE_IDLE;
}

void unabto_stream_accept(unabto_stream* stream) {
    UNABTO_ASSERT(my_audio_stream.state == STREAM_STATE_IDLE);
    my_audio_stream.commandOk = false;
    my_audio_stream.commandLength = 0;
    my_audio_stream.stream = stream;
    my_audio_stream.state = STREAM_STATE_READ_COMMAND;
}

void unabto_stream_event(unabto_stream* stream, unabto_stream_event_type type) {
    UNABTO_ASSERT(my_audio_stream.stream == stream);

    if (my_audio_stream.state == STREAM_STATE_IDLE) {
        return;
    }

    if (my_audio_stream.state == STREAM_STATE_READ_COMMAND) {
        const uint8_t* buf;
        unabto_stream_hint hint;
        size_t readLength = unabto_stream_read(stream, &buf, &hint);

        if (readLength > 0) {
            size_t i;
            size_t ackLength = 0;

            for (i = 0; i < readLength; i++) {
                ackLength++;
                if (my_audio_stream.commandLength == strlen(audioString)) {
                    if (buf[i] == '\n') {
                        my_audio_stream.state = STREAM_STATE_COMMAND_OK;
                        break;
                    }
                } else {
                    if (buf[i] != audioString[my_audio_stream.commandLength] ||
                        my_audio_stream.commandLength > strlen(audioString)) {
                        my_audio_stream.state = STREAM_STATE_COMMAND_FAIL;
                        ackLength = readLength;
                        break;
                    }
                    my_audio_stream.commandLength++;
                }
            }
            if (!unabto_stream_ack(stream, buf, ackLength, &hint)) {
                my_audio_stream.state = STREAM_STATE_CLOSING;
            }

        } else {
            if (hint != UNABTO_STREAM_HINT_OK) {
                my_audio_stream.state = STREAM_STATE_CLOSING;
            }
        }
    }

    if (my_audio_stream.state == STREAM_STATE_COMMAND_FAIL) {
        const char* failString = "-\n";
        unabto_stream_hint hint;
        unabto_stream_write(stream, (uint8_t*)failString, strlen(failString),
                            &hint);
        my_audio_stream.state = STREAM_STATE_CLOSING;
    }

    if (my_audio_stream.state == STREAM_STATE_COMMAND_OK) {
        const char* okString = "+\n";
        unabto_stream_hint hint;
        size_t wrote = unabto_stream_write(stream, (uint8_t*)okString,
                                           strlen(okString), &hint);
        if (wrote != strlen(okString)) {
            my_audio_stream.state = STREAM_STATE_CLOSING;
        } else {
        	adpcm_reset_decoder();
        	adpcm_reset_encoder();
            my_audio_stream.state = STREAM_STATE_STREAMING;
        }
    }

    if (my_audio_stream.state == STREAM_STATE_STREAMING) {
		const uint8_t* buf;
		unabto_stream_hint hint;
		size_t readLength = unabto_stream_read(stream, &buf, &hint);
		if (readLength > 0) {
			adpcm_decode(pPlayBuffer, buf, readLength);
			if (!unabto_stream_ack(stream, buf, readLength, &hint)) {
				my_audio_stream.state = STREAM_STATE_CLOSING;
			}
		} else {
			if (hint != UNABTO_STREAM_HINT_OK) {
				my_audio_stream.state = STREAM_STATE_CLOSING;
			}
		}

		//float playBufFilled = (GetBufferSize(pPlayBuffer) * 100.0) / (float)(PLAY_BUFFER_SIZE);
		//float recordBufFilled = (GetBufferSize(pRecordBuffer) * 100.0) / (float)(RECORD_BUFFER_SIZE);
		//UART_PRINT("rec buf: %4.1f %%  play buf: %4.1f %%\n\r", recordBufFilled, playBufFilled);
    }

    if (my_audio_stream.state == STREAM_STATE_CLOSING) {
        if (unabto_stream_close(stream)) {
            unabto_stream_release(stream);
            my_audio_stream.state = STREAM_STATE_IDLE;
        }
    }
}


void stream_audio_write() {
	if (my_audio_stream.state != STREAM_STATE_STREAMING) {
		return;
	}
	size_t encodedLen = adpcm_encode(pRecordBuffer, encodedBuf, sizeof(encodedBuf));
	if (encodedLen == 0) {
		return;
	} else {
		UNABTO_ASSERT(encodedLen == sizeof(encodedBuf));
	}

	unabto_stream_hint hint;
	size_t writeLength =
			unabto_stream_write(my_audio_stream.stream, encodedBuf, sizeof(encodedBuf), &hint);

	if (writeLength <= 0 && hint != UNABTO_STREAM_HINT_OK) {
		my_audio_stream.state = STREAM_STATE_CLOSING;
	}

	UpdateReadPtr(pRecordBuffer, writeLength * ENCODING_RATIO);
}
