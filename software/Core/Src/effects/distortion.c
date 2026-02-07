#include "distortion.h"
#include <string.h>
#include <math.h>

static float distortion_gain = 10.0f;  // 1.0 to 100.0 (drive amount)
static float distortion_level = 1.0f;  // 0.0 to 1.0 (output level)
static float distortion_tone = 0.5f;   // 0.0 to 1.0 (tone control - filter cutoff)

// Tone filter states for left and right channels
static float tone_state_L = 0.0f;
static float tone_state_R = 0.0f;

// DaisySP-style SoftLimit function (polynomial saturation)
// More aggressive than SoftClip, good for distortion
static inline float SoftLimit(float x) {
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// DaisySP-style fclamp function
static inline float fclamp(float in, float min, float max) {
    return (in < min) ? min : ((in > max) ? max : in);
}

void Distortion_Init(void) {
    tone_state_L = 0.0f;
    tone_state_R = 0.0f;
}

void Distortion_SetGain(float gain) {
    // Clamp gain between 1.0 and 100.0
    distortion_gain = fclamp(gain, 1.0f, 100.0f);
}

void Distortion_SetLevel(float level) {
    // Clamp output level between 0.0 and 1.0
    distortion_level = fclamp(level, 0.0f, 1.0f);
}

void Distortion_SetTone(float tone) {
    // Clamp tone between 0.0 and 1.0 (controls one-pole filter cutoff)
    distortion_tone = fclamp(tone, 0.0f, 1.0f);
}

void Distortion_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    // Pre-compute constants outside loop
    const float tone_coeff = 0.05f + distortion_tone * 0.35f;  // Filter coefficient
    const float final_gain = distortion_level;
    const float drive = distortion_gain;
    
    for (uint16_t i = 0; i < size; i++) {
        // Apply drive gain and DaisySP-style soft saturation (polynomial)
        float satL = SoftLimit(inL[i] * drive);
        float satR = SoftLimit(inR[i] * drive);
        
        // Apply one-pole lowpass filter for tone control (DaisySP fonepole style)
        tone_state_L += tone_coeff * (satL - tone_state_L);
        tone_state_R += tone_coeff * (satR - tone_state_R);
        
        // Output with level control
        outL[i] = tone_state_L * final_gain;
        outR[i] = tone_state_R * final_gain;
    }
}

