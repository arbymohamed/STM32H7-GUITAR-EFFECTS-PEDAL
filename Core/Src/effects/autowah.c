#include "autowah.h"
#include <math.h>

static float autowah_wah = 0.5f;       // 0.0 to 1.0
static float autowah_level = 0.5f;     // 0.0 to 1.0
static float autowah_mix = 0.7f;       // 0.0 to 1.0
static float autowah_wet_boost = 1.75f; // 0.5 to 3.0
static float sample_rate = 48000.0f;

// Envelope follower states (in linear domain, faster)
static float env_L = 0.0f;
static float env_R = 0.0f;

// Bandpass filter states (stereo)
static float bp_y1_L = 0.0f, bp_y2_L = 0.0f;
static float bp_y1_R = 0.0f, bp_y2_R = 0.0f;

// Cached filter coefficients for current frequency
static float last_freq = 500.0f;
static float cached_b0 = 0.0f;
static float cached_a1 = 0.0f;
static float cached_a2 = 0.0f;

// Pre-calculate bandpass filter coefficients
static void update_bp_coeffs(float freq, float Q) {
    if (fabsf(freq - last_freq) < 1.0f) return; // Skip if frequency hasn't changed much
    last_freq = freq;
    
    float omega = 2.0f * 3.14159265f * freq / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * Q);
    
    float a0 = 1.0f + alpha;
    cached_b0 = alpha / a0;
    cached_a1 = -2.0f * cos_omega / a0;
    cached_a2 = (1.0f - alpha) / a0;
}

// Fast inline bandpass filter
static inline float bandpass_fast(float in, float *y1, float *y2) {
    float out = cached_b0 * in - cached_a1 * (*y1) - cached_a2 * (*y2);
    *y2 = *y1;
    *y1 = out;
    return out;
}

void AutoWah_Init(void) {
    env_L = env_R = 0.0f;
    bp_y1_L = bp_y2_L = bp_y1_R = bp_y2_R = 0.0f;
}

void AutoWah_SetWah(float wah) {
    autowah_wah = (wah < 0.0f) ? 0.0f : (wah > 1.0f) ? 1.0f : wah;
}

void AutoWah_SetLevel(float level) {
    autowah_level = (level < 0.0f) ? 0.0f : (level > 1.0f) ? 1.0f : level;
}

void AutoWah_SetMix(float mix) {
    autowah_mix = (mix < 0.0f) ? 0.0f : (mix > 1.0f) ? 1.0f : mix;
}

void AutoWah_SetWetBoost(float boost) {
    autowah_wet_boost = (boost < 0.5f) ? 0.5f : (boost > 3.0f) ? 3.0f : boost;
}

void AutoWah_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    float min_freq = 300.0f;
    float max_freq = 2500.0f;
    float Q = 5.0f + autowah_wah * 10.0f;  // Q: 5 to 15
    
    // Fast envelope follower coefficients
    const float attack = 0.02f;
    const float release = 0.001f;
    
    for (uint16_t i = 0; i < size; i++) {
        // Envelope followers (simplified, using direct sample)
        float abs_L = fabsf(inL[i]);
        float abs_R = fabsf(inR[i]);
        
        if (abs_L > env_L) {
            env_L += attack * (abs_L - env_L);
        } else {
            env_L += release * (abs_L - env_L);
        }
        
        if (abs_R > env_R) {
            env_R += attack * (abs_R - env_R);
        } else {
            env_R += release * (abs_R - env_R);
        }
        
        // Map envelope (0..1) to frequency (300..2500 Hz)
        float freq_L = min_freq + (env_L * autowah_level * 3.0f) * (max_freq - min_freq);
        float freq_R = min_freq + (env_R * autowah_level * 3.0f) * (max_freq - min_freq);
        
        // Clamp frequencies
        freq_L = (freq_L < min_freq) ? min_freq : (freq_L > max_freq) ? max_freq : freq_L;
        freq_R = (freq_R < min_freq) ? min_freq : (freq_R > max_freq) ? max_freq : freq_R;
        
        // Update filter coefficients only when frequency changes significantly
        if (i == 0 || fabsf(freq_L - last_freq) > 5.0f) {
            update_bp_coeffs(freq_L, Q);
        }
        
        // Apply bandpass filter
        float processedL = bandpass_fast(inL[i], &bp_y1_L, &bp_y2_L);
        float processedR = bandpass_fast(inR[i], &bp_y1_R, &bp_y2_R);
        
        // Mix dry/wet with wet boost
        outL[i] = inL[i] * (1.0f - autowah_mix) + processedL * autowah_mix * autowah_wet_boost;
        outR[i] = inR[i] * (1.0f - autowah_mix) + processedR * autowah_mix * autowah_wet_boost;
    }
}
