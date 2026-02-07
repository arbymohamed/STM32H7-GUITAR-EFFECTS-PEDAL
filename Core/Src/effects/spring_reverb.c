#include "spring_reverb.h"
#include <string.h>
#include <math.h>

// Simple spring-style emulation using a short chained allpass network with feedback and a bandpass-ish tone
#define SR_ALLP 5
#define SR_AP_SIZE 1024

__attribute__((section(".sdram"), aligned(32))) static float sr_ap_l[SR_ALLP][SR_AP_SIZE];
__attribute__((section(".sdram"), aligned(32))) static float sr_ap_r[SR_ALLP][SR_AP_SIZE];
static uint16_t sr_ap_idx_l[SR_ALLP];
static uint16_t sr_ap_idx_r[SR_ALLP];

static float sr_mix = 0.5f;
static float sr_tension = 0.6f;
static float sr_decay = 0.8f; // seconds
static float sr_tone = 0.5f;

// LP filter state for tone control (moved outside loop to prevent artifacts)
static float spring_lp_state_l = 0.0f;
static float spring_lp_state_r = 0.0f;

void SpringReverb_Init(void){
    memset(sr_ap_l,0,sizeof(sr_ap_l));
    memset(sr_ap_r,0,sizeof(sr_ap_r));
    memset(sr_ap_idx_l,0,sizeof(sr_ap_idx_l));
    memset(sr_ap_idx_r,0,sizeof(sr_ap_idx_r));
    spring_lp_state_l = 0.0f;
    spring_lp_state_r = 0.0f;
}

void SpringReverb_SetMix(float mix){ if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f; sr_mix = mix; }
void SpringReverb_SetTension(float t){ if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f; sr_tension = t; }
void SpringReverb_SetDecay(float d){ if (d < 0.01f) d = 0.01f; if (d > 5.0f) d = 5.0f; sr_decay = d; }
void SpringReverb_SetTone(float tone){ if (tone < 0.0f) tone = 0.0f; if (tone > 1.0f) tone = 1.0f; sr_tone = tone; }

static inline float allpass(float in, float *buf, uint16_t size, uint16_t *idx, float fb){
    float buf_out = buf[*idx];
    buf[*idx] = in + buf_out * fb;
    float out = buf_out - buf[*idx] * fb;
    *idx = (*idx + 1) % size;
    return out;
}

void SpringReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size){
    // tension maps to allpass feedback, decay affects overall feedback scaling
    float base_fb = 0.5f + 0.45f * sr_tension;
    float fb = base_fb * expf(-3.0f/(sr_decay*48000.0f));

    // Improved LP filter coefficient: range 0.002 to 0.02 (stable at 48kHz)
    float lpcoef = 0.002f + 0.018f * (1.0f - sr_tone);

    for(uint16_t i=0;i<size;i++){
        float inl = inL[i];
        float inr = inR[i];

        float wetl = inl, wetr = inr;
        // run through short allpass chain to emulate dispersive spring behavior
        for(int a=0;a<SR_ALLP;a++){
            float apfb = 0.7f + 0.05f * a;
            wetl = allpass(wetl, sr_ap_l[a], SR_AP_SIZE, &sr_ap_idx_l[a], apfb * fb);
            wetr = allpass(wetr, sr_ap_r[a], SR_AP_SIZE, &sr_ap_idx_r[a], apfb * fb);
        }

        // Proper one-pole lowpass for tone control (state outside loop for stability)
        spring_lp_state_l += lpcoef * (wetl - spring_lp_state_l);
        spring_lp_state_r += lpcoef * (wetr - spring_lp_state_r);

        // Band-pass effect: mix original with lowpassed to get spring resonance
        float shaped_l = wetl - 0.5f * spring_lp_state_l;
        float shaped_r = wetr - 0.5f * spring_lp_state_r;

        outL[i] = (1.0f - sr_mix) * inl + sr_mix * shaped_l;
        outR[i] = (1.0f - sr_mix) * inr + sr_mix * shaped_r;
    }
}
