/**
  ******************************************************************************
  * @file    amp_sim.c
  * @brief   Guitar amplifier simulation implementation
  *          Features multiple amp models with tone stack EQ and tube-style
  *          saturation/clipping algorithms optimized for STM32
  ******************************************************************************
  */
/* USER CODE END Header */

#include "amp_sim.h"
#include <math.h>
#include <string.h>

/* Helper macros */
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/* Amp model names */
static const char* amp_model_names[] = {
    "Clean",
    "Crunch",
    "Lead",
    "Blues",
    "Metal"
};

/* Biquad filter coefficients */
typedef struct {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients (a0 = 1.0)
} BiquadCoeff_t;

/* Internal helper functions */
static void recalculate_eq_coeffs(AmpSim_t* amp);
static float apply_biquad_cached(float input, float* z1, float* z2, 
                                  float b0, float b1, float b2, float a1, float a2);
static float soft_clip_tube(float x, float gain);
static float asymmetric_clip(float x, float gain);

/* Forward declaration */
static void recalculate_eq_coeffs(AmpSim_t* amp);

/* Initialize amp simulation */
void AmpSim_Init(AmpSim_t* amp, uint32_t sample_rate) {
    memset(amp, 0, sizeof(AmpSim_t));
    
    amp->sample_rate = sample_rate;
    amp->model = AMP_MODEL_CLEAN;
    amp->enabled = true;
    
    /* Default tone settings (conservative for headroom) */
    amp->gain = 0.2f;      // Reduced from 0.5 - very gentle drive
    amp->bass = 0.5f;
    amp->mid = 0.5f;
    amp->treble = 0.5f;
    amp->presence = 0.3f;
    amp->master = 0.5f;    // Reduced from 0.7
    
    /* Pre-calculate initial filter coefficients */
    recalculate_eq_coeffs(amp);
}

/* Recalculate EQ filter coefficients (only when tone controls change) */
static void recalculate_eq_coeffs(AmpSim_t* amp) {
    float w0, alpha, cos_w0, sin_w0, A, beta, a0;
    float Q = 0.707f;  // Butterworth response
    
    /* Bass shelving filter (80 Hz) */
    A = powf(10.0f, (amp->bass - 0.5f) * 0.4f);  // +/- 12 dB
    w0 = 2.0f * M_PI * 80.0f / amp->sample_rate;
    cos_w0 = cosf(w0);
    sin_w0 = sinf(w0);
    beta = sqrtf(A) / Q;
    
    amp->bass_b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + beta * sin_w0);
    amp->bass_b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
    amp->bass_b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - beta * sin_w0);
    a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + beta * sin_w0;
    amp->bass_a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0) / a0;
    amp->bass_a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - beta * sin_w0) / a0;
    amp->bass_b0 /= a0;
    amp->bass_b1 /= a0;
    amp->bass_b2 /= a0;
    
    /* Mid peaking filter (800 Hz) */
    A = powf(10.0f, (amp->mid - 0.5f) * 0.4f);  // +/- 12 dB
    w0 = 2.0f * M_PI * 800.0f / amp->sample_rate;
    cos_w0 = cosf(w0);
    sin_w0 = sinf(w0);
    alpha = sin_w0 / (2.0f * Q);
    
    amp->mid_b0 = 1.0f + alpha * A;
    amp->mid_b1 = -2.0f * cos_w0;
    amp->mid_b2 = 1.0f - alpha * A;
    a0 = 1.0f + alpha / A;
    amp->mid_a1 = -2.0f * cos_w0 / a0;
    amp->mid_a2 = (1.0f - alpha / A) / a0;
    amp->mid_b0 /= a0;
    amp->mid_b1 /= a0;
    amp->mid_b2 /= a0;
    
    /* Treble shelving filter (3000 Hz) */
    A = powf(10.0f, (amp->treble - 0.5f) * 0.4f);  // +/- 12 dB
    w0 = 2.0f * M_PI * 3000.0f / amp->sample_rate;
    cos_w0 = cosf(w0);
    sin_w0 = sinf(w0);
    beta = sqrtf(A) / Q;
    
    amp->treble_b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + beta * sin_w0);
    amp->treble_b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
    amp->treble_b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - beta * sin_w0);
    a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + beta * sin_w0;
    amp->treble_a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0) / a0;
    amp->treble_a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - beta * sin_w0) / a0;
    amp->treble_b0 /= a0;
    amp->treble_b1 /= a0;
    amp->treble_b2 /= a0;
    
    /* Presence high-shelf filter (6000 Hz) */
    A = 1.0f + amp->presence * 2.5f;  // Slightly increased from 2.0 for more control
    w0 = 2.0f * M_PI * 6000.0f / amp->sample_rate;
    cos_w0 = cosf(w0);
    sin_w0 = sinf(w0);
    beta = sqrtf(A) / Q;
    
    amp->presence_b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + beta * sin_w0);
    amp->presence_b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
    amp->presence_b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - beta * sin_w0);
    a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + beta * sin_w0;
    amp->presence_a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0) / a0;
    amp->presence_a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - beta * sin_w0) / a0;
    amp->presence_b0 /= a0;
    amp->presence_b1 /= a0;
    amp->presence_b2 /= a0;
    
    /* Store current values to detect future changes */
    amp->last_bass = amp->bass;
    amp->last_mid = amp->mid;
    amp->last_treble = amp->treble;
    amp->last_presence = amp->presence;
}

/* Process audio through amp simulation */
void AmpSim_Process(AmpSim_t* amp, const float* input, float* output, size_t size) {
    if (!amp->enabled) {
        /* Bypass - copy input to output */
        memcpy(output, input, size * sizeof(float));
        return;
    }
    
    /* Check if tone controls changed - recalc coefficients if needed */
    if (amp->bass != amp->last_bass || amp->mid != amp->last_mid ||
        amp->treble != amp->last_treble || amp->presence != amp->last_presence) {
        recalculate_eq_coeffs(amp);
    }
    
    /* Get model-specific parameters */
    float gain_factor, saturation, asymmetry, makeup_gain;
    
    switch (amp->model) {
        case AMP_MODEL_CLEAN:
            gain_factor = 1.0f + (amp->gain * 2.0f);  // 1.0x to 3.0x
            saturation = 0.15f;  // Very light saturation - sparkly clean until you really push it
            asymmetry = 0.0f;    // Symmetric for clean, transparent sound
            makeup_gain = 0.95f; // Minimal compression, preserve dynamics
            break;
            
        case AMP_MODEL_CRUNCH:
            gain_factor = 1.5f + (amp->gain * 5.5f);  // 1.5x to 7.0x (increased for more bite)
            saturation = 0.68f;  // Medium-high for British crunch character
            asymmetry = 0.25f;   // More asymmetry for that Marshall "bark"
            makeup_gain = 0.58f; // Moderate compression
            break;
            
        case AMP_MODEL_LEAD:
            gain_factor = 2.0f + (amp->gain * 10.0f);  // 2.0x to 12.0x
            saturation = 0.8f;
            asymmetry = 0.12f;  // Less asymmetry for tighter, more harmonic distortion
            makeup_gain = 0.45f;  // Increased for more output
            break;
            
        case AMP_MODEL_BLUES:
            gain_factor = 1.2f + (amp->gain * 4.3f);  // 1.2x to 5.5x (increased slightly)
            saturation = 0.48f;  // Reduced for more touch-sensitivity and dynamics
            asymmetry = 0.35f;   // High asymmetry for warm, sag-like blues character
            makeup_gain = 0.72f; // Light compression to preserve touch response
            break;
            
        case AMP_MODEL_METAL:
            gain_factor = 3.0f + (amp->gain * 19.0f);  // 3.0x to 22.0x (increased for brutal gain)
            saturation = 0.92f;  // Extreme saturation for tight, compressed metal
            asymmetry = 0.03f;   // Very low - tight, symmetric for palm mutes
            makeup_gain = 0.28f; // Heavy compression
            break;
            
        default:
            gain_factor = 1.0f;
            saturation = 0.0f;
            asymmetry = 0.0f;
            makeup_gain = 1.0f;
            break;
    }
    
    /* Process each sample */
    for (size_t i = 0; i < size; i++) {
        float sample = input[i];
        
        /* Input scaling to prevent clipping */
        sample *= 0.5f;  // Reduced from 0.7 for more headroom
        
        /* Pre-EQ (bass and mid before clipping) */
        sample = apply_biquad_cached(sample, &amp->bass_z1, &amp->bass_z2,
                                      amp->bass_b0, amp->bass_b1, amp->bass_b2,
                                      amp->bass_a1, amp->bass_a2);
        sample = apply_biquad_cached(sample, &amp->mid_z1, &amp->mid_z2,
                                      amp->mid_b0, amp->mid_b1, amp->mid_b2,
                                      amp->mid_a1, amp->mid_a2);
        
        /* Apply gain and clipping/saturation */
        sample *= gain_factor;
        
        if (asymmetry > 0.01f) {
            sample = asymmetric_clip(sample, saturation) * (1.0f + asymmetry * 0.5f);
        } else {
            sample = soft_clip_tube(sample, saturation);
        }
        
        /* Apply makeup gain to compensate for saturation losses */
        sample *= makeup_gain;

        /* Post-EQ (treble and presence after clipping) */
        sample = apply_biquad_cached(sample, &amp->treble_z1, &amp->treble_z2,
                                      amp->treble_b0, amp->treble_b1, amp->treble_b2,
                                      amp->treble_a1, amp->treble_a2);
        sample = apply_biquad_cached(sample, &amp->presence_z1, &amp->presence_z2,
                                      amp->presence_b0, amp->presence_b1, amp->presence_b2,
                                      amp->presence_a1, amp->presence_a2);
        
        /* DC blocking filter (proper 1st-order HPF) */
        float dc_blocked = sample - amp->dc_block_in_z1 + 0.995f * amp->dc_block_out_z1;
        amp->dc_block_in_z1 = sample;
        amp->dc_block_out_z1 = dc_blocked;
        
        /* Master volume with normalization */
        // Scale master range to 0.3-1.0 to maintain consistent levels
        float master_scaled = 0.3f + (amp->master * 0.7f);
        float final_output = dc_blocked * master_scaled;

        /* Gentle soft limit to prevent occasional peaks (fast tanh approximation) */
        if (final_output > 0.95f) {
            float x = (final_output - 0.95f) * 10.0f;
            float x2 = x * x;
            float tanh_approx = (x > 3.0f) ? 1.0f : x * (27.0f + x2) / (27.0f + 9.0f * x2);
            final_output = 0.95f + 0.05f * tanh_approx;
        } else if (final_output < -0.95f) {
            float x = (final_output + 0.95f) * 10.0f;
            float x2 = x * x;
            float tanh_approx = (x < -3.0f) ? -1.0f : x * (27.0f + x2) / (27.0f + 9.0f * x2);
            final_output = -0.95f + 0.05f * tanh_approx;
        }

        output[i] = final_output;
    }
}

/* Apply biquad filter using cached coefficients */
static float apply_biquad_cached(float input, float* z1, float* z2,
                                 float b0, float b1, float b2, float a1, float a2) {
    float output = b0 * input + *z1;
    *z1 = b1 * input - a1 * output + *z2;
    *z2 = b2 * input - a2 * output;
    return output;
}

/* Improved soft tube-style clipping using optimized tanh approximation */
static float soft_clip_tube(float x, float gain) {
    /* Apply gain with tube-like characteristic */
    float a = x * (1.0f + gain * 1.5f);
    
    /* Fast tanh approximation for tube-like saturation */
    if (a > 3.0f) return 0.995f;
    if (a < -3.0f) return -0.995f;
    
    /* Rational approximation of tanh (smooth, tube-like) */
    float a2 = a * a;
    return a * (27.0f + a2) / (27.0f + 9.0f * a2);
}

/* Improved asymmetric clipping for more realistic tube behavior */
static float asymmetric_clip(float x, float gain) {
    float a = x * (1.0f + gain);
    
    /* Triode-style asymmetric clipping:
     * - Positive side clips harder (grid current clipping)
     * - Negative side clips softer (cutoff clipping)
     */
    if (a > 0.0f) {
        /* Positive: harder clipping with compression */
        if (a > 2.0f) return 0.85f;
        float a2 = a * a;
        return a * (1.0f - 0.18f * a + 0.03f * a2);
    } else {
        /* Negative: softer clipping, more linear */
        if (a < -2.5f) return -0.95f;
        return a * (1.0f + 0.08f * a);
    }
}

/* Set amp model */
void AmpSim_SetModel(AmpSim_t* amp, AmpModel_t model) {
    if (model < AMP_MODEL_COUNT) {
        amp->model = model;
        
        /* Reset filter states when changing models */
        amp->bass_z1 = amp->bass_z2 = 0.0f;
        amp->mid_z1 = amp->mid_z2 = 0.0f;
        amp->treble_z1 = amp->treble_z2 = 0.0f;
        amp->presence_z1 = amp->presence_z2 = 0.0f;
        amp->dc_block_in_z1 = 0.0f;
        amp->dc_block_out_z1 = 0.0f;
    }
}

/* Get current amp model */
AmpModel_t AmpSim_GetModel(const AmpSim_t* amp) {
    return amp->model;
}

/* Enable/disable amp simulation */
void AmpSim_Enable(AmpSim_t* amp, bool enable) {
    amp->enabled = enable;
}

/* Check if amp is enabled */
bool AmpSim_IsEnabled(const AmpSim_t* amp) {
    return amp->enabled;
}

/* Set gain control */
void AmpSim_SetGain(AmpSim_t* amp, float gain) {
    amp->gain = CLAMP(gain, 0.0f, 1.0f);
}

/* Set bass EQ */
void AmpSim_SetBass(AmpSim_t* amp, float bass) {
    amp->bass = CLAMP(bass, 0.0f, 1.0f);
}

/* Set mid EQ */
void AmpSim_SetMid(AmpSim_t* amp, float mid) {
    amp->mid = CLAMP(mid, 0.0f, 1.0f);
}

/* Set treble EQ */
void AmpSim_SetTreble(AmpSim_t* amp, float treble) {
    amp->treble = CLAMP(treble, 0.0f, 1.0f);
}

/* Set presence control */
void AmpSim_SetPresence(AmpSim_t* amp, float presence) {
    amp->presence = CLAMP(presence, 0.0f, 1.0f);
}

/* Set master volume */
void AmpSim_SetMaster(AmpSim_t* amp, float master) {
    amp->master = CLAMP(master, 0.0f, 1.0f);
}

/* Get amp model name */
const char* AmpSim_GetModelName(AmpModel_t model) {
    if (model < AMP_MODEL_COUNT) {
        return amp_model_names[model];
    }
    return "Unknown";
}

/* Get current gain */
float AmpSim_GetGain(const AmpSim_t* amp) {
    return amp->gain;
}

/* Get current bass */
float AmpSim_GetBass(const AmpSim_t* amp) {
    return amp->bass;
}

/* Get current mid */
float AmpSim_GetMid(const AmpSim_t* amp) {
    return amp->mid;
}

/* Get current treble */
float AmpSim_GetTreble(const AmpSim_t* amp) {
    return amp->treble;
}

/* Get current presence */
float AmpSim_GetPresence(const AmpSim_t* amp) {
    return amp->presence;
}

/* Get current master */
float AmpSim_GetMaster(const AmpSim_t* amp) {
    return amp->master;
}
