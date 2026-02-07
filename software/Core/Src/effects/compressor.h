#ifndef COMPRESSOR_H
#define COMPRESSOR_H
#include <guitar_pedal.h>
#include <stdint.h>

// Professional studio-style compressor with realistic controls
void Compressor_Init(float sample_rate);
void Compressor_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);

// Professional compressor controls (4 main parameters)
void Compressor_SetThreshold(float threshold_db); // -60.0 to 0.0 dB (typical: -20 to -10 dB)
void Compressor_SetRatio(float ratio);            // 1.0 to 20.0 (1:1 to 20:1, >10 is limiting)
void Compressor_SetAttack(float attack_ms);       // 0.1 to 100.0 ms (how fast it clamps down)
void Compressor_SetRelease(float release_ms);     // 10.0 to 1000.0 ms (how fast it recovers)

// Optional: Auto makeup gain (compensates for level loss from compression)
void Compressor_SetAutoMakeup(float enable);      // 0.0 = off, 1.0 = auto gain compensation

#endif // COMPRESSOR_H
