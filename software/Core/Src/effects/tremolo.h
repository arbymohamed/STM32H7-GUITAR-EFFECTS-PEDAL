#ifndef TREMOLO_H
#define TREMOLO_H
#include <guitar_pedal.h>
#include <stdint.h>

void Tremolo_Init(void);
void Tremolo_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void Tremolo_SetDepth(float depth);      // 0.0 to 1.0
void Tremolo_SetRate(float rate);        // 0.1 to 20.0 Hz
void Tremolo_SetMix(float mix);          // 0.0 to 1.0 (dry to wet)

#endif // TREMOLO_H