#include "plate_reverb.h"
#include <string.h>
#include <math.h>

#define PR_COMBS 3
#define PR_APS 3

#define PR_COMB_SIZE 1500
#define PR_AP_SIZE 600

__attribute__((section(".sdram"), aligned(32))) static float pr_comb_l[PR_COMBS][PR_COMB_SIZE];
__attribute__((section(".sdram"), aligned(32))) static float pr_comb_r[PR_COMBS][PR_COMB_SIZE];
static uint16_t pr_comb_idx_l[PR_COMBS];
static uint16_t pr_comb_idx_r[PR_COMBS];

__attribute__((section(".sdram"), aligned(32))) static float pr_ap_l[PR_APS][PR_AP_SIZE];
__attribute__((section(".sdram"), aligned(32))) static float pr_ap_r[PR_APS][PR_AP_SIZE];
static uint16_t pr_ap_idx_l[PR_APS];
static uint16_t pr_ap_idx_r[PR_APS];

static float pr_mix = 0.35f;
static float pr_time = 1.0f;
static float pr_damp = 0.5f;
static float pr_tone = 0.5f;

void PlateReverb_Init(void) {
    memset(pr_comb_l,0,sizeof(pr_comb_l));
    memset(pr_comb_r,0,sizeof(pr_comb_r));
    memset(pr_ap_l,0,sizeof(pr_ap_l));
    memset(pr_ap_r,0,sizeof(pr_ap_r));
    memset(pr_comb_idx_l,0,sizeof(pr_comb_idx_l));
    memset(pr_comb_idx_r,0,sizeof(pr_comb_idx_r));
    memset(pr_ap_idx_l,0,sizeof(pr_ap_idx_l));
    memset(pr_ap_idx_r,0,sizeof(pr_ap_idx_r));
}

void PlateReverb_SetMix(float mix){ if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f; pr_mix = mix; }
void PlateReverb_SetTime(float time){ if (time < 0.01f) time = 0.01f; if (time > 10.0f) time = 10.0f; pr_time = time; }
void PlateReverb_SetDamping(float damp){ if (damp < 0.0f) damp = 0.0f; if (damp > 1.0f) damp = 1.0f; pr_damp = damp; }
void PlateReverb_SetTone(float tone){ if (tone < 0.0f) tone = 0.0f; if (tone > 1.0f) tone = 1.0f; pr_tone = tone; }

static inline float comb(float in, float *buf, uint16_t size, uint16_t *idx, float fb, float damp, float *damp_state){
    float out = buf[*idx];
    *damp_state = (*damp_state * (1.0f - damp)) + (out * damp);
    buf[*idx] = in + (*damp_state * fb);
    *idx = (*idx + 1) % size;
    return out;
}

static inline float allpass(float in, float *buf, uint16_t size, uint16_t *idx, float fb){
    float buf_out = buf[*idx];
    buf[*idx] = in + buf_out * fb;
    float out = buf_out - buf[*idx] * fb;
    *idx = (*idx + 1) % size;
    return out;
}

void PlateReverb_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size){
    float base_fb = 0.55f + 0.4f * fminf(1.0f, pr_time/2.0f);
    float fb = base_fb * expf(-3.0f/(pr_time*48000.0f));
    float damp = pr_damp;

    static float comb_damp_l[PR_COMBS] = {0};
    static float comb_damp_r[PR_COMBS] = {0};

    for(uint16_t i=0;i<size;i++){
        float inl = inL[i];
        float inr = inR[i];

        float wetl = 0.0f, wetr = 0.0f;
        for(int c=0;c<PR_COMBS;c++){
            wetl += comb(inl, pr_comb_l[c], PR_COMB_SIZE, &pr_comb_idx_l[c], fb, damp, &comb_damp_l[c]);
            wetr += comb(inr, pr_comb_r[c], PR_COMB_SIZE, &pr_comb_idx_r[c], fb, damp, &comb_damp_r[c]);
        }
        wetl /= PR_COMBS; wetr /= PR_COMBS;

        // a dense allpass chain gives plate-like density
        for(int ap=0;ap<PR_APS;ap++){
            float apfb = 0.75f + 0.05f * (float)ap; // slight variation
            wetl = allpass(wetl, pr_ap_l[ap], PR_AP_SIZE, &pr_ap_idx_l[ap], apfb);
            wetr = allpass(wetr, pr_ap_r[ap], PR_AP_SIZE, &pr_ap_idx_r[ap], apfb);
        }

        // simple tone control: lowpass on wet path (1-pole)
        static float tone_state_l = 0.0f, tone_state_r = 0.0f;
        float lp_coeff = 0.2f + 0.7f * (1.0f - pr_tone); // tone=1 -> brighter (less lowpass)
        tone_state_l = tone_state_l + lp_coeff * (wetl - tone_state_l);
        tone_state_r = tone_state_r + lp_coeff * (wetr - tone_state_r);

        outL[i] = (1.0f - pr_mix) * inl + pr_mix * tone_state_l;
        outR[i] = (1.0f - pr_mix) * inr + pr_mix * tone_state_r;
    }
}
