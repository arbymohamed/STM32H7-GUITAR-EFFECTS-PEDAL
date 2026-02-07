#include "flanger.h"
#include <string.h>
#include <math.h>

#define FLANGER_MAX_SAMPLES 960 // 20ms at 48kHz
__attribute__((section(".adcarray"), aligned(32))) static float flanger_buffer_left[FLANGER_MAX_SAMPLES];
__attribute__((section(".adcarray"), aligned(32))) static float flanger_buffer_right[FLANGER_MAX_SAMPLES];
static uint16_t flanger_index = 0;

static float flanger_rate = 0.3f;      // Hz
static float flanger_depth = 0.9f;     // 0.0 to 1.0
static float flanger_feedback = 0.2f;  // 0.0 to 1.0
static float flanger_delay_ms = 4.0f;  // ms
static float flanger_phase = 0.0f;
static float flanger_sample_rate = 48000.0f;

// Thiran allpass interpolator state (for smooth cubic Hermite interpolation)
static float thiran_prev_y_L = 0.0f;
static float thiran_prev_y_R = 0.0f;

// Fast clamp function
static inline float fclamp_flanger(float x, float min, float max) {
    return x < min ? min : (x > max ? max : x);
}

// Thiran allpass interpolation (cubic Hermite quality interpolation)
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

void Flanger_Init(void) {
    memset(flanger_buffer_left, 0, sizeof(flanger_buffer_left));
    memset(flanger_buffer_right, 0, sizeof(flanger_buffer_right));
    flanger_index = 0;
    flanger_phase = 0.0f;
    thiran_prev_y_L = 0.0f;
    thiran_prev_y_R = 0.0f;
}

void Flanger_SetRate(float rate) {
    if (rate < 0.05f) rate = 0.05f;
    if (rate > 5.0f) rate = 5.0f;
    flanger_rate = rate;
}

void Flanger_SetDepth(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    flanger_depth = depth;
}

void Flanger_SetFeedback(float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    flanger_feedback = feedback * 0.97f; // Clip to prevent runaway
}

void Flanger_SetDelay(float delay_ms) {
    if (delay_ms < 0.1f) delay_ms = 0.1f;
    if (delay_ms > 7.0f) delay_ms = 7.0f;
    flanger_delay_ms = delay_ms;
}

void Flanger_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    // Base delay in samples
    float base_delay = flanger_delay_ms * 0.001f * flanger_sample_rate;
    float lfo_amp = flanger_depth * base_delay;
    float lfo_inc = 4.0f * flanger_rate / flanger_sample_rate;
    
    for (uint16_t i = 0; i < size; i++) {
        // Triangle LFO (0..1)
        flanger_phase += lfo_inc;
        if (flanger_phase > 1.0f) flanger_phase -= 1.0f;
        float lfo = (flanger_phase < 0.5f) ? (2.0f * flanger_phase) : (2.0f - 2.0f * flanger_phase);
        
        // Modulated delay
        float mod_delay = base_delay + lfo * lfo_amp;
        int int_delay = (int)floorf(mod_delay);
        float frac = mod_delay - int_delay;
        
        // Read indices with cubic Hermite interpolation (UPGRADE: was linear)
        int read_index = flanger_index - int_delay - 1;
        if (read_index < 0) read_index += FLANGER_MAX_SAMPLES;
        int read_index_next = (read_index + 1) % FLANGER_MAX_SAMPLES;
        
        // Left channel - use Thiran allpass interpolation for smooth delay reads
        float x0L = flanger_buffer_left[read_index];
        float x1L = flanger_buffer_left[read_index_next];
        float delayedL = thiran_allpass_interp(x0L, x1L, frac, &thiran_prev_y_L);
        
        flanger_buffer_left[flanger_index] = inL[i] + delayedL * flanger_feedback;
        outL[i] = (inL[i] + delayedL) * 0.5f; // Equal mix
        
        // Right channel
        float x0R = flanger_buffer_right[read_index];
        float x1R = flanger_buffer_right[read_index_next];
        float delayedR = thiran_allpass_interp(x0R, x1R, frac, &thiran_prev_y_R);
        
        flanger_buffer_right[flanger_index] = inR[i] + delayedR * flanger_feedback;
        outR[i] = (inR[i] + delayedR) * 0.5f;
        
        // Advance buffer
        flanger_index++;
        if (flanger_index >= FLANGER_MAX_SAMPLES) flanger_index = 0;
    }
}
