#ifndef HALL_REVERB_H
#define HALL_REVERB_H
#include <stdint.h>

void HallReverb_Init(void);
void HallReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void HallReverb_SetMix(float mix);        // 0..1
void HallReverb_SetTime(float time);      // seconds (0.01 - 10.0)
void HallReverb_SetDamping(float damp);   // 0..1
void HallReverb_SetPreDelayMs(float ms);  // 0 - 50 ms (reduced for embedded RAM constraints)

#endif // HALL_REVERB_H
