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

void play(const uint8_t *buf, size_t size) {
	for(;;) {
		long lRetVal = FillBuffer(pPlayBuffer, (uint8_t*)buf, size);
		if (lRetVal < 0) {
			UART_PRINT("Play buffer full\n\r");
		} else {
			return;
		}
	}
}

void adpcm_decode_and_play(const int8_t * input, size_t size) {
    uint8_t output[64]; // must be multiple of 4
    size_t outputIndex = 0;

    size_t inputIndex = 0;
    while (inputIndex < size) {
        int32_t leftCode = input[inputIndex++] & 0xFF;
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
        	play(output, outputIndex);
        	outputIndex = 0;
        }
    }

    if(outputIndex > 0) {
    	play(output, outputIndex);
    }
}

size_t adpcm_record_and_encode(int8_t * output, size_t size) {
	size_t available = GetBufferSize(pRecordBuffer);
	uint8_t *input = pRecordBuffer->pucReadPtr;

	if(available < 4*2048) return 0;

	size_t inputIndex = 0;
	size_t outputIndex = 0;
	while(available >= inputIndex + 4 && size > outputIndex) {
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

		output[outputIndex++] = (int8_t) ((leftCode << 4) | rightCode);
	}

	if(inputIndex > 0) {
		UpdateReadPtr(pRecordBuffer, inputIndex);
	}

	return outputIndex;
}
