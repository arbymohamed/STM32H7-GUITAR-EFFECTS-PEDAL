#ifndef DELAY_H
#define DELAY_H
#include <guitar_pedal.h>
#include <stdint.h>

void Delay_Init(void);
void Delay_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void Delay_SetTime(float time_ms);     // 0 - 800 ms (optimized for guitar)
void Delay_SetFeedback(float feedback); // 0.0 - 1.0 (safety limited to 0.98 internally)
void Delay_SetMix(float mix);          // 0.0 - 1.0 (linear crossfade)
void Delay_SetTone(float tone);        // 0.0 (dark) - 1.0 (bright)

#endif // DELAY_H
