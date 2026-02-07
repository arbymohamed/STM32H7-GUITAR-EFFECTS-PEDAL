#ifndef CHORUS_H
#define CHORUS_H
#include <guitar_pedal.h>
#include <stdint.h>

#define CHORUS_MAX_SAMPLES 1200 // ~25ms at 48kHz

void Chorus_Init(void);
void Chorus_SetRate(float rate);
void Chorus_SetDepth(float depth);
void Chorus_SetMix(float mix);
void Chorus_SetDelay(float delay_ms);
void Chorus_SetFeedback(float feedback);
void Chorus_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size);
static inline float thiran_allpass_interp(
    float x0, float x1,   // two adjacent delay samples
    float mu,             // fractional part, 0..1
    float *y_prev         // state
) {
    float a = (1.0f - mu) / (1.0f + mu);
    float y = -a * (*y_prev) + x0 + a * x1;
    *y_prev = y;
    return y;
}

#endif // CHORUS_H