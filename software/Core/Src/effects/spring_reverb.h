#ifndef SPRING_REVERB_H
#define SPRING_REVERB_H
#include <stdint.h>

void SpringReverb_Init(void);
void SpringReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void SpringReverb_SetMix(float mix);
void SpringReverb_SetTension(float tension); // 0..1 (affects resonant character)
void SpringReverb_SetDecay(float decay);     // seconds
void SpringReverb_SetTone(float tone);       // 0..1

#endif // SPRING_REVERB_H
