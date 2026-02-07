#include "delay.h"
#include <string.h>

// Practical delay buffer size (800ms is perfect for guitar)
// Allocate to SDRAM to avoid RAM overflow
#define DELAY_MAX_SAMPLES 38400  // 800ms at 48kHz (sweet spot for guitar delays)
#define SAMPLE_RATE 48000.0f

__attribute__((section(".sdram"), aligned(32))) static float delay_buffer_left[DELAY_MAX_SAMPLES] ;
__attribute__((section(".sdram"), aligned(32))) static float delay_buffer_right[DELAY_MAX_SAMPLES] ;

static uint32_t delay_write_index = 0;
static volatile float delay_time = 0.5f;      // 0.0 to 1.0 (fraction of max delay)
static float delay_time_smoothed = 0.5f;  // Smoothed version for actual processing
static volatile float delay_feedback = 0.5f;  // 0.0 to 0.98 (safety limited)
static volatile float delay_mix = 0.3f;       // 0.0 to 1.0

// Lowpass filter state for feedback path (darkens repeats like analog delays)
typedef struct {
    float z1;  // Previous output
} OnePole_LPF;

static OnePole_LPF lpf_left = {0.0f};
static OnePole_LPF lpf_right = {0.0f};
static float lpf_cutoff = 0.7f;  // 0.0 (dark) to 1.0 (bright)

// Simple one-pole lowpass filter (for darkening feedback repeats)
static inline float OnePole_Process(OnePole_LPF* lpf, float input, float coeff) {
    lpf->z1 = lpf->z1 + coeff * (input - lpf->z1);
    return lpf->z1;
}

// Linear interpolation between two samples
static inline float Lerp(float a, float b, float frac) {
    return a + (b - a) * frac;
}

void Delay_Init(void) {
    memset(delay_buffer_left, 0, sizeof(delay_buffer_left));
    memset(delay_buffer_right, 0, sizeof(delay_buffer_right));
    delay_write_index = 0;
    lpf_left.z1 = 0.0f;
    lpf_right.z1 = 0.0f;
    delay_time_smoothed = delay_time;
}

void Delay_SetTime(float time_ms) {
    // time_ms: 0 to 800 ms (perfect for guitar delays)
    if (time_ms < 0.0f) time_ms = 0.0f;
    if (time_ms > 800.0f) time_ms = 800.0f;
    delay_time = time_ms / 800.0f;
    __DMB();  // Memory barrier
}
void Delay_SetFeedback(float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    // Safety limit to prevent runaway feedback explosion
    if (feedback > 0.98f) feedback = 0.98f;
    delay_feedback = feedback;
    __DMB();  // Memory barrier
}

void Delay_SetMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    delay_mix = mix;
    __DMB();  // Memory barrier
}

void Delay_SetTone(float tone) {
    // tone: 0.0 (very dark) to 1.0 (very bright)
    if (tone < 0.0f) tone = 0.0f;
    if (tone > 1.0f) tone = 1.0f;
    lpf_cutoff = tone;
    __DMB();  // Memory barrier to ensure parameter is written before audio ISR reads
}

void Delay_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size)
{
    // This gradually transitions to the new delay time instead of jumping immediately
    float smoothing_coeff = 0.001f;  // Lower = smoother (0.001 = ~20ms transition time)

    // LPF coefficient (tone control): lower = darker repeats
    float lpf_coeff = 0.05f + lpf_cutoff * 0.85f;  // Maps 0-1 to 0.05-0.90

    // Simple linear crossfade (cheaper than equal-power, barely audible difference)
    float wet_gain = delay_mix;
    float dry_gain = 1.0f - delay_mix;

    for (uint16_t i = 0; i < size; i++) {
        delay_time_smoothed += smoothing_coeff * (delay_time - delay_time_smoothed);

        // Calculate delay in samples (fractional for smooth interpolation)
        float delay_samples_float = delay_time_smoothed * (DELAY_MAX_SAMPLES - 2);  // -2 for safety

        // Calculate read position with fractional part for interpolation
        float read_pos_float = (float)delay_write_index - delay_samples_float;
        if (read_pos_float < 0.0f) read_pos_float += DELAY_MAX_SAMPLES;

        // Integer and fractional parts
        uint32_t read_index = (uint32_t)read_pos_float;
        float frac = read_pos_float - (float)read_index;

        // Read two adjacent samples for linear interpolation (optimized wrap)
        uint32_t read_index_next = read_index + 1;
        if (read_index_next >= DELAY_MAX_SAMPLES) read_index_next = 0;

        // Linear interpolation (smooth delay reads, eliminates zipper noise)
        float delayedL = Lerp(delay_buffer_left[read_index],
                             delay_buffer_left[read_index_next],
                             frac);
        float delayedR = Lerp(delay_buffer_right[read_index],
                             delay_buffer_right[read_index_next],
                             frac);

        // Apply lowpass filter to feedback path (darkens repeats like analog delay)
        float filteredL = OnePole_Process(&lpf_left, delayedL, lpf_coeff);
        float filteredR = OnePole_Process(&lpf_right, delayedR, lpf_coeff);

        // Write to delay buffer with feedback (safety limited in SetFeedback)
        delay_buffer_left[delay_write_index] = inL[i] + filteredL * delay_feedback;
        delay_buffer_right[delay_write_index] = inR[i] + filteredR * delay_feedback;

        // Simple linear mix (cheaper CPU, slightly quieter at 50% but acceptable)
        outL[i] = dry_gain * inL[i] + wet_gain * delayedL;
        outR[i] = dry_gain * inR[i] + wet_gain * delayedR;

        // Advance write pointer
        delay_write_index++;
        if (delay_write_index >= DELAY_MAX_SAMPLES) delay_write_index = 0;
    }
}
