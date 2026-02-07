#include "../effectsmanager/multieffects_manager.h"

#include <slider_conversions.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h> // for fabsf

// Global effect chain
static EffectChain g_chain;

// Temporary audio buffers for chaining
__attribute__((section(".adcarray"), aligned(32))) static float temp_bufferL[512]; // Adjust size as needed
__attribute__((section(".adcarray"), aligned(32))) static float temp_bufferR[512];

// Effect names
const char* effect_names[EFFECT_COUNT] = {
    "DELAY",
	"OVERDRIVE",
	"CHORUS",
	"TREMOLO",
	"FLANGER",
	"PHASER",
	"DISTORTION",
	"AUTOWAH",
	"COMPRESSOR",
	"HALLREVERB",
	"PLATEREVERB",
	"SPRINGREVERB",
	"SHIMMERREVERB"

};

// Parameter metadata for each effect
static const ParamInfo param_info[EFFECT_COUNT][MAX_PARAMS_PER_EFFECT] = {
    // EFFECT_DELAY
    {
        {0.0f, 800.0f, "ms", "Time"},      // param 0
        {0.0f, 1.0f, "%", "Feedback"},      // param 1
        {0.0f, 1.0f, "", "Tone"},           // param 2
        {0.0f, 1.0f, "%", "Mix"}            // param 3
    },
    // EFFECT_OVERDRIVE (simplified with DaisySP SoftClip)
    {
        {1.0f, 10.0f, "", "Drive"},        // param 0 - Input gain (1.0-10.0)
        {0.0f, 1.0f, "%", "Level"},        // param 1 - Output level (0.0-1.0)
        {0.0f, 1.0f, "", "Tone"},          // param 2 - Tone filter cutoff (0.0-1.0)
        {0.0f, 0.0f, "", ""}                // param 3 (unused)
    },
    // EFFECT_CHORUS
    {
        {0.1f, 5.0f, "Hz", "Rate"},        // param 0
        {0.0f, 1.0f, "", "Depth"},          // param 1
        {0.0f, 1.0f, "%", "Mix"},           // param 2
        {0.0f, 0.0f, "", ""}                // param 3 (unused)
    },
    // EFFECT_TREMOLO
    {
        {0.1f, 20.0f, "Hz", "Rate"},        // param 0
        {0.0f, 1.0f, "", "Depth"},          // param 1
        {0.0f, 1.0f, "%", "Mix"},           // param 2
        {0.0f, 0.0f, "", ""}                // param 3 (unused)
    },
    // EFFECT_FLANGER
    {
        {0.1f, 5.0f, "Hz", "Rate"},        // param 0
        {0.0f, 1.0f, "", "Depth"},          // param 1
        {0.0f, 1.0f, "", "Feedback"},       // param 2
        {1.0f, 7.0f, "ms", "Delay"}        // param 3
    },
    // EFFECT_PHASER
    {
        {0.1f, 5.0f, "Hz", "Rate"},        // param 0
        {0.0f, 1.0f, "", "Depth"},          // param 1
        {0.0f, 1.0f, "", "Feedback"},       // param 2
        {200.0f, 4000.0f, "Hz", "Freq"}     // param 3
    },
    // EFFECT_DISTORTION
    {
        {1.0f, 20.0f, "", "Gain"},          // param 0
        {0.0f, 1.0f, "%", "Level"},         // param 1
        {0.0f, 1.0f, "", "Tone"},           // param 2
        {0.0f, 0.0f, "", ""}                // param 3 (unused)
    },
    // EFFECT_AUTOWAH
    {
        {0.0f, 1.0f, "", "Wah"},            // param 0
        {0.0f, 1.0f, "%", "Level"},         // param 1
        {0.0f, 1.0f, "%", "Mix"},           // param 2
        {0.5f, 3.0f, "", "Output"}          // param 3
    },
    // EFFECT_COMPRESSOR
    {
        {-60.0f, 0.0f, "dB", "Amount"},    // param 0 (threshold in dB)
        {1.0f, 20.0f, ":1", "Ratio"},      // param 1
        {0.1f, 100.0f, "ms", "Attack"},     // param 2
        {10.0f, 1000.0f, "ms", "Release"}   // param 3
    },
    // EFFECT_HALL_REVERB
    {
        {0.1f, 10.f, "", "Time"},          // param 0
        {0.0f, 1.0f, "", "Damping"},        // param 1
        {0.0f, 100.0f, "ms", "PreDelay"},   // param 2
        {0.0f, 1.0f, "%", "Mix"}            // param 3
    },
    // EFFECT_PLATE_REVERB
    {
        {0.1f, 10.0f, "", "Time"},          // param 0
        {0.0f, 1.0f, "", "Damping"},        // param 1
        {0.0f, 1.0f, "", "Tone"},           // param 2
        {0.0f, 1.0f, "%", "Mix"}            // param 3
    },
    // EFFECT_SPRING_REVERB
    {
        {0.1f, 1.0f, "", "Tension"},          // param 0
        {0.0f, 5.0f, "", "Decay"},        // param 1
        {0.0f, 1.0f, "", "Tone"},           // param 2
        {0.0f, 1.0f, "%", "Mix"}            // param 3
    },
    // EFFECT_SHIMMER_REVERB
    {
        {0.1f, 10.0f, "", "Time"},          // param 0
        {0.0f, 0.05f, "", "Depth"},        // param 1
        {0.1f, 5.0f, "", "Rate"},           // param 2
        {0.0f, 1.0f, "%", "Mix"}            // param 3
    }
};

void MultiEffects_Init(void) {
    memset(&g_chain, 0, sizeof(EffectChain));
    g_chain.count = 0;
    g_chain.selected_instance = 0;
    g_chain.selected_param = 0;

    // Initialize effect modules at startup (no per-instance state assumed by modules)
    Delay_Init();
    Overdrive_Init();
    Chorus_Init();
    Tremolo_Init();
    Flanger_Init();
    Phaser_Init();
    Distortion_Init();
    AutoWah_Init();
    Compressor_Init(48000.0f); // Assuming 48kHz sample rate
    HallReverb_Init();
    PlateReverb_Init();
    SpringReverb_Init();
    ShimmerReverb_Init();
}

bool MultiEffects_AddEffect(EffectType type) {
    if (g_chain.count >= MAX_EFFECT_CHAIN || type >= EFFECT_COUNT) {
        return false;
    }

    // Prepare new instance at the end of the chain
    EffectInstance* instance = &g_chain.instances[g_chain.count];
    instance->type = type;
    instance->enabled = true;
    strncpy(instance->name, effect_names[type], sizeof(instance->name) - 1);
    instance->name[sizeof(instance->name) - 1] = '\0';

    // Set sensible default parameters for each effect (stored in natural units)
    switch (type) {
        case EFFECT_DELAY:
            instance->params[0] = 100.0f; // time (ms)
            instance->params[1] = 0.5f;   // feedback
            instance->params[2] = 0.7f;   // tone
            instance->params[3] = 0.3f;   // mix
            break;
        case EFFECT_OVERDRIVE:
            instance->params[0] = 5.0f;   // drive (gain)
            instance->params[1] = 0.5f;   // level (0-1, default 50%)
            instance->params[2] = 0.5f;   // tone (0-1, default mid: ~5.5kHz)
            instance->params[3] = 0.7f;   // damping (resonance, default 0.7)
            break;
        case EFFECT_CHORUS:
            instance->params[0] = 1.0f;   // rate
            instance->params[1] = 0.5f;   // depth
            instance->params[2] = 0.5f;   // mix
            instance->params[3] = 0.0f;
            break;
        case EFFECT_TREMOLO:
            instance->params[0] = 2.0f;   // rate
            instance->params[1] = 0.5f;   // depth
            instance->params[2] = 0.5f;   // mix
            instance->params[3] = 0.0f;
            break;
        case EFFECT_FLANGER:
            instance->params[0] = 0.5f;   // rate
            instance->params[1] = 0.5f;   // depth
            instance->params[2] = 0.5f;   // feedback
            instance->params[3] = 5.0f;   // delay (ms)
            break;
        case EFFECT_PHASER:
            instance->params[0] = 0.5f;   // rate
            instance->params[1] = 0.5f;   // depth
            instance->params[2] = 0.5f;   // feedback
            instance->params[3] = 2000.0f; // freq (Hz)
            break;
        case EFFECT_DISTORTION:
            instance->params[0] = 10.0f;  // gain
            instance->params[1] = 0.5f;   // level
            instance->params[2] = 0.5f;   // tone
            instance->params[3] = 0.0f;
            break;
        case EFFECT_AUTOWAH:
            instance->params[0] = 0.5f;   // wah (resonance/Q)
            instance->params[1] = 0.5f;   // level (sensitivity)
            instance->params[2] = 0.5f;   // mix
            instance->params[3] = 1.75f;  // wet boost (0.5-3.0, default 50% = 1.75)
            break;
        case EFFECT_COMPRESSOR:
            instance->params[0] = 33.0f;   // amount (0-100%, default ~33% = -40dB threshold)
            instance->params[1] = 4.0f;    // ratio
            instance->params[2] = 5.0f;    // attack (ms)
            instance->params[3] = 100.0f;  // release (ms)
            break;
        case EFFECT_HALL_REVERB:
            instance->params[0] = 3.0f;   // time (s)
            instance->params[1] = 0.5f;   // damping
            instance->params[2] = 20.0f;  // predelay (ms)
            instance->params[3] = 0.5f;   // mix
            break;
        case EFFECT_PLATE_REVERB:
            instance->params[0] = 1.0f;   // time (s)
            instance->params[1] = 0.5f;   // damping
            instance->params[2] = 0.5f;   // tone
            instance->params[3] = 0.35f;  // mix
            break;
        case EFFECT_SPRING_REVERB:
            instance->params[0] = 0.6f;   // tension
            instance->params[1] = 0.5f;   // decay
            instance->params[2] = 10.0f;  // tone
            instance->params[3] = 0.35f;  // mix
            break;
        case EFFECT_SHIMMER_REVERB:
            instance->params[0] = 1.5f;   // time (s)
            instance->params[1] = 0.01f;  // depth
            instance->params[2] = 0.25f;  // rate
            instance->params[3] = 0.35f;  // mix
            break;
        default:
            for (int i = 0; i < MAX_PARAMS; ++i) instance->params[i] = 0.0f;
            break;
    }

    // Add instance to chain and immediately apply defaults to the effect module.
    // We increment count first so MultiEffects_SetEffectParam can validate the index.
    g_chain.count++;
    uint8_t added_index = g_chain.count - 1;
    for (uint8_t p = 0; p < MAX_PARAMS; p++) {
        // This will both store the value and call the module setter so the audio path
        // sees correct settings immediately.
        MultiEffects_SetEffectParam(added_index, p, instance->params[p]);
        // Ensure dirty flag is cleared after initial apply
        g_chain.instances[added_index].params_dirty[p] = false;
    }

    return true;
}

bool MultiEffects_RemoveEffect(uint8_t index) {
    if (index >= g_chain.count) {
        return false;
    }

    // Shift remaining effects down
    for (uint8_t i = index; i < g_chain.count - 1; i++) {
        g_chain.instances[i] = g_chain.instances[i + 1];
    }

    g_chain.count--;

    // Adjust selected instance if needed
    if (g_chain.selected_instance >= g_chain.count && g_chain.count > 0) {
        g_chain.selected_instance = g_chain.count - 1;
    }

    return true;
}

void MultiEffects_ToggleEffect(uint8_t index) {
    if (index < g_chain.count) {
        g_chain.instances[index].enabled = !g_chain.instances[index].enabled;
    }
}

void MultiEffects_SetEffectParam(uint8_t instance_idx, uint8_t param_idx, float value) {
    if (instance_idx < g_chain.count && param_idx < MAX_PARAMS) {
        // Store value in the instance (natural units)
        g_chain.instances[instance_idx].params[param_idx] = value;
        // Mark dirty so other systems know it changed; clear after applying
        g_chain.instances[instance_idx].params_dirty[param_idx] = true;

        // Apply parameter to the actual effect module immediately so UI feedback is instant.
        EffectInstance* instance = &g_chain.instances[instance_idx];
        switch (instance->type) {
            case EFFECT_DELAY:
                if (param_idx == 0) Delay_SetTime(value);
                else if (param_idx == 1) Delay_SetFeedback(value);
                else if (param_idx == 2) Delay_SetTone(value);
                else if (param_idx == 3) Delay_SetMix(value);
                break;
            case EFFECT_OVERDRIVE:
                if (param_idx == 0) Overdrive_SetGain(value);
                else if (param_idx == 1) Overdrive_SetLevel(value);
                else if (param_idx == 2) Overdrive_SetTone(value);
                break;
            case EFFECT_CHORUS:
                if (param_idx == 0) Chorus_SetRate(value);
                else if (param_idx == 1) Chorus_SetDepth(value);
                else if (param_idx == 2) Chorus_SetMix(value);
                break;
            case EFFECT_TREMOLO:
                if (param_idx == 0) Tremolo_SetRate(value);
                else if (param_idx == 1) Tremolo_SetDepth(value);
                else if (param_idx == 2) Tremolo_SetMix(value);
                break;
            case EFFECT_FLANGER:
                if (param_idx == 0) Flanger_SetRate(value);
                else if (param_idx == 1) Flanger_SetDepth(value);
                else if (param_idx == 2) Flanger_SetFeedback(value);
                else if (param_idx == 3) Flanger_SetDelay(value);
                break;
            case EFFECT_PHASER:
                if (param_idx == 0) Phaser_SetRate(value);
                else if (param_idx == 1) Phaser_SetDepth(value);
                else if (param_idx == 2) Phaser_SetFeedback(value);
                else if (param_idx == 3) Phaser_SetFreq(value);
                break;
            case EFFECT_DISTORTION:
                if (param_idx == 0) Distortion_SetGain(value);
                else if (param_idx == 1) Distortion_SetLevel(value);
                else if (param_idx == 2) Distortion_SetTone(value);
                break;
            case EFFECT_AUTOWAH:
                if (param_idx == 0) AutoWah_SetWah(value);
                else if (param_idx == 1) AutoWah_SetLevel(value);
                else if (param_idx == 2) AutoWah_SetMix(value);
                else if (param_idx == 3) AutoWah_SetWetBoost(value);
                break;
            case EFFECT_COMPRESSOR:
                if (param_idx == 0) Compressor_SetThreshold(value); // slider now returns negative dB directly
                else if (param_idx == 1) Compressor_SetRatio(value);
                else if (param_idx == 2) Compressor_SetAttack(value);
                else if (param_idx == 3) Compressor_SetRelease(value);
                break;
            case EFFECT_HALL_REVERB:
                if (param_idx == 0) HallReverb_SetTime(value);
                else if (param_idx == 1) HallReverb_SetDamping(value);
                else if (param_idx == 2) HallReverb_SetPreDelayMs(value);
                else if (param_idx == 3) HallReverb_SetMix(value);
                break;
            case EFFECT_PLATE_REVERB:
                if (param_idx == 0) PlateReverb_SetTime(value);
                else if (param_idx == 1) PlateReverb_SetDamping(value);
                else if (param_idx == 2) PlateReverb_SetTone(value);
                else if (param_idx == 3) PlateReverb_SetMix(value);
                break;
            case EFFECT_SPRING_REVERB:
                if (param_idx == 0) SpringReverb_SetTension(value);
                else if (param_idx == 1) SpringReverb_SetDecay(value);
                else if (param_idx == 2) SpringReverb_SetTone(value);
                else if (param_idx == 3) SpringReverb_SetMix(value);
                break;
            case EFFECT_SHIMMER_REVERB:
                if (param_idx == 0) ShimmerReverb_SetTime(value);
                else if (param_idx == 1) ShimmerReverb_SetDepth(value);
                else if (param_idx == 2) ShimmerReverb_SetRate(value);
                else if (param_idx == 3) ShimmerReverb_SetMix(value);
                break;
            default:
                break;
        }

        // Parameter has been applied to module; clear dirty flag.
        g_chain.instances[instance_idx].params_dirty[param_idx] = false;
    }
}

void MultiEffects_ProcessAudio(float* inL, float* inR, float* outL, float* outR, uint16_t size) {
    // If no effects in chain, passthrough input to output
    if (g_chain.count == 0) {
        memcpy(outL, inL, size * sizeof(float));
        memcpy(outR, inR, size * sizeof(float));
        return;
    }

    // Start with input
    float* currentL = inL;
    float* currentR = inR;
    float* nextL = temp_bufferL;
    float* nextR = temp_bufferR;

    // Process each enabled effect in the chain
    for (uint8_t i = 0; i < g_chain.count; i++) {
        EffectInstance* instance = &g_chain.instances[i];

        if (!instance->enabled) {
            continue; // Skip disabled effects
        }

        // IMPORTANT: Do NOT re-apply parameters every audio block here.
        // MultiEffects_SetEffectParam applies parameter values when the UI changes them
        // (or when an effect is added). Reapplying every block was causing needless
        // overhead and potential conflicts for modules that are not instance-aware.

        // Process audio through this effect
        switch (instance->type) {
            case EFFECT_DELAY:
                Delay_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_OVERDRIVE:
                Overdrive_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_CHORUS:
                Chorus_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_TREMOLO:
                Tremolo_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_FLANGER:
                Flanger_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_PHASER:
                Phaser_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_DISTORTION:
                Distortion_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_AUTOWAH:
                AutoWah_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_COMPRESSOR:
                Compressor_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_HALL_REVERB:
                HallReverb_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_PLATE_REVERB:
                PlateReverb_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_SPRING_REVERB:
                SpringReverb_Process(currentL, currentR, nextL, nextR, size);
                break;
            case EFFECT_SHIMMER_REVERB:
                ShimmerReverb_Process(currentL, currentR, nextL, nextR,size);
                break;
            default:
                for (uint16_t s = 0; s < size; s++) {
                    nextL[s] = currentL[s];
                    nextR[s] = currentR[s];
                }
                break;
        }

        // Swap buffers for next iteration
        float* tempL = currentL;
        float* tempR = currentR;
        currentL = nextL;
        currentR = nextR;
        nextL = tempL;
        nextR = tempR;
    }
    // After all effects, copy final processed buffer to output
    for (uint16_t s = 0; s < size; s++) {
        outL[s] = currentL[s];
        outR[s] = currentR[s];
    }
}

// UI Navigation helpers
uint8_t MultiEffects_GetChainCount(void) {
    return g_chain.count;
}

EffectInstance* MultiEffects_GetInstance(uint8_t index) {
    if (index < g_chain.count) {
        return &g_chain.instances[index];
    }
    return NULL;
}

void MultiEffects_SetSelectedInstance(uint8_t index) {
    if (index < g_chain.count) {
        g_chain.selected_instance = index;
    }
}

// Get parameter info for a specific effect and parameter
const ParamInfo* MultiEffects_GetParamInfo(EffectType type, uint8_t param_idx) {
    if (type >= EFFECT_COUNT || param_idx >= MAX_PARAMS_PER_EFFECT) {
        return NULL;
    }
    return &param_info[type][param_idx];
}

// Helper function to manually format a float as a string (no printf %f support)
static void float_to_string(float value, char* buf, size_t buf_size, int decimals) {
    if (buf == NULL || buf_size == 0) return;

    // Handle negative
    int pos = 0;
    if (value < 0.0f) {
        buf[pos++] = '-';
        value = -value;
    }

    // Get integer and fractional parts
    int int_part = (int)value;
    float frac_part = value - (float)int_part;

    // Format integer part
    char temp[20];
    int temp_pos = 0;
    if (int_part == 0) {
        temp[temp_pos++] = '0';
    } else {
        while (int_part > 0 && temp_pos < 19) {
            temp[temp_pos++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    // Reverse integer digits
    for (int i = temp_pos - 1; i >= 0 && pos < (int)buf_size - 1; i--) {
        buf[pos++] = temp[i];
    }

    // Add decimal point and fractional part
    if (decimals > 0 && pos < (int)buf_size - 1) {
        buf[pos++] = '.';
        for (int i = 0; i < decimals && pos < (int)buf_size - 1; i++) {
            frac_part *= 10.0f;
            int digit = (int)frac_part;
            buf[pos++] = '0' + (digit % 10);
            frac_part -= (float)digit;
        }
    }

    buf[pos] = '\0';
}

// Format parameter value for display
void MultiEffects_FormatParamValue(EffectType type, uint8_t param_idx, int bar_value, char* buf, size_t buf_size) {
    const ParamInfo* info = MultiEffects_GetParamInfo(type, param_idx);

    if (!info || buf == NULL || buf_size == 0) {
        snprintf(buf, buf_size, "%d", bar_value);
        return;
    }

    // Check if this parameter is used (max != 0 or has a name)
    if (info->max == 0.0f && info->name[0] == '\0') {
        snprintf(buf, buf_size, "--");
        return;
    }

    // Use the same conversion as the slider -> param path so the displayed value
    // matches exactly what MultiEffects_ConvertParam() will return when the slider
    // is at `bar_value`.
    float actual_value = MultiEffects_ConvertParam(type, param_idx, (uint32_t)bar_value);

    // Format based on the type of value
    if (strcmp(info->unit, "Hz") == 0) {
        // Frequency - show kHz if >= 1000
        if (actual_value >= 1000.0f) {
            float khz_value = actual_value / 1000.0f;
            float_to_string(khz_value, buf, buf_size - 3, 1);
            strcat(buf, "kHz");
        } else {
            float_to_string(actual_value, buf, buf_size - 2, 1);
            strcat(buf, "Hz");
        }
    } else if (strcmp(info->unit, "%") == 0) {
        // Percentage parameters from convert functions are typically 0.0-1.0
        float pct = actual_value * 100.0f;
        float_to_string(pct, buf, buf_size - 1, 0);
        strcat(buf, "%");
    } else if (strcmp(info->unit, "dB") == 0) {
        // Compression amount - slider 0-100% maps to -60dB to 0dB threshold
        float_to_string(actual_value, buf, buf_size - 2, 1);
        strcat(buf, "dB");
    } else if (strcmp(info->unit, "ms") == 0) {
        // Milliseconds
        if (actual_value >= 1000.0f) {
            float sec_value = actual_value / 1000.0f;
            float_to_string(sec_value, buf, buf_size - 1, 2);
            strcat(buf, "s");
        } else {
            float_to_string(actual_value, buf, buf_size - 2, 0);
            strcat(buf, "ms");
        }
    } else if (strcmp(info->unit, ":1") == 0) {
        // Ratio (for compressor)
        float_to_string(actual_value, buf, buf_size - 2, 1);
        strcat(buf, ":1");
    } else if (info->unit[0] != '\0') {
        // Generic unit
        size_t unit_len = strlen(info->unit);
        float_to_string(actual_value, buf, buf_size - unit_len, 1);
        strcat(buf, info->unit);
    } else {
        // No unit, just value
        if (actual_value < 10.0f) {
            float_to_string(actual_value, buf, buf_size, 2);
        } else {
            float_to_string(actual_value, buf, buf_size, 1);
        }
    }
}

void MultiEffects_SetSelectedParam(uint8_t param) {
    if (param < MAX_PARAMS) {
        g_chain.selected_param = param;
    }
}

uint8_t MultiEffects_GetSelectedInstance(void) {
    return g_chain.selected_instance;
}

uint8_t MultiEffects_GetSelectedParam(void) {
    return g_chain.selected_param;
}

// Find best slider position (0..100) whose conversion maps to actual_value.
// This performs a simple search over 0..100 using the same conversion function
// used when moving sliders. It ensures the slider position shown in the UI
// corresponds to the stored natural-unit parameter value.
uint8_t MultiEffects_ActualToSlider(EffectType type, uint8_t param_idx, float actual_value) {
    uint8_t best = 0;
    float best_diff = 1e30f;
    for (int v = 0; v <= 100; ++v) {
        float val = MultiEffects_ConvertParam(type, param_idx, (uint32_t)v);
        float diff = fabsf(val - actual_value);
        if (diff < best_diff) {
            best_diff = diff;
            best = (uint8_t)v;
        }
    }
    return best;
}

float MultiEffects_ConvertParam(EffectType type, uint8_t param_idx, uint32_t slider_val) {
    switch (type) {
        case EFFECT_DELAY:
            if      (param_idx == 0) return slider_to_delay_time(slider_val);
            else if (param_idx == 1) return slider_to_delay_feedback(slider_val);
            else if (param_idx == 2) return slider_to_delay_tone(slider_val);
            else if (param_idx == 3) return slider_to_delay_mix(slider_val);
            break;
        case EFFECT_OVERDRIVE:
            if      (param_idx == 0) return slider_to_overdrive_gain(slider_val);
            else if (param_idx == 1) return slider_to_overdrive_level(slider_val);
            else if (param_idx == 2) return slider_to_overdrive_tone(slider_val);
            break;
        case EFFECT_CHORUS:
            if      (param_idx == 0) return slider_to_chorus_rate(slider_val);
            else if (param_idx == 1) return slider_to_chorus_depth(slider_val);
            else if (param_idx == 2) return slider_to_chorus_mix(slider_val);
            break;
        case EFFECT_TREMOLO:
            if      (param_idx == 0) return slider_to_tremolo_rate(slider_val);
            else if (param_idx == 1) return slider_to_tremolo_depth(slider_val);
            else if (param_idx == 2) return slider_to_tremolo_mix(slider_val);
            break;
        case EFFECT_FLANGER:
            if      (param_idx == 0) return slider_to_flanger_rate(slider_val);
            else if (param_idx == 1) return slider_to_flanger_depth(slider_val);
            else if (param_idx == 2) return slider_to_flanger_feedback(slider_val);
            else if (param_idx == 3) return slider_to_flanger_delay(slider_val);
            break;
        case EFFECT_PHASER:
            if      (param_idx == 0) return slider_to_phaser_rate(slider_val);
            else if (param_idx == 1) return slider_to_phaser_depth(slider_val);
            else if (param_idx == 2) return slider_to_phaser_feedback(slider_val);
            else if (param_idx == 3) return slider_to_phaser_freq(slider_val);
            break;
        case EFFECT_DISTORTION:
            if      (param_idx == 0) return slider_to_distortion_gain(slider_val);
            else if (param_idx == 1) return slider_to_distortion_level(slider_val);
            else if (param_idx == 2) return slider_to_distortion_tone(slider_val);
            break;
        case EFFECT_AUTOWAH:
            if      (param_idx == 0) return slider_to_autowah_wah(slider_val);
            else if (param_idx == 1) return slider_to_autowah_level(slider_val);
            else if (param_idx == 2) return slider_to_autowah_mix(slider_val);
            else if (param_idx == 3) return slider_to_autowah_wet_boost(slider_val);
            break;
        case EFFECT_COMPRESSOR:
            if      (param_idx == 0) return slider_to_compressor_amount(slider_val);
            else if (param_idx == 1) return slider_to_compressor_ratio(slider_val);
            else if (param_idx == 2) return slider_to_compressor_attack(slider_val);
            else if (param_idx == 3) return slider_to_compressor_release(slider_val);
            break;
        case EFFECT_HALL_REVERB:
            if      (param_idx == 0) return slider_to_hall_time(slider_val);
            else if (param_idx == 1) return slider_to_hall_damping(slider_val);
            else if (param_idx == 2) return slider_to_hall_predelay_ms(slider_val);
            else if (param_idx == 3) return slider_to_hall_mix(slider_val);
            break;
        case EFFECT_PLATE_REVERB:
            if      (param_idx == 0) return slider_to_plate_time(slider_val);
            else if (param_idx == 1) return slider_to_plate_damping(slider_val);
            else if (param_idx == 2) return slider_to_plate_tone(slider_val);
            else if (param_idx == 3) return slider_to_plate_mix(slider_val);
            break;
        case EFFECT_SPRING_REVERB:
            if      (param_idx == 0) return slider_to_spring_tension(slider_val);
            else if (param_idx == 1) return slider_to_spring_decay(slider_val);
            else if (param_idx == 2) return slider_to_spring_tone(slider_val);
            else if (param_idx == 3) return slider_to_spring_mix(slider_val);
            break;
        case EFFECT_SHIMMER_REVERB:
            if      (param_idx == 0) return slider_to_shimmer_time(slider_val);
            else if (param_idx == 1) return slider_to_shimmer_depth(slider_val);
            else if (param_idx == 2) return slider_to_shimmer_rate(slider_val);
            else if (param_idx == 3) return slider_to_shimmer_mix(slider_val);
            break;
        default:
            break;
    }
    return 0.0f;
}

// Helper to find effect index by name
int MultiEffects_FindEffectIndexByName(const char *name) {
    for (int i = 0; i < g_chain.count; i++) {
        if (strcmp(g_chain.instances[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}
