#include "overdrive.h"
#include <string.h>
#include <math.h>

// Global state for left and right channels
static Overdrive_State od_state_L = {0};
static Overdrive_State od_state_R = {0};

// DaisySP-style SoftClip function (polynomial saturation)
static inline float SoftClip(float x) {
    if (x < -3.0f)
        return -1.0f;
    else if (x > 3.0f)
        return 1.0f;
    else {
        // SoftLimit: x * (27 + x²) / (27 + 9x²)
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

// DaisySP-style fclamp function (fast clamping with FPU optimization)
static inline float fclamp(float in, float min, float max) {
    return (in < min) ? min : ((in > max) ? max : in);
}

void Overdrive_Init(void) {
    memset(&od_state_L, 0, sizeof(Overdrive_State));
    memset(&od_state_R, 0, sizeof(Overdrive_State));
    
    od_state_L.drive = 2.0f;
    od_state_L.tone = 0.5f;
    od_state_L.level = 1.0f;
    
    od_state_R.drive = 2.0f;
    od_state_R.tone = 0.5f;
    od_state_R.level = 1.0f;
}

void Overdrive_SetGain(float gain) {
    // Clamp drive between 1.0 and 10.0
    gain = fclamp(gain, 1.0f, 10.0f);
    od_state_L.drive = gain;
    od_state_R.drive = gain;
}

void Overdrive_SetLevel(float level) {
    // Clamp output level between 0.0 and 1.0
    level = fclamp(level, 0.0f, 1.0f);
    od_state_L.level = level;
    od_state_R.level = level;
}

void Overdrive_SetTone(float tone) {
    // Clamp tone (filter coefficient) between 0.0 and 1.0
    tone = fclamp(tone, 0.0f, 1.0f);
    od_state_L.tone = tone;
    od_state_R.tone = tone;
}

void Overdrive_Process(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    // Pre-compute tone filter coefficient (0.01 to 0.3)
    const float tone_coeff = 0.01f + od_state_L.tone * 0.3f;
    const float drive = od_state_L.drive;
    const float level = od_state_L.level;
    
    for (uint16_t i = 0; i < size; i++) {
        // Apply drive gain and DaisySP-style soft clipping
        float dL = SoftClip(inL[i] * drive);
        float dR = SoftClip(inR[i] * drive);
        
        // Apply one-pole lowpass filter for tone control
        // Using DaisySP-style fonepole logic: state += coeff * (input - state)
        od_state_L.tone_state_l += tone_coeff * (dL - od_state_L.tone_state_l);
        od_state_R.tone_state_r += tone_coeff * (dR - od_state_R.tone_state_r);
        
        // Output with level control
        outL[i] = od_state_L.tone_state_l * level;
        outR[i] = od_state_R.tone_state_r * level;
    }
}

