/**
  ******************************************************************************
  * @file    preset_manager.h
  * @brief   Preset management for guitar pedal - save/load complete configurations
  ******************************************************************************
  */

#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../effectsmanager/multieffects_manager.h"

/* Configuration */
#define MAX_PRESETS         8
#define PRESET_NAME_LENGTH  20

/* Preset effect slot - mirrors EffectInstance but without runtime state */
typedef struct {
    EffectType type;
    bool enabled;
    float params[MAX_PARAMS];
} PresetEffectSlot;

/* Amp settings snapshot */
typedef struct {
    bool enabled;
    uint8_t model;          // Amp model (0-4)
    float gain;
    float bass;
    float mid;
    float treble;
    float presence;
    float master;
} PresetAmpSettings;

/* Cabinet settings snapshot */
typedef struct {
    bool enabled;
    uint8_t cabinet_index;  // Cabinet IR index (0-15)
} PresetCabinetSettings;

/* Noise gate settings snapshot */
typedef struct {
    bool enabled;
    float threshold_db;
    float ratio;
} PresetNoiseGateSettings;

/* Complete preset data structure */
typedef struct {
    char name[PRESET_NAME_LENGTH];
    
    /* Effect chain */
    PresetEffectSlot effects[MAX_EFFECT_CHAIN];
    uint8_t effect_count;
    
    /* Amp simulation */
    PresetAmpSettings amp;
    
    /* Cabinet simulation */
    PresetCabinetSettings cabinet;
    
    /* Noise gate */
    PresetNoiseGateSettings noise_gate;
    
    /* Input/Output levels */
    float input_gain;
    float output_level;
    
    /* Flags for what to restore */
    bool include_amp;
    bool include_cabinet;
    bool include_noise_gate;
    bool include_levels;
    
    /* Validity marker */
    bool is_valid;
} PresetData;

/* Preset Manager API */

/**
 * @brief Initialize the preset manager with factory presets
 */
void PresetManager_Init(void);

/**
 * @brief Save current configuration to a preset slot
 * @param slot Preset slot index (0 to MAX_PRESETS-1)
 * @return true if saved successfully
 */
bool PresetManager_Save(uint8_t slot);

/**
 * @brief Load a preset and apply it to the effect chain
 * @param slot Preset slot index (0 to MAX_PRESETS-1)
 * @return true if loaded successfully
 */
bool PresetManager_Load(uint8_t slot);

/**
 * @brief Set the name of a preset
 * @param slot Preset slot index
 * @param name Preset name (will be truncated if too long)
 * @return true if successful
 */
bool PresetManager_SetName(uint8_t slot, const char* name);

/**
 * @brief Get the name of a preset
 * @param slot Preset slot index
 * @return Pointer to preset name, or "Empty" if invalid
 */
const char* PresetManager_GetName(uint8_t slot);

/**
 * @brief Get the currently loaded preset slot
 * @return Current slot index, or 0xFF if no preset loaded
 */
uint8_t PresetManager_GetCurrentSlot(void);

/**
 * @brief Check if a preset slot contains valid data
 * @param slot Preset slot index
 * @return true if slot has valid preset data
 */
bool PresetManager_IsSlotValid(uint8_t slot);

/**
 * @brief Get total number of preset slots
 * @return Number of available preset slots
 */
uint8_t PresetManager_GetSlotCount(void);

/**
 * @brief Get number of valid (non-empty) presets
 * @return Count of valid presets
 */
uint8_t PresetManager_GetValidPresetCount(void);

/**
 * @brief Clear a preset slot
 * @param slot Preset slot index
 * @return true if cleared successfully
 */
bool PresetManager_ClearSlot(uint8_t slot);

/**
 * @brief Copy a preset from one slot to another
 * @param from_slot Source slot index
 * @param to_slot Destination slot index
 * @return true if copied successfully
 */
bool PresetManager_CopyPreset(uint8_t from_slot, uint8_t to_slot);

/**
 * @brief Swap two presets
 * @param slot_a First slot index
 * @param slot_b Second slot index
 * @return true if swapped successfully
 */
bool PresetManager_SwapPresets(uint8_t slot_a, uint8_t slot_b);

/**
 * @brief Get direct access to preset data (read-only, for UI display)
 * @param slot Preset slot index
 * @return Pointer to PresetData, or NULL if invalid slot
 */
const PresetData* PresetManager_GetPresetData(uint8_t slot);

/* Configuration - what to include when saving */

/**
 * @brief Set whether to include amp settings in presets
 */
void PresetManager_SetIncludeAmp(bool include);

/**
 * @brief Set whether to include cabinet settings in presets
 */
void PresetManager_SetIncludeCabinet(bool include);

/**
 * @brief Set whether to include noise gate settings in presets
 */
void PresetManager_SetIncludeNoiseGate(bool include);

/**
 * @brief Set whether to include input/output levels in presets
 */
void PresetManager_SetIncludeLevels(bool include);

/**
 * @brief Get include amp setting
 */
bool PresetManager_GetIncludeAmp(void);

/**
 * @brief Get include cabinet setting
 */
bool PresetManager_GetIncludeCabinet(void);

/**
 * @brief Get include noise gate setting
 */
bool PresetManager_GetIncludeNoiseGate(void);

/**
 * @brief Get include levels setting
 */
bool PresetManager_GetIncludeLevels(void);

/* Factory presets */

/**
 * @brief Reset all presets to factory defaults
 */
void PresetManager_ResetToFactory(void);

/**
 * @brief Reset a single slot to its factory preset (if it has one)
 * @param slot Preset slot index
 * @return true if factory preset existed and was restored
 */
bool PresetManager_ResetSlotToFactory(uint8_t slot);

#endif /* PRESET_MANAGER_H */
