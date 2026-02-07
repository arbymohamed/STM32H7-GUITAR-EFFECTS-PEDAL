#ifndef OVERDRIVE_H
#define OVERDRIVE_H

#include <stdint.h>

// Simplified overdrive using DaisySP SoftClip
typedef struct {
    float drive;           // 1.0 - 10.0 (input gain)
    float tone;            // 0.0 - 1.0 (one-pole filter cutoff)
    float level;           // 0.0 - 1.0 (output level)
    float tone_state_l;    // Filter state for left channel
    float tone_state_r;    // Filter state for right channel
} Overdrive_State;

void Overdrive_Init(void);
void Overdrive_SetGain(float gain);      // Drive: 1.0 - 10.0
void Overdrive_SetLevel(float level);    // Level: 0.0 - 1.0
void Overdrive_SetTone(float tone);      // Tone: 0.0 - 1.0
void Overdrive_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);

#endif // OVERDRIVE_H
