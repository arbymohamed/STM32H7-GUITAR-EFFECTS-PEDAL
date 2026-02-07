#include "phaser.h"
#include <string.h>
#include <math.h>

#define PHASER_STAGES 4 // 4 allpass filters for classic phaser sound

static float phaser_rate = 0.5f;       // Hz
static float phaser_depth = 0.7f;      // 0.0 to 1.0
static float phaser_feedback = 0.5f;   // 0.0 to 1.0
static float phaser_freq = 1000.0f;    // Hz
static float phaser_phase = 0.0f;
static float phaser_sample_rate = 48000.0f;

// Allpass filter states for stereo (4 stages each)
static float ap_stateL[PHASER_STAGES];
static float ap_stateR[PHASER_STAGES];
static float feedback_stateL = 0.0f;
static float feedback_stateR = 0.0f;

// Cached LFO value and coefficient
static float cached_lfo = 0.0f;
static float cached_coeff = 0.0f;

// Simple allpass filter
static inline float allpass(float in, float *state, float coeff) {
    float out = *state + in * coeff;
    *state = in - out * coeff;
    return out;
}

void Phaser_Init(void) {
    memset(ap_stateL, 0, sizeof(ap_stateL));
    memset(ap_stateR, 0, sizeof(ap_stateR));
    feedback_stateL = 0.0f;
    feedback_stateR = 0.0f;
    phaser_phase = 0.0f;
    cached_lfo = 0.0f;
    cached_coeff = 0.0f;
}

void Phaser_SetRate(float rate) {
    phaser_rate = (rate < 0.05f) ? 0.05f : (rate > 5.0f) ? 5.0f : rate;
}

void Phaser_SetDepth(float depth) {
    phaser_depth = (depth < 0.0f) ? 0.0f : (depth > 1.0f) ? 1.0f : depth;
}

void Phaser_SetFeedback(float feedback) {
    phaser_feedback = (feedback < 0.0f) ? 0.0f : (feedback > 1.0f) ? 1.0f : feedback * 0.95f;
}

void Phaser_SetFreq(float freq) {
    phaser_freq = (freq < 200.0f) ? 200.0f : (freq > 4000.0f) ? 4000.0f : freq;
}

void Phaser_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    float lfo_inc = phaser_rate / phaser_sample_rate;
    
    for (uint16_t i = 0; i < size; i++) {
        // Triangle LFO (0..1)
        phaser_phase += lfo_inc;
        if (phaser_phase > 1.0f) phaser_phase -= 1.0f;
        float lfo = (phaser_phase < 0.5f) ? (2.0f * phaser_phase) : (2.0f - 2.0f * phaser_phase);
        
        // Only recalculate coefficient when LFO changes significantly (every few samples)
        if (i == 0 || fabsf(lfo - cached_lfo) > 0.01f) {
            cached_lfo = lfo;
            
            // Modulate allpass frequency
            float mod_freq = phaser_freq * (0.5f + phaser_depth * lfo);
            
            // Calculate allpass coefficient from frequency (simplified, clamped)
            float omega = 2.0f * 3.14159265f * mod_freq / phaser_sample_rate;
            cached_coeff = (1.0f - omega) / (1.0f + omega);
            // Clamp to prevent instability
            if (cached_coeff > 0.99f) cached_coeff = 0.99f;
            if (cached_coeff < -0.99f) cached_coeff = -0.99f;
        }
        
        // Left channel: input + feedback
        float inputL = inL[i] + feedback_stateL * phaser_feedback;
        float processedL = inputL;
        
        // Chain of allpass filters
        for (int j = 0; j < PHASER_STAGES; j++) {
            processedL = allpass(processedL, &ap_stateL[j], cached_coeff);
        }
        
        feedback_stateL = processedL;
        outL[i] = (inL[i] + processedL) * 0.5f; // Mix with dry
        
        // Right channel
        float inputR = inR[i] + feedback_stateR * phaser_feedback;
        float processedR = inputR;
        
        for (int j = 0; j < PHASER_STAGES; j++) {
            processedR = allpass(processedR, &ap_stateR[j], cached_coeff);
        }
        
        feedback_stateR = processedR;
        outR[i] = (inR[i] + processedR) * 0.5f;
    }
}
