#ifndef AUTOWAH_H
#define AUTOWAH_H
#include <guitar_pedal.h>
#include <stdint.h>

void AutoWah_Init(void);
void AutoWah_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
void AutoWah_SetWah(float wah);        // 0.0 - 1.0 (wah amount)
void AutoWah_SetLevel(float level);    // 0.0 - 1.0 (sensitivity)
void AutoWah_SetMix(float mix);        // 0.0 - 1.0 (dry/wet)
void AutoWah_SetWetBoost(float boost); // 0.5 - 3.0 (wet signal multiplier)

#endif // AUTOWAH_H
