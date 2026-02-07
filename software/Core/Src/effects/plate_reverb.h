#ifndef PLATE_REVERB_H
#define PLATE_REVERB_H
#include <stdint.h>

void PlateReverb_Init(void);
void PlateReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void PlateReverb_SetMix(float mix);
void PlateReverb_SetTime(float time);
void PlateReverb_SetDamping(float damp);
void PlateReverb_SetTone(float tone); // 0..1 low->dark

#endif // PLATE_REVERB_H
