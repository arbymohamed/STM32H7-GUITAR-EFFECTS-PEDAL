#include "hall_reverb.h"
#include <string.h>
#include <math.h>

#define HR_NUM_COMBS   3
#define HR_NUM_ALLPASS 1

// Optimized comb sizes for better performance (3 instead of 4)
#define HR_COMB_1 557
#define HR_COMB_2 677
#define HR_COMB_3 797
#define HR_COMB_MAX HR_COMB_3

// Optimized allpass (1 instead of 2 for efficiency)
#define HR_AP_1 341
#define HR_AP_MAX HR_AP_1

__attribute__((section(".sdram"), aligned(32))) static float comb_buf_l[HR_NUM_COMBS][HR_COMB_MAX];
__attribute__((section(".sdram"), aligned(32))) static float comb_buf_r[HR_NUM_COMBS][HR_COMB_MAX];
static uint16_t comb_idx_l[HR_NUM_COMBS];
static uint16_t comb_idx_r[HR_NUM_COMBS];

__attribute__((section(".sdram"), aligned(32))) static float ap_buf_l[HR_NUM_ALLPASS][HR_AP_MAX];
__attribute__((section(".sdram"), aligned(32))) static float ap_buf_r[HR_NUM_ALLPASS][HR_AP_MAX];
static uint16_t ap_idx_l[HR_NUM_ALLPASS];
static uint16_t ap_idx_r[HR_NUM_ALLPASS];

static const uint16_t hr_comb_sizes[HR_NUM_COMBS] = { HR_COMB_1, HR_COMB_2, HR_COMB_3 };
static const uint16_t hr_ap_sizes[HR_NUM_ALLPASS] = { HR_AP_1 };

// pre-delay buffer (reduced to 50ms max to save RAM)
#define HR_PREBUF_SAMPLES 2400  // 50ms at 48kHz
__attribute__((section(".sdram"), aligned(32))) static float prebuf_l[HR_PREBUF_SAMPLES];
__attribute__((section(".sdram"), aligned(32))) static float prebuf_r[HR_PREBUF_SAMPLES];
static uint16_t pre_write_idx = 0;
static uint16_t pre_len = 0;
// <CHANGE> Added fill counter to track valid samples in buffer
static uint16_t pre_fill_count = 0;

// parameters
static float hr_mix = 0.4f;
static float hr_time = 1.2f; // seconds
static float hr_damping = 0.5f;
static float hr_predelay_ms = 0.0f;

void HallReverb_Init(void) {
    memset(comb_buf_l, 0, sizeof(comb_buf_l));
    memset(comb_buf_r, 0, sizeof(comb_buf_r));
    memset(ap_buf_l, 0, sizeof(ap_buf_l));
    memset(ap_buf_r, 0, sizeof(ap_buf_r));
    memset(comb_idx_l, 0, sizeof(comb_idx_l));
    memset(comb_idx_r, 0, sizeof(comb_idx_r));
    memset(ap_idx_l, 0, sizeof(ap_idx_l));
    memset(ap_idx_r, 0, sizeof(ap_idx_r));
    memset(prebuf_l, 0, sizeof(prebuf_l));
    memset(prebuf_r, 0, sizeof(prebuf_r));
    pre_write_idx = 0;
    pre_len = 0;
    pre_fill_count = 0;
}

void HallReverb_SetMix(float mix) { if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f; hr_mix = mix; }
void HallReverb_SetTime(float time) { if (time < 0.01f) time = 0.01f; if (time > 10.0f) time = 10.0f; hr_time = time; }
void HallReverb_SetDamping(float damp) { if (damp < 0.0f) damp = 0.0f; if (damp > 1.0f) damp = 1.0f; hr_damping = damp; }

// <CHANGE> Fixed predelay setter to properly clear buffer and reset fill state
void HallReverb_SetPreDelayMs(float ms) {
    if (ms < 0.0f) ms = 0.0f;
    if (ms > 50.0f) ms = 50.0f;
    hr_predelay_ms = ms;
    pre_len = (uint16_t)(ms * 48.0f);
    if (pre_len >= HR_PREBUF_SAMPLES) pre_len = HR_PREBUF_SAMPLES-1;

    // Clear the buffer to prevent reading stale data
    memset(prebuf_l, 0, sizeof(prebuf_l));
    memset(prebuf_r, 0, sizeof(prebuf_r));
    pre_write_idx = 0;
    pre_fill_count = 0;
}

static inline float comb_process(float in, float *buf, uint16_t size, uint16_t *idx, float feedback, float damp, float *damp_state) {
    float out = buf[*idx];
    *damp_state = (*damp_state * (1.0f - damp)) + (out * damp);
    buf[*idx] = in + (*damp_state * feedback);
    *idx = (*idx + 1) % size;
    return out;
}

static inline float allpass_process(float in, float *buf, uint16_t size, uint16_t *idx, float feedback) {
    float buf_out = buf[*idx];
    buf[*idx] = in + buf_out * feedback;
    float out = buf_out - buf[*idx] * feedback;
    *idx = (*idx + 1) % size;
    return out;
}

void HallReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    float base_fb = 0.6f + 0.35f * fminf(1.0f, hr_time/3.0f);
    float fb_scale = expf(-3.0f / (hr_time * 48000.0f));
    float feedback = base_fb * fb_scale;
    // Safety clamp to prevent unstable feedback that can hang/blow up the system
    if (feedback > 0.995f) feedback = 0.995f;
    if (feedback < 0.0f) feedback = 0.0f;
    float damp = hr_damping;

    static float comb_damp_l[HR_NUM_COMBS] = {0};
    static float comb_damp_r[HR_NUM_COMBS] = {0};

    for (uint16_t i=0;i<size;i++) {
        float inputL = inL[i];
        float inputR = inR[i];

        // <CHANGE> Fixed pre-delay buffer to properly track fill state and avoid reading garbage
        float feedL = inputL;
        float feedR = inputR;

        if (pre_len > 0) {
            // Write current sample to buffer
            prebuf_l[pre_write_idx] = inputL;
            prebuf_r[pre_write_idx] = inputR;

            // Only read delayed samples once buffer has enough data
            if (pre_fill_count >= pre_len) {
                // Calculate read position (write_idx - pre_len, wrapped)
                uint16_t read_idx = (pre_write_idx + HR_PREBUF_SAMPLES - pre_len) % HR_PREBUF_SAMPLES;
                feedL = prebuf_l[read_idx];
                feedR = prebuf_r[read_idx];
            } else {
                // Buffer still filling - use dry signal to avoid glitches
                feedL = inputL;
                feedR = inputR;
                pre_fill_count++;
            }

            // Advance write pointer
            pre_write_idx = (pre_write_idx + 1) % HR_PREBUF_SAMPLES;
        } else {
            // No predelay - reset fill counter
            pre_fill_count = 0;
        }

        float wetL = 0.0f, wetR = 0.0f;
        // combs
        for (int c=0;c<HR_NUM_COMBS;c++) {
            uint16_t sz = hr_comb_sizes[c];
            wetL += comb_process(feedL, comb_buf_l[c], sz, &comb_idx_l[c], feedback, damp, &comb_damp_l[c]);
            wetR += comb_process(feedR, comb_buf_r[c], sz, &comb_idx_r[c], feedback, damp, &comb_damp_r[c]);
        }
        wetL /= HR_NUM_COMBS;
        wetR /= HR_NUM_COMBS;

        // allpass
        for (int ap=0;ap<HR_NUM_ALLPASS;ap++) {
            wetL = allpass_process(wetL, ap_buf_l[ap], hr_ap_sizes[ap], &ap_idx_l[ap], 0.7f);
            wetR = allpass_process(wetR, ap_buf_r[ap], hr_ap_sizes[ap], &ap_idx_r[ap], 0.7f);
        }

        // Add a small immediate component so a fully-wet mix still has audible input.
        // Use the instantaneous input (inputL/inputR) rather than the pre-delayed feed
        // so that PreDelay doesn't cause the wet-only path to be silent while the
        // pre-delay buffer is filling.
        // Ratio chosen empirically: 35% immediate + 65% reverb tail
        wetL = 0.35f * inputL + 0.65f * wetL;
        wetR = 0.35f * inputR + 0.65f * wetR;

        outL[i] = (1.0f - hr_mix) * inputL + hr_mix * wetL;
        outR[i] = (1.0f - hr_mix) * inputR + hr_mix * wetR;
    }
}
