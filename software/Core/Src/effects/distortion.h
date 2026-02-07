#ifndef DISTORTION_H
#define DISTORTION_H
#include <guitar_pedal.h>
#include <stdint.h>

// Wavefolder-based distortion effect
void Distortion_Init(void);
void Distortion_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void Distortion_SetGain(float gain);     // 1.0 - 20.0 (input gain)
void Distortion_SetLevel(float level);   // 0.0 - 1.0 (output level)
void Distortion_SetTone(float tone);     // 0.0 - 1.0 (low-pass filter)

#endif // DISTORTION_H
