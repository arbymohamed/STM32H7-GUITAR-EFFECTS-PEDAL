#ifndef SHIMMER_REVERB_H
#define SHIMMER_REVERB_H
#include <stdint.h>

void ShimmerReverb_Init(void);
void ShimmerReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void ShimmerReverb_SetMix(float mix);
void ShimmerReverb_SetTime(float time);
void ShimmerReverb_SetDepth(float depth); // modulation depth
void ShimmerReverb_SetRate(float rate);   // modulation rate (Hz)

#endif // SHIMMER_REVERB_H
