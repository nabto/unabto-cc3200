#include "adpcm_audio.h"
#include "circ_buff.h"

#include "uart_if.h"
#include "common.h"

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

void adpcm_reset_decoder() {
	leftStepIndexDec = rightStepIndexDec = 0;
	leftPredictedDec = rightPredictedDec = 0;
}

void adpcm_reset_encoder() {
	leftStepIndexEnc = rightStepIndexEnc = 0;
	leftPredictedEnc = rightPredictedEnc = 0;
}

void adpcm_decode(tCircularBuffer *pCircularBuffer, const uint8_t * input, const size_t input_size) {
    uint8_t output[ENCODING_RATIO * 32]; // it's faster to fill buffer in chunks
    unsigned int outputIndex = 0;

    unsigned int i;
    for (i = 0; i < input_size; ++i) {
        int32_t leftCode = input[i] & 0xFF;
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
        output[outputIndex++] = leftPredictedDec;
        output[outputIndex++] = (leftPredictedDec >> 8);
        output[outputIndex++] = rightPredictedDec;
        output[outputIndex++] = (rightPredictedDec >> 8);
        leftStepIndexDec += stepIndexTable[leftCode];
        rightStepIndexDec += stepIndexTable[rightCode];
        if (leftStepIndexDec > 88) leftStepIndexDec = 88;
        if (rightStepIndexDec > 88) rightStepIndexDec = 88;
        if (leftStepIndexDec < 0) leftStepIndexDec = 0;
        if (rightStepIndexDec < 0) rightStepIndexDec = 0;

        if(outputIndex == sizeof(output)) {
        	FillBuffer(pCircularBuffer, output, outputIndex);
        	outputIndex = 0;
        }
    }

    if(outputIndex > 0) {
    	FillBuffer(pCircularBuffer, output, outputIndex);
    }
}

size_t adpcm_encode(tCircularBuffer *pCircularBuffer, uint8_t * output, const size_t output_size) {
	if(GetBufferSize(pRecordBuffer) < ENCODING_RATIO * output_size) {
		return 0;
	}

	size_t inputIndex = 0;
	size_t outputIndex = 0;
	while(output_size > outputIndex) {
		uint8_t b0 = ReadBufferByte(pCircularBuffer, inputIndex++);
		uint8_t b1 = ReadBufferByte(pCircularBuffer, inputIndex++);
		uint8_t b2 = ReadBufferByte(pCircularBuffer, inputIndex++);
		uint8_t b3 = ReadBufferByte(pCircularBuffer, inputIndex++);

		int32_t leftSample  = (int16_t)((b0 & 0xff)|(b1 << 8));
		int32_t rightSample = (int16_t)((b2 & 0xff)|(b3 << 8));

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

		output[outputIndex++] = (uint8_t) ((leftCode << 4) | rightCode);
	}

	return outputIndex;
}
