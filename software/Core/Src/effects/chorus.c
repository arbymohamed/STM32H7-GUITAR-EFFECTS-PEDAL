#include "chorus.h"
#include <string.h>
#include <math.h>

#define CHORUS_MAX_SAMPLES 1200 // ~25ms at 48kHz

__attribute__((section(".adcarray"), aligned(32))) static float chorus_delay_left[CHORUS_MAX_SAMPLES];
__attribute__((section(".adcarray"), aligned(32))) static float chorus_delay_right[CHORUS_MAX_SAMPLES];
static uint16_t chorus_index = 0;
static float chorus_rate = 0.3f;    // Hz
static float chorus_depth = 0.5f;   // 0.0 to 1.0
static float chorus_mix = 0.5f;     // 0.0 to 1.0
static float chorus_delay_ms = 12.0f; // ms
static float chorus_feedback = 0.2f; // 0.0 to 1.0
static float chorus_phase = 0.0f;
static float chorus_phase_r = 0.5f; // stereo offset
static float chorus_sample_rate = 48000.0f;
static float thiran_prev_y_L = 0.0f;
static float thiran_prev_y_R = 0.0f;


void Chorus_Init(void) {
    memset(chorus_delay_left, 0, sizeof(chorus_delay_left));
    memset(chorus_delay_right, 0, sizeof(chorus_delay_right));
    chorus_index = 0;
    chorus_phase = 0.0f;
    chorus_phase_r = 0.5f;
}

void Chorus_SetRate(float rate) {
    if (rate < 0.05f) rate = 0.05f;
    if (rate > 5.0f) rate = 5.0f;
    chorus_rate = rate;
}

void Chorus_SetDepth(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    chorus_depth = depth;
}

void Chorus_SetMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    chorus_mix = mix;
}

void Chorus_SetDelay(float delay_ms) {
    if (delay_ms < 1.0f) delay_ms = 1.0f;
    if (delay_ms > 24.0f) delay_ms = 24.0f;
    chorus_delay_ms = delay_ms;
}

void Chorus_SetFeedback(float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    chorus_feedback = feedback;
}

void Chorus_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    int base_delay = (int)(chorus_delay_ms * 0.001f * chorus_sample_rate);
    int mod_depth = (int)(chorus_depth * base_delay);
    float lfo_inc = chorus_rate / chorus_sample_rate;
    
    for (int i = 0; i < size; i++) {
        // --- Left channel ---
        // Triangle LFO (0..1)
        chorus_phase += lfo_inc;
        if (chorus_phase > 1.0f) chorus_phase -= 1.0f;
        float lfoL = (chorus_phase < 0.5f) ? (2.0f * chorus_phase) : (2.0f - 2.0f * chorus_phase);

        // Modulated delay (fractional)
        float mod_delay_L = base_delay + mod_depth * lfoL;
        int int_delay_L   = (int)floorf(mod_delay_L);
        float frac_L      = mod_delay_L - int_delay_L;

        // Read indices
        int read_index_L = chorus_index - int_delay_L - 1;
        if (read_index_L < 0) read_index_L += CHORUS_MAX_SAMPLES;
        int read_index_next_L = (read_index_L + 1) % CHORUS_MAX_SAMPLES;

        // Fetch from delay buffer
        float x0L = chorus_delay_left[read_index_L];
        float x1L = chorus_delay_left[read_index_next_L];

        // Interpolated delayed sample
        float delayedL = thiran_allpass_interp(x0L, x1L, frac_L, &thiran_prev_y_L);

        // Write input + feedback
        chorus_delay_left[chorus_index] = inL[i] + delayedL * chorus_feedback;

        // Mix
        outL[i] = (1.0f - chorus_mix) * inL[i] + chorus_mix * delayedL;


        // --- Right channel (stereo with phase offset) ---
        float phase_R = chorus_phase + 0.5f;
        if (phase_R > 1.0f) phase_R -= 1.0f;
        float lfoR = (phase_R < 0.5f) ? (2.0f * phase_R) : (2.0f - 2.0f * phase_R);

        float mod_delay_R = base_delay + mod_depth * lfoR;
        int int_delay_R   = (int)floorf(mod_delay_R);
        float frac_R      = mod_delay_R - int_delay_R;

        int read_index_R = chorus_index - int_delay_R - 1;
        if (read_index_R < 0) read_index_R += CHORUS_MAX_SAMPLES;
        int read_index_next_R = (read_index_R + 1) % CHORUS_MAX_SAMPLES;

        float x0R = chorus_delay_right[read_index_R];
        float x1R = chorus_delay_right[read_index_next_R];

        float delayedR = thiran_allpass_interp(x0R, x1R, frac_R, &thiran_prev_y_R);

        chorus_delay_right[chorus_index] = inR[i] + delayedR * chorus_feedback;

        outR[i] = (1.0f - chorus_mix) * inR[i] + chorus_mix * delayedR;


        // --- Advance delay buffer index ---
        chorus_index++;
        if (chorus_index >= CHORUS_MAX_SAMPLES) chorus_index = 0;
    }
}
