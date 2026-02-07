#include "tremolo.h"
#include <math.h>

#define TWO_PI 6.2831853f

static float tremolo_depth = 0.5f;  // 0.0 to 1.0
static float tremolo_rate = 2.0f;   // Hz
static float tremolo_phase = 0.0f;
static float tremolo_mix = 0.5f;    // 0.0 to 1.0

// Generate LFO value 0..1 (sine only - simplified)
static inline float GenerateLFO(float phase) {
    // Simple sine LFO: sin(phase) ranges -1..1, map to 0..1
    float sine_val = sinf(phase);
    return (sine_val + 1.0f) * 0.5f;
}

void Tremolo_Init(void) {
    tremolo_depth = 0.5f;
    tremolo_rate = 2.0f;
    tremolo_phase = 0.0f;
    tremolo_mix = 0.5f;
}

void Tremolo_SetDepth(float depth) {
    tremolo_depth = (depth < 0.0f) ? 0.0f : (depth > 1.0f) ? 1.0f : depth;
}

void Tremolo_SetRate(float rate) {
    tremolo_rate = (rate < 0.1f) ? 0.1f : (rate > 20.0f) ? 20.0f : rate;
}

void Tremolo_SetMix(float mix) {
    tremolo_mix = (mix < 0.0f) ? 0.0f : (mix > 1.0f) ? 1.0f : mix;
}

void Tremolo_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        float lfo = GenerateLFO(tremolo_phase);
        float gain = 1.0f - tremolo_depth + tremolo_depth * lfo;
        
        float processedL = inL[i] * gain;
        float processedR = inR[i] * gain;
        
        outL[i] = inL[i] * (1.0f - tremolo_mix) + processedL * tremolo_mix;
        outR[i] = inR[i] * (1.0f - tremolo_mix) + processedR * tremolo_mix;
        
        tremolo_phase += TWO_PI * tremolo_rate / 48000.0f;
        if (tremolo_phase >= TWO_PI) tremolo_phase -= TWO_PI;
    }
}