#include <unabto/unabto_stream.h>
#include <unabto/unabto_memory.h>
#include <stream_audio.h>

#include "circ_buff.h"

extern tCircularBuffer *pPlayBuffer;
extern tCircularBuffer *pRecordBuffer;

int8_t stepIndexTable[] = {
    8, 6, 4, 2, -1, -1, -1, -1, -1, -1, -1, -1, 2, 4, 6, 8
};

int16_t stepTable[] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

int32_t leftStepIndexDec, rightStepIndexDec, leftPredictedDec, rightPredictedDec;
int32_t leftStepIndexEnc, rightStepIndexEnc, leftPredictedEnc, rightPredictedEnc;

void reset() {
	leftStepIndexDec = rightStepIndexDec = 0;
	leftPredictedDec = rightPredictedDec = 0;

	leftStepIndexEnc = rightStepIndexEnc = 0;
	leftPredictedEnc = rightPredictedEnc = 0;
}

void decode_and_play(const int8_t * input, size_t size) {
	//nabto_stamp_t start = nabtoGetStamp();

    size_t inputIndex;
    uint8_t output[4*10];
    size_t ptr = 0;
    for (inputIndex = 0; inputIndex < size; inputIndex++) {
        int32_t leftCode = input[inputIndex] & 0xFF;
        int32_t rightCode = leftCode & 0xF;
        leftCode = leftCode >> 4;
        int32_t leftStep = stepTable[leftStepIndexDec];
        int32_t rightStep = stepTable[rightStepIndexDec];
        leftPredictedDec += ((leftCode * leftStep) >> 2) - ((15 * leftStep) >> 3);
        rightPredictedDec += ((rightCode * rightStep) >> 2) - ((15 * rightStep) >> 3);
        if (leftPredictedDec > 32767) leftPredictedDec = 32767;
        if (rightPredictedDec > 32767) rightPredictedDec = 32767;
        if (leftPredictedDec < -32768) leftPredictedDec = -32768;
        if (rightPredictedDec < -32768) rightPredictedDec = -32768;
        output[ptr++] = leftPredictedDec;
        output[ptr++] = (leftPredictedDec >> 8);
        output[ptr++] = rightPredictedDec;
        output[ptr++] = (rightPredictedDec >> 8);
        leftStepIndexDec += stepIndexTable[leftCode];
        rightStepIndexDec += stepIndexTable[rightCode];
        if (leftStepIndexDec > 88) leftStepIndexDec = 88;
        if (rightStepIndexDec > 88) rightStepIndexDec = 88;
        if (leftStepIndexDec < 0) leftStepIndexDec = 0;
        if (rightStepIndexDec < 0) rightStepIndexDec = 0;

        if(ptr >= 4*10) { // this speeds up the slow FillBuffer (chunked fill)
        	long lRetVal = FillBuffer(pPlayBuffer, output, 4*10);
			if (lRetVal < 0) {
				UART_PRINT("Unable to fill play buffer\n\r");
				LOOP_FOREVER();
			}
			ptr = 0;
        }
    }

    //nabto_stamp_t end = nabtoGetStamp();
    //UART_PRINT("%u %lu\n\r", size, end - start);
}


bool record_and_encode(int8_t * output, size_t size) {
	unsigned int available = GetBufferSize(pRecordBuffer);
	if(available >= 4 * size) {
		uint8_t *input = pRecordBuffer->pucReadPtr;

		size_t inputIndex = 0, outputIndex;
		for (outputIndex = 0; outputIndex < size; ++outputIndex) {

			int32_t leftSample  = (int16_t)(( input[inputIndex]   & 0xff )|( input[inputIndex+1] << 8 ));
			int32_t rightSample = (int16_t)(( input[inputIndex+2] & 0xff )|( input[inputIndex+3] << 8 ));
			inputIndex += 4;

		    int32_t leftStep = stepTable[leftStepIndexEnc];
		    int32_t rightStep = stepTable[rightStepIndexEnc];
		    int32_t leftCode = ((leftSample - leftPredictedEnc) * 4 + leftStep * 8) / leftStep;
		    int32_t rightCode = ((rightSample - rightPredictedEnc) * 4 + rightStep * 8) / rightStep;
			if (leftCode > 15) leftCode = 15;
			if (rightCode > 15) rightCode = 15;
			if (leftCode < 0) leftCode = 0;
			if (rightCode < 0) rightCode = 0;
			leftPredictedEnc += ((leftCode * leftStep) >> 2) - ((15 * leftStep) >> 3);
			rightPredictedEnc += ((rightCode * rightStep) >> 2) - ((15 * rightStep) >> 3);
			if (leftPredictedEnc > 32767) leftPredictedEnc = 32767;
			if (rightPredictedEnc > 32767) rightPredictedEnc = 32767;
			if (leftPredictedEnc < -32768) leftPredictedEnc = -32768;
			if (rightPredictedEnc < -32768) rightPredictedEnc = -32768;
			leftStepIndexEnc += stepIndexTable[leftCode];
			rightStepIndexEnc += stepIndexTable[rightCode];
			if (leftStepIndexEnc > 88) leftStepIndexEnc = 88;
			if (rightStepIndexEnc > 88) rightStepIndexEnc = 88;
			if (leftStepIndexEnc < 0) leftStepIndexEnc = 0;
			if (rightStepIndexEnc < 0) rightStepIndexEnc = 0;
			output[outputIndex] = (int8_t) ((leftCode << 4) | rightCode);
		}

		UpdateReadPtr(pRecordBuffer, 4 * size);
		return true;
	}
	return false;
}


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
    audio_stream* echo = &audio_streams[unabto_stream_index(stream)];

    if (echo->state == STREAM_STATE_IDLE) {
        return;
    }

    if (echo->state == STREAM_STATE_READ_COMMAND) {
        const uint8_t* buf;
        unabto_stream_hint hint;
        size_t readLength = unabto_stream_read(stream, &buf, &hint);

        if (readLength > 0) {
            size_t i;
            size_t ackLength = 0;

            for (i = 0; i < readLength; i++) {
                ackLength++;
                if (echo->commandLength == strlen(audioString)) {
                    if (buf[i] == '\n') {
                        echo->state = STREAM_STATE_COMMAND_OK;
                        break;
                    }
                } else {
                    if (buf[i] != audioString[echo->commandLength] ||
                        echo->commandLength > strlen(audioString)) {
                        echo->state = STREAM_STATE_COMMAND_FAIL;
                        ackLength = readLength;
                        break;
                    }
                    echo->commandLength++;
                }
            }
            if (!unabto_stream_ack(stream, buf, ackLength, &hint)) {
                echo->state = STREAM_STATE_CLOSING;
            }

        } else {
            if (hint != UNABTO_STREAM_HINT_OK) {
                echo->state = STREAM_STATE_CLOSING;
            }
        }
    }

    if (echo->state == STREAM_STATE_COMMAND_FAIL) {
        const char* failString = "-\n";
        unabto_stream_hint hint;
        unabto_stream_write(stream, (uint8_t*)failString, strlen(failString),
                            &hint);
        echo->state = STREAM_STATE_CLOSING;
    }

    if (echo->state == STREAM_STATE_COMMAND_OK) {
        const char* okString = "+\n";
        unabto_stream_hint hint;
        size_t wrote = unabto_stream_write(stream, (uint8_t*)okString,
                                           strlen(okString), &hint);
        if (wrote != strlen(okString)) {
            echo->state = STREAM_STATE_CLOSING;
        } else {
        	reset();
            echo->state = STREAM_STATE_FORWARDING;
        }
    }

    if (echo->state == STREAM_STATE_FORWARDING) {
        const uint8_t* buf;
        unabto_stream_hint hint;
        size_t readLength = unabto_stream_read(stream, &buf, &hint);

        if (readLength > 0) {
        	decode_and_play((const int8_t*) buf, readLength);

        	/*
        	// loop back client audio
        	size_t writeLength =
                unabto_stream_write(stream, buf, readLength, &hint);
            if (writeLength > 0) {
                if (!unabto_stream_ack(stream, buf, writeLength, &hint)) {
                    echo->state = STREAM_STATE_CLOSING;
                }
            } else {
                if (hint != UNABTO_STREAM_HINT_OK) {
                    echo->state = STREAM_STATE_CLOSING;
                }
            }
        	*/

        	// send recorded audio
        	if (!unabto_stream_ack(stream, buf, readLength, &hint)) {
				echo->state = STREAM_STATE_CLOSING;
			} else if(record_and_encode((int8_t *)buf, readLength)) {
        		size_t writeLength =
					unabto_stream_write(stream, buf, readLength, &hint);
				if (writeLength <= 0 && hint != UNABTO_STREAM_HINT_OK) {
					echo->state = STREAM_STATE_CLOSING;
				}
        	}

        } else {
            if (hint != UNABTO_STREAM_HINT_OK) {
                echo->state = STREAM_STATE_CLOSING;
            }
        }
    }

    if (echo->state == STREAM_STATE_CLOSING) {
        if (unabto_stream_close(stream)) {
            unabto_stream_release(stream);
            echo->state = STREAM_STATE_IDLE;
        }
    }
}
