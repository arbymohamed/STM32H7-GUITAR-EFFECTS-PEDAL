#ifndef MULTIEFFECTS_MANAGER_H
#define MULTIEFFECTS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "delay.h"
#include "overdrive.h"
#include "chorus.h"
#include "tremolo.h"
#include "flanger.h"
#include "phaser.h"
#include "distortion.h"
#include "autowah.h"
#include "compressor.h"
#include "hall_reverb.h"
#include "plate_reverb.h"
#include "spring_reverb.h"
#include "shimmer_reverb.h"

#define MAX_EFFECT_CHAIN 8
// Keep MAX_PARAMS in sync with UI (4 sliders on the shared parameter page)
#define MAX_PARAMS 4
#define MAX_PARAMS_PER_EFFECT 4

// Parameter metadata structure
typedef struct {
    float min;           // Minimum parameter value
    float max;           // Maximum parameter value
    const char* unit;    // Unit string (Hz, ms, dB, %, etc.)
    const char* name;    // Parameter name
} ParamInfo;

// Effect type enum (same as before)
typedef enum {
    EFFECT_DELAY = 0,
    EFFECT_OVERDRIVE,
    EFFECT_CHORUS,
    EFFECT_TREMOLO,
	EFFECT_FLANGER,
	EFFECT_PHASER,
	EFFECT_DISTORTION,
	EFFECT_AUTOWAH,
	EFFECT_COMPRESSOR,
	EFFECT_HALL_REVERB,
	EFFECT_PLATE_REVERB,
	EFFECT_SPRING_REVERB,
	EFFECT_SHIMMER_REVERB,
    EFFECT_COUNT
} EffectType;

// Effect instance structure
typedef struct {
    EffectType type;
    bool enabled;
    float params[MAX_PARAMS];
    bool params_dirty[MAX_PARAMS];  // Track which params changed - used to avoid thrashing setters every audio block
    char name[16];
} EffectInstance;

// Effect chain structure
typedef struct {
    EffectInstance instances[MAX_EFFECT_CHAIN];
    uint8_t count;
    uint8_t selected_instance; // For UI navigation
    uint8_t selected_param;    // For parameter editing
} EffectChain;

// MultiEffects Manager API
void MultiEffects_Init(void);
bool MultiEffects_AddEffect(EffectType type);
bool MultiEffects_RemoveEffect(uint8_t index);
bool MultiEffects_MoveEffect(uint8_t from_index, uint8_t to_index);
void MultiEffects_ToggleEffect(uint8_t index);
void MultiEffects_SetEffectParam(uint8_t instance_idx, uint8_t param_idx, float value);
void MultiEffects_ProcessAudio(float* inL, float* inR, float* outL, float* outR, uint16_t size);

// UI Navigation helpers
uint8_t MultiEffects_GetChainCount(void);
EffectInstance* MultiEffects_GetInstance(uint8_t index);
void MultiEffects_SetSelectedInstance(uint8_t index);
void MultiEffects_SetSelectedParam(uint8_t param);
uint8_t MultiEffects_GetSelectedInstance(void);
uint8_t MultiEffects_GetSelectedParam(void);

// Parameter conversion (from slider value 0..100 to actual units)
float MultiEffects_ConvertParam(EffectType type, uint8_t param_idx, uint32_t slider_val);
// Helper to find the slider position (0..100) that corresponds to an actual parameter value.
uint8_t MultiEffects_ActualToSlider(EffectType type, uint8_t param_idx, float actual_value);
int MultiEffects_FindEffectIndexByName(const char *name);

// Parameter info and formatting functions
const ParamInfo* MultiEffects_GetParamInfo(EffectType type, uint8_t param_idx);
void MultiEffects_FormatParamValue(EffectType type, uint8_t param_idx, int bar_value, char* buf, size_t buf_size);

extern const char* effect_names[EFFECT_COUNT];
#endif // MULTIEFFECTS_MANAGER_H