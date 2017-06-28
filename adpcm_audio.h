#ifndef ADPCM_AUDIO_H_
#define ADPCM_AUDIO_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "circ_buff.h"

#define ENCODING_RATIO 4

void adpcm_reset_decoder();
void adpcm_reset_encoder();

void adpcm_decode(tCircularBuffer *pCircularBuffer, const uint8_t * input, const size_t input_size);
size_t adpcm_encode(tCircularBuffer *pCircularBuffer, uint8_t * output, const size_t output_size);

#endif
