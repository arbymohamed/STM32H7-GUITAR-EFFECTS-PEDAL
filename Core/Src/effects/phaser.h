#ifndef PHASER_H
#define PHASER_H
#include <guitar_pedal.h>
#include <stdint.h>

void Phaser_Init(void);
void Phaser_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void Phaser_SetRate(float rate);        // 0.05 - 5.0 Hz
void Phaser_SetDepth(float depth);      // 0.0 - 1.0
void Phaser_SetFeedback(float feedback); // 0.0 - 1.0
void Phaser_SetFreq(float freq);        // 200 - 4000 Hz (center frequency)

#endif // PHASER_H
