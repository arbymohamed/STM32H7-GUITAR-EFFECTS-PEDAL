// Slider Parameter Conversion Functions
// Converts LVGL slider values (0-100) to effect parameters
// Replaces the old ADC conversion functions

#ifndef SLIDER_PARAM_CONVERSION_H
#define SLIDER_PARAM_CONVERSION_H

#include <stdint.h>

// Helper macro to normalize slider value (0-100) to 0.0-1.0
#define SLIDER_TO_FLOAT(val) ((val) / 100.0f)

// ============================================================================
// DELAY EFFECT
// ============================================================================
static inline float slider_to_delay_time(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val) * 800.0f;  // 0 - 800 ms
}

static inline float slider_to_delay_feedback(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_delay_tone(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 (dark) - 1.0 (bright)
}

static inline float slider_to_delay_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

// ============================================================================
// OVERDRIVE EFFECT
// ============================================================================
static inline float slider_to_overdrive_gain(int32_t slider_val) {
    return 1.0f + SLIDER_TO_FLOAT(slider_val) * 9.0f;  // 1.0 - 10.0
}

static inline float slider_to_overdrive_level(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_overdrive_tone(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_overdrive_damping(int32_t slider_val) {
    return 0.3f + SLIDER_TO_FLOAT(slider_val) * 0.7f;  // 0.3 - 1.0
}

// ============================================================================
// CHORUS EFFECT
// ============================================================================
static inline float slider_to_chorus_rate(int32_t slider_val) {
    return 0.05f + SLIDER_TO_FLOAT(slider_val) * 4.95f;  // 0.05 - 5.0 Hz
}

static inline float slider_to_chorus_depth(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_chorus_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

// ============================================================================
// TREMOLO EFFECT
// ============================================================================
static inline float slider_to_tremolo_rate(int32_t slider_val) {
    return 0.1f + SLIDER_TO_FLOAT(slider_val) * 19.9f;  // 0.1 - 20.0 Hz
}

static inline float slider_to_tremolo_depth(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_tremolo_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

// ============================================================================
// FLANGER EFFECT
// ============================================================================
static inline float slider_to_flanger_rate(int32_t slider_val) {
    return 0.05f + SLIDER_TO_FLOAT(slider_val) * 4.95f;  // 0.05 - 5.0 Hz
}

static inline float slider_to_flanger_depth(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_flanger_feedback(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_flanger_delay(int32_t slider_val) {
    return 0.1f + SLIDER_TO_FLOAT(slider_val) * 6.9f;  // 0.1 - 7.0 ms
}

// ============================================================================
// PHASER EFFECT
// ============================================================================
static inline float slider_to_phaser_rate(int32_t slider_val) {
    return 0.05f + SLIDER_TO_FLOAT(slider_val) * 4.95f;  // 0.05 - 5.0 Hz
}

static inline float slider_to_phaser_depth(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_phaser_feedback(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_phaser_freq(int32_t slider_val) {
    return 200.0f + SLIDER_TO_FLOAT(slider_val) * 3800.0f;  // 200 - 4000 Hz
}

// ============================================================================
// DISTORTION EFFECT
// ============================================================================
static inline float slider_to_distortion_gain(int32_t slider_val) {
    return 1.0f + SLIDER_TO_FLOAT(slider_val) * 19.0f;  // 1.0 - 20.0
}

static inline float slider_to_distortion_level(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_distortion_tone(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

// ============================================================================
// AUTO-WAH EFFECT
// ============================================================================
static inline float slider_to_autowah_wah(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0 (resonance/Q)
}

static inline float slider_to_autowah_level(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0 (sensitivity)
}

static inline float slider_to_autowah_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_autowah_wet_boost(int32_t slider_val) {
    return 0.5f + SLIDER_TO_FLOAT(slider_val) * 2.5f;  // 0.5 - 3.0
}

// ============================================================================
// COMPRESSOR EFFECT
// ============================================================================
static inline float slider_to_compressor_amount(int32_t slider_val) {
    // Intuitive: 0=gentle (no compression), 100=maximum (full compression)
    // Maps: slider 0 → 0dB (no compression), slider 100 → -60dB (maximum compression)
    return 0.0f - SLIDER_TO_FLOAT(slider_val) * 60.0f;  // 0.0 to -60.0 dB
}

static inline float slider_to_compressor_ratio(int32_t slider_val) {
    return 1.0f + SLIDER_TO_FLOAT(slider_val) * 19.0f;  // 1.0 - 20.0
}

static inline float slider_to_compressor_attack(int32_t slider_val) {
    // Logarithmic mapping for better control at low values
    float normalized = SLIDER_TO_FLOAT(slider_val);
    return 0.1f + (normalized * normalized) * 99.9f;  // 0.1 - 100.0 ms (log curve)
}

static inline float slider_to_compressor_release(int32_t slider_val) {
    // Logarithmic mapping for better control
    float normalized = SLIDER_TO_FLOAT(slider_val);
    return 10.0f + (normalized * normalized) * 990.0f;  // 10.0 - 1000.0 ms (log curve)
}

static inline float slider_to_compressor_automakeup(int32_t switch_state) {
    return (switch_state > 0) ? 1.0f : 0.0f;  // 1.0 = ON, 0.0 = OFF
}

// ============================================================================
// HALL REVERB
// ============================================================================
static inline float slider_to_hall_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_hall_time(int32_t slider_val) {
    return 0.01f + SLIDER_TO_FLOAT(slider_val) * 9.99f;  // 0.01 - 10.0 s
}

static inline float slider_to_hall_damping(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_hall_predelay_ms(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val) * 100.0f;  // 0 - 100 ms
}

// ============================================================================
// PLATE REVERB
// ============================================================================
static inline float slider_to_plate_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_plate_time(int32_t slider_val) {
    return 0.01f + SLIDER_TO_FLOAT(slider_val) * 9.99f;  // 0.01 - 10.0 s
}

static inline float slider_to_plate_damping(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_plate_tone(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 (dark) - 1.0 (bright)
}

// ============================================================================
// SPRING REVERB
// ============================================================================
static inline float slider_to_spring_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_spring_tension(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_spring_decay(int32_t slider_val) {
    return 0.01f + SLIDER_TO_FLOAT(slider_val) * 4.99f;  // 0.01 - 5.0 s
}

static inline float slider_to_spring_tone(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

// ============================================================================
// SHIMMER REVERB
// ============================================================================
static inline float slider_to_shimmer_mix(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_shimmer_time(int32_t slider_val) {
    return 0.01f + SLIDER_TO_FLOAT(slider_val) * 9.99f;  // 0.01 - 10.0 s
}

static inline float slider_to_shimmer_depth(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val) * 0.05f;  // 0.0 - 0.05 s (max mod depth)
}

static inline float slider_to_shimmer_rate(int32_t slider_val) {
    return 0.01f + SLIDER_TO_FLOAT(slider_val) * 4.99f;  // 0.01 - 5.0 Hz
}

// ============================================================================
// AMP SIMULATION
// ============================================================================
static inline float slider_to_amp_gain(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_amp_bass(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0 (0.5 = neutral)
}

static inline float slider_to_amp_mid(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0 (0.5 = neutral)
}

static inline float slider_to_amp_treble(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0 (0.5 = neutral)
}

static inline float slider_to_amp_presence(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

static inline float slider_to_amp_master(int32_t slider_val) {
    return SLIDER_TO_FLOAT(slider_val);  // 0.0 - 1.0
}

#endif // SLIDER_PARAM_CONVERSION_H
