#ifndef FLANGER_H
#define FLANGER_H
#include <guitar_pedal.h>
#include <stdint.h>

void Flanger_Init(void);
void Flanger_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void Flanger_SetRate(float rate);      // 0.05 - 5.0 Hz
void Flanger_SetDepth(float depth);    // 0.0 - 1.0
void Flanger_SetFeedback(float feedback); // 0.0 - 1.0
void Flanger_SetDelay(float delay_ms); // 0.1 - 7.0 ms

#endif // FLANGER_H
