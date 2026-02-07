#include "shimmer_reverb.h"
#include <string.h>
#include <math.h>

// Shimmer = basic hall-like reverb with an LFO-modulated delay path mixed in to add metallic shimmer
#define SR_NUM_COMBS 3
#define SR_NUM_AP 1

#define SR_COMB_MAX 4096
#define SR_AP_MAX 512

#define SAMPLE_RATE 48000.0f

__attribute__((section(".sdram"), aligned(32))) static float scomb_l[SR_NUM_COMBS][SR_COMB_MAX];
__attribute__((section(".sdram"), aligned(32))) static float scomb_r[SR_NUM_COMBS][SR_COMB_MAX];
static uint16_t scomb_idx_l[SR_NUM_COMBS];
static uint16_t scomb_idx_r[SR_NUM_COMBS];

__attribute__((section(".sdram"), aligned(32))) static float sap_l[SR_NUM_AP][SR_AP_MAX];
__attribute__((section(".sdram"), aligned(32)))  static float sap_r[SR_NUM_AP][SR_AP_MAX];
static uint16_t sap_idx_l[SR_NUM_AP];
static uint16_t sap_idx_r[SR_NUM_AP];

// modulation delay buffer (for shimmer path)
#define SHIM_DELAY_MAX 2400
__attribute__((section(".sdram"), aligned(32))) static float shim_buf_l[SHIM_DELAY_MAX];
__attribute__((section(".sdram"), aligned(32)))  static float shim_buf_r[SHIM_DELAY_MAX];
static uint16_t shim_head = 0;

static float shimmer_mix = 0.35f;
static float shimmer_time = 1.5f;
static float shimmer_depth = 0.025f; // in seconds
static float shimmer_rate = 0.25f; // Hz

static float comb_damp_l[SR_NUM_COMBS] = {0};
static float comb_damp_r[SR_NUM_COMBS] = {0};
static float lfo_phase = 0.0f;

// Pre-calculated sine LFO table to avoid expensive sinf() per sample
#define LFO_TABLE_SIZE 256
static float lfo_sin_table[LFO_TABLE_SIZE];
static uint8_t lfo_table_init = 0;

static void shimmer_init_lfo_table(void) {
    if (lfo_table_init) return;
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = 2.0f * 3.14159265f * (float)i / (float)LFO_TABLE_SIZE;
        lfo_sin_table[i] = (sinf(phase) + 1.0f) * 0.5f;  // Maps to 0..1
    }
    lfo_table_init = 1;
}

void ShimmerReverb_Init(void){
    memset(scomb_l,0,sizeof(scomb_l));
    memset(scomb_r,0,sizeof(scomb_r));
    memset(sap_l,0,sizeof(sap_l));
    memset(sap_r,0,sizeof(sap_r));
    memset(scomb_idx_l,0,sizeof(scomb_idx_l));
    memset(scomb_idx_r,0,sizeof(scomb_idx_r));
    memset(sap_idx_l,0,sizeof(sap_idx_l));
    memset(sap_idx_r,0,sizeof(sap_idx_r));
    memset(shim_buf_l,0,sizeof(shim_buf_l));
    memset(shim_buf_r,0,sizeof(shim_buf_r));
    shim_head = 0;
    memset(comb_damp_l,0,sizeof(comb_damp_l));
    memset(comb_damp_r,0,sizeof(comb_damp_r));
    lfo_phase = 0.0f;
    shimmer_init_lfo_table();  // Pre-calculate LFO table once
}

void ShimmerReverb_SetMix(float mix){ if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f; shimmer_mix = mix; }
void ShimmerReverb_SetTime(float time){ if (time < 0.01f) time = 0.01f; if (time > 10.0f) time = 10.0f; shimmer_time = time; }
void ShimmerReverb_SetDepth(float depth){ if (depth < 0.0f) depth = 0.0f; if (depth > 0.05f) depth = 0.05f; shimmer_depth = depth; }
void ShimmerReverb_SetRate(float rate){ if (rate < 0.01f) rate = 0.01f; if (rate > 5.0f) rate = 5.0f; shimmer_rate = rate; }

static inline float comb_proc(float in, float *buf, uint16_t size, uint16_t *idx, float fb, float damp, float *damp_state){
    float out = buf[*idx];
    *damp_state = (*damp_state * (1.0f - damp)) + (out * damp);
    buf[*idx] = in + (*damp_state * fb);
    *idx = (*idx + 1) % size;
    return out;
}

static inline float ap_proc(float in, float *buf, uint16_t size, uint16_t *idx, float fb){
    float buf_out = buf[*idx];
    buf[*idx] = in + buf_out * fb;
    float out = buf_out - buf[*idx] * fb;
    *idx = (*idx + 1) % size;
    return out;
}

static inline float lerp(float a, float b, float t){
    return a + (b - a) * t;
}

// Cubic hermite interpolation for smoother delay line reading (reduces modulation artifacts)
static inline float cubic_interp(float y0, float y1, float y2, float y3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    
    float c0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float c1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c2 = -0.5f * y0 + 0.5f * y2;
    float c3 = y1;
    
    return c0 * t3 + c1 * t2 + c2 * t + c3;
}

void ShimmerReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size){
    float fb = 0.55f + 0.4f * fminf(1.0f, shimmer_time/3.0f);
    fb *= expf(-3.0f/(shimmer_time*SAMPLE_RATE));
    float damp = 0.5f;

    // LFO table increment (fractional index into 256-entry table)
    float lfo_table_inc = (shimmer_rate / SAMPLE_RATE) * (float)LFO_TABLE_SIZE;
    float lfo_table_phase = lfo_phase * (float)LFO_TABLE_SIZE;

    for(uint16_t i=0;i<size;i++){
        float inl = inL[i];
        float inr = inR[i];

        float wetl=0, wetr=0;
        // combs (using original dynamic sizing: 1024, 1600, 2200, 2800)
        for(int c=0;c<SR_NUM_COMBS;c++){
            int sz = 1024 + c*600;  // Original formula: 1024, 1600, 2200, 2800
            wetl += comb_proc(inl, scomb_l[c], sz, &scomb_idx_l[c], fb, damp, &comb_damp_l[c]);
            wetr += comb_proc(inr, scomb_r[c], sz, &scomb_idx_r[c], fb, damp, &comb_damp_r[c]);
        }
        wetl /= SR_NUM_COMBS; wetr /= SR_NUM_COMBS;

        // allpass smoothing (using original dynamic sizing: 431, 371, 311, 251)
        for(int a=0;a<SR_NUM_AP;a++){
            int asz = 431 - a*60;  // Original formula: 431, 371
            wetl = ap_proc(wetl, sap_l[a], asz, &sap_idx_l[a], 0.7f);
            wetr = ap_proc(wetr, sap_r[a], asz, &sap_idx_r[a], 0.7f);
        }

        // write into shimmer delay buffer
        shim_buf_l[shim_head] = wetl;
        shim_buf_r[shim_head] = wetr;

        // Get LFO from pre-calculated table with smooth interpolation (not uint8_t)
        float lfo_frac = lfo_table_phase - (int)lfo_table_phase;  // Fractional part for smooth LFO
        int lfo_idx0 = (int)lfo_table_phase % LFO_TABLE_SIZE;
        int lfo_idx1 = (lfo_idx0 + 1) % LFO_TABLE_SIZE;
        float mod = lerp(lfo_sin_table[lfo_idx0], lfo_sin_table[lfo_idx1], lfo_frac);  // Smooth LFO
        
        // More moderate delay modulation (20-35ms instead of 20-45ms)
        float var_delay = (0.020f + mod * 0.015f) * SAMPLE_RATE; // samples: 20ms ± 7.5ms

        // Cubic interpolation for smooth delay line (reduces modulation artifacts)
        int rd_idx = (shim_head + SHIM_DELAY_MAX - (int)var_delay) % SHIM_DELAY_MAX;
        float frac = var_delay - (int)var_delay;
        
        // Get 4 samples for cubic interpolation
        int idx0 = (rd_idx + SHIM_DELAY_MAX - 1) % SHIM_DELAY_MAX;
        int idx1 = rd_idx;
        int idx2 = (rd_idx + 1) % SHIM_DELAY_MAX;
        int idx3 = (rd_idx + 2) % SHIM_DELAY_MAX;
        
        float mod_wetl = cubic_interp(shim_buf_l[idx0], shim_buf_l[idx1], shim_buf_l[idx2], shim_buf_l[idx3], frac);
        float mod_wetr = cubic_interp(shim_buf_r[idx0], shim_buf_r[idx1], shim_buf_r[idx2], shim_buf_r[idx3], frac);

        // increment lfo phase in table space
        lfo_table_phase += lfo_table_inc;
        if(lfo_table_phase >= (float)LFO_TABLE_SIZE) lfo_table_phase -= (float)LFO_TABLE_SIZE;

        shim_head = (shim_head + 1) % SHIM_DELAY_MAX;

        // mix original wet and modulated shimmer component (slightly reduced shimmer mix)
        float combined_l = 0.85f * wetl + 0.15f * mod_wetl;
        float combined_r = 0.85f * wetr + 0.15f * mod_wetr;

        // final mix
        outL[i] = (1.0f - shimmer_mix) * inl + shimmer_mix * combined_l;
        outR[i] = (1.0f - shimmer_mix) * inr + shimmer_mix * combined_r;
    }
    
    // Store phase back for next block
    lfo_phase = lfo_table_phase / (float)LFO_TABLE_SIZE;
    if(lfo_phase >= 1.0f) lfo_phase -= 1.0f;
}
