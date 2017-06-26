#ifndef ADPCM_AUDIO_H_
#define ADPCM_AUDIO_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void adpcm_reset_decoder();
void adpcm_reset_encoder();

void adpcm_decode_and_play(const int8_t * input, size_t size);
size_t adpcm_record_and_encode(int8_t * output, size_t size);

#endif
