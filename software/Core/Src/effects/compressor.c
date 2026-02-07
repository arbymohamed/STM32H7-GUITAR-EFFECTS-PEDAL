#include "compressor.h"
#include <string.h>
#include <math.h>

// Compressor parameters (professional units)
static volatile float comp_threshold_db = -20.0f;  // -60.0 to 0.0 dB
static volatile float comp_ratio = 4.0f;            // 1.0 to 20.0
static volatile float comp_attack_ms = 5.0f;        // 0.1 to 100.0 ms
static volatile float comp_release_ms = 100.0f;     // 10.0 to 1000.0 ms
static volatile float comp_auto_makeup = 1.0f;      // 0.0 = off, 1.0 = on

// Sample rate for time calculations
static float sample_rate = 48000.0f;

// Attack/release coefficients (calculated from ms)
static float attack_coeff = 0.1f;
static float release_coeff = 0.001f;

// Envelope follower states (in dB)
static float envelope_L = -96.0f;
static float envelope_R = -96.0f;

// Smoothed gain reduction (linear, for final multiplication)
static float gain_smooth_L = 1.0f;
static float gain_smooth_R = 1.0f;

// Fast absolute value
static inline float fabs_inline(float x) {
    return x < 0.0f ? -x : x;
}

// Convert linear amplitude to dB
static inline float lin_to_db(float lin) {
    if (lin < 0.000001f) return -96.0f; // Floor at -96dB
    return 20.0f * log10f(lin);
}

// Convert dB to linear amplitude
static inline float db_to_lin(float db) {
    return powf(10.0f, db / 20.0f);
}

// Calculate time constant coefficient from milliseconds
static inline float ms_to_coeff(float ms, float sr) {
    // Time constant: samples needed to reach ~63% of target
    // coeff = 1 - exp(-1 / (time_in_seconds * sample_rate))
    if (ms < 0.001f) ms = 0.001f;
    float samples = (ms / 1000.0f) * sr;
    return 1.0f - expf(-1.0f / samples);
}

void Compressor_Init(float sample_rate_hz) {
    sample_rate = sample_rate_hz;
    envelope_L = -96.0f;
    envelope_R = -96.0f;
    gain_smooth_L = 1.0f;
    gain_smooth_R = 1.0f;

    // Calculate initial coefficients
    attack_coeff = ms_to_coeff(comp_attack_ms, sample_rate);
    release_coeff = ms_to_coeff(comp_release_ms, sample_rate);
}

void Compressor_SetThreshold(float threshold_db) {
    // Clamp to professional range
    if (threshold_db < -60.0f) threshold_db = -60.0f;
    if (threshold_db > 0.0f) threshold_db = 0.0f;
    comp_threshold_db = threshold_db;
    __DMB();  // Memory barrier
}

void Compressor_SetRatio(float ratio) {
    // Clamp to professional range (1:1 to 20:1)
    if (ratio < 1.0f) ratio = 1.0f;
    if (ratio > 20.0f) ratio = 20.0f;
    comp_ratio = ratio;
    __DMB();  // Memory barrier
}

void Compressor_SetAttack(float attack_ms) {
    // Clamp to professional range
    if (attack_ms < 0.1f) attack_ms = 0.1f;
    if (attack_ms > 100.0f) attack_ms = 100.0f;
    comp_attack_ms = attack_ms;
    attack_coeff = ms_to_coeff(comp_attack_ms, sample_rate);
    __DMB();  // Memory barrier
}

void Compressor_SetRelease(float release_ms) {
    // Clamp to professional range
    if (release_ms < 10.0f) release_ms = 10.0f;
    if (release_ms > 1000.0f) release_ms = 1000.0f;
    comp_release_ms = release_ms;
    release_coeff = ms_to_coeff(comp_release_ms, sample_rate);
    __DMB();  // Memory barrier
}

void Compressor_SetAutoMakeup(float enable) {
    comp_auto_makeup = (enable > 0.5f) ? 1.0f : 0.0f;
    __DMB();  // Memory barrier
}

void Compressor_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    // Smoothing for gain changes (prevents zipper noise)
    const float gain_smooth_coeff = 0.05f;

    // Calculate auto-makeup gain
    // Compensates for the level loss from compression at threshold
    float makeup_db = 0.0f;
    if (comp_auto_makeup > 0.5f) {
        // Makeup gain formula: gain_reduction at threshold = threshold_excess * (ratio - 1) / ratio
        // At threshold, excess is 0, so no reduction. But average signal is usually ABOVE threshold.
        // Simple approximation: makeup = typical excess at threshold * compression factor
        // For most compressors: makeup ≈ threshold / 2 (assumes average signal is 10-20dB above threshold)
        makeup_db = -comp_threshold_db * 0.6f; // Scale factor to match typical compression
        // Limit makeup to reasonable range
        if (makeup_db > 18.0f) makeup_db = 18.0f;
        if (makeup_db < 0.0f) makeup_db = 0.0f;
    }
    float makeup_gain = db_to_lin(makeup_db);
    
    for (uint16_t i = 0; i < size; i++) {
        // Get input levels
        float abs_L = fabs_inline(inL[i]);
        float abs_R = fabs_inline(inR[i]);

        // Convert to dB for envelope detection
        float input_db_L = lin_to_db(abs_L);
        float input_db_R = lin_to_db(abs_R);
        
        // Envelope follower with attack/release (in dB domain)
        // Attack when signal rises, release when it falls
        if (input_db_L > envelope_L) {
            envelope_L += attack_coeff * (input_db_L - envelope_L);
        } else {
            envelope_L += release_coeff * (input_db_L - envelope_L);
        }
        
        if (input_db_R > envelope_R) {
            envelope_R += attack_coeff * (input_db_R - envelope_R);
        } else {
            envelope_R += release_coeff * (input_db_R - envelope_R);
        }
        
        // Calculate gain reduction in dB domain (correct approach)
        float target_gain_db_L = 0.0f; // 0dB = no reduction
        float target_gain_db_R = 0.0f;
        
        // Left channel
        if (envelope_L > comp_threshold_db) {
            // How much is the signal above threshold?
            float excess_db = envelope_L - comp_threshold_db;
            // Apply ratio: reduce the excess by (ratio - 1) / ratio
            // Example: 4:1 ratio means reduce excess by 3/4
            float reduction_db = excess_db * (comp_ratio - 1.0f) / comp_ratio;
            target_gain_db_L = -reduction_db; // Negative because we're reducing
        }
        
        // Right channel
        if (envelope_R > comp_threshold_db) {
            float excess_db = envelope_R - comp_threshold_db;
            float reduction_db = excess_db * (comp_ratio - 1.0f) / comp_ratio;
            target_gain_db_R = -reduction_db;
        }
        
        // Convert to linear gain
        float target_gain_L = db_to_lin(target_gain_db_L);
        float target_gain_R = db_to_lin(target_gain_db_R);

        // Smooth the gain to avoid artifacts
        gain_smooth_L += gain_smooth_coeff * (target_gain_L - gain_smooth_L);
        gain_smooth_R += gain_smooth_coeff * (target_gain_R - gain_smooth_R);

        // Apply compression (with conservative makeup gain)
        outL[i] = inL[i] * gain_smooth_L * makeup_gain;
        outR[i] = inR[i] * gain_smooth_R * makeup_gain;
        
        // Fast soft clipping to prevent overshoot (avoiding expensive tanhf)
        float xL = outL[i] * 0.9f;
        float xR = outR[i] * 0.9f;
        // Fast tanh approximation: x(27 + x²) / (27 + 9x²)
        if (xL > 3.0f) xL = 1.0f; else if (xL < -3.0f) xL = -1.0f; else { float x2 = xL * xL; xL = xL * (27.0f + x2) / (27.0f + 9.0f * x2); }
        if (xR > 3.0f) xR = 1.0f; else if (xR < -3.0f) xR = -1.0f; else { float x2 = xR * xR; xR = xR * (27.0f + x2) / (27.0f + 9.0f * x2); }
        outL[i] = xL / 0.9f;
        outR[i] = xR / 0.9f;
    }
}
