/**
  ******************************************************************************
  * @file    preset_manager.c
  * @brief   Preset management implementation
  ******************************************************************************
  */

#include "preset_manager.h"
#include <string.h>
#include <stdio.h>
#include "../guitarpedal/guitar_pedal.h"

/* Preset bank storage */
static PresetData preset_bank[MAX_PRESETS];
static uint8_t current_slot = 0xFF;  // 0xFF = no preset loaded

/* Configuration flags - what to include when saving */
static bool include_amp = true;
static bool include_cabinet = true;
static bool include_noise_gate = true;
static bool include_levels = true;

/* Forward declarations for factory presets */
static void CreateFactoryPresets(void);

/**
 * @brief Initialize the preset manager
 */
void PresetManager_Init(void)
{
    // Clear all preset slots
    memset(preset_bank, 0, sizeof(preset_bank));
    
    // Mark all slots as invalid initially
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        preset_bank[i].is_valid = false;
        snprintf(preset_bank[i].name, PRESET_NAME_LENGTH, "Empty %d", i + 1);
    }
    
    current_slot = 0xFF;
    
    // Create factory presets
    CreateFactoryPresets();
}

/**
 * @brief Save current configuration to a preset slot
 */
bool PresetManager_Save(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return false;
    }
    
    PresetData* preset = &preset_bank[slot];
    
    // Keep the existing name if it was set, otherwise generate default
    if (!preset->is_valid) {
        snprintf(preset->name, PRESET_NAME_LENGTH, "Preset %d", slot + 1);
    }
    
    // Save effect chain
    preset->effect_count = MultiEffects_GetChainCount();
    for (uint8_t i = 0; i < preset->effect_count && i < MAX_EFFECT_CHAIN; i++) {
        EffectInstance* instance = MultiEffects_GetInstance(i);
        if (instance) {
            preset->effects[i].type = instance->type;
            preset->effects[i].enabled = instance->enabled;
            for (uint8_t p = 0; p < MAX_PARAMS; p++) {
                preset->effects[i].params[p] = instance->params[p];
            }
        }
    }
    
    // Clear remaining effect slots
    for (uint8_t i = preset->effect_count; i < MAX_EFFECT_CHAIN; i++) {
        memset(&preset->effects[i], 0, sizeof(PresetEffectSlot));
    }
    
    // Save amp settings
    preset->include_amp = include_amp;
    if (include_amp) {
        preset->amp.enabled = GuitarPedal_IsAmpEnabled();
        preset->amp.model = GuitarPedal_GetAmpModel();
        preset->amp.gain = GuitarPedal_GetAmpGain();
        preset->amp.bass = GuitarPedal_GetAmpBass();
        preset->amp.mid = GuitarPedal_GetAmpMid();
        preset->amp.treble = GuitarPedal_GetAmpTreble();
        preset->amp.presence = GuitarPedal_GetAmpPresence();
        preset->amp.master = GuitarPedal_GetAmpMaster();
    }
    
    // Save cabinet settings
    preset->include_cabinet = include_cabinet;
    if (include_cabinet) {
        preset->cabinet.enabled = GuitarPedal_IsCabinetEnabled();
        preset->cabinet.cabinet_index = GuitarPedal_GetCabinet();
    }
    
    // Save noise gate settings
    preset->include_noise_gate = include_noise_gate;
    if (include_noise_gate) {
        preset->noise_gate.enabled = GuitarPedal_IsNoiseGateEnabled();
        preset->noise_gate.threshold_db = GuitarPedal_GetNoiseGateThreshold();
        preset->noise_gate.ratio = GuitarPedal_GetNoiseGateRatio();
    }
    
    // Save levels
    preset->include_levels = include_levels;
    if (include_levels) {
        preset->input_gain = GuitarPedal_GetInputGain();
        preset->output_level = GuitarPedal_GetOutputLevel();
    }
    
    preset->is_valid = true;
    current_slot = slot;
    
    return true;
}

/**
 * @brief Load a preset and apply it to the system
 */
bool PresetManager_Load(uint8_t slot)
{
    if (slot >= MAX_PRESETS || !preset_bank[slot].is_valid) {
        return false;
    }
    
    PresetData* preset = &preset_bank[slot];
    
    // Clear current effect chain
    while (MultiEffects_GetChainCount() > 0) {
        MultiEffects_RemoveEffect(0);
    }
    
    // Restore effect chain
    for (uint8_t i = 0; i < preset->effect_count && i < MAX_EFFECT_CHAIN; i++) {
        PresetEffectSlot* eff = &preset->effects[i];
        
        // Add the effect
        if (MultiEffects_AddEffect(eff->type)) {
            uint8_t idx = MultiEffects_GetChainCount() - 1;
            
            // Set enabled state
            EffectInstance* instance = MultiEffects_GetInstance(idx);
            if (instance) {
                instance->enabled = eff->enabled;
            }
            
            // Set all parameters
            for (uint8_t p = 0; p < MAX_PARAMS; p++) {
                MultiEffects_SetEffectParam(idx, p, eff->params[p]);
            }
        }
    }
    
    // Restore amp settings if included
    if (preset->include_amp) {
        GuitarPedal_EnableAmp(preset->amp.enabled);
        GuitarPedal_SetAmpModel(preset->amp.model);
        GuitarPedal_SetAmpGain(preset->amp.gain);
        GuitarPedal_SetAmpBass(preset->amp.bass);
        GuitarPedal_SetAmpMid(preset->amp.mid);
        GuitarPedal_SetAmpTreble(preset->amp.treble);
        GuitarPedal_SetAmpPresence(preset->amp.presence);
        GuitarPedal_SetAmpMaster(preset->amp.master);
    }
    
    // Restore cabinet settings if included
    if (preset->include_cabinet) {
        GuitarPedal_EnableCabinet(preset->cabinet.enabled);
        GuitarPedal_SetCabinet(preset->cabinet.cabinet_index);
    }
    
    // Restore noise gate settings if included
    if (preset->include_noise_gate) {
        GuitarPedal_EnableNoiseGate(preset->noise_gate.enabled);
        GuitarPedal_SetNoiseGateThreshold(preset->noise_gate.threshold_db);
        GuitarPedal_SetNoiseGateRatio(preset->noise_gate.ratio);
    }
    
    // Restore levels if included
    if (preset->include_levels) {
        GuitarPedal_SetInputGain(preset->input_gain);
        GuitarPedal_SetOutputLevel(preset->output_level);
    }
    
    current_slot = slot;
    
    return true;
}

/**
 * @brief Set preset name
 */
bool PresetManager_SetName(uint8_t slot, const char* name)
{
    if (slot >= MAX_PRESETS || name == NULL) {
        return false;
    }
    
    strncpy(preset_bank[slot].name, name, PRESET_NAME_LENGTH - 1);
    preset_bank[slot].name[PRESET_NAME_LENGTH - 1] = '\0';
    
    return true;
}

/**
 * @brief Get preset name
 */
const char* PresetManager_GetName(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return "Invalid";
    }
    
    if (!preset_bank[slot].is_valid) {
        return "Empty";
    }
    
    return preset_bank[slot].name;
}

/**
 * @brief Get current slot
 */
uint8_t PresetManager_GetCurrentSlot(void)
{
    return current_slot;
}

/**
 * @brief Check if slot is valid
 */
bool PresetManager_IsSlotValid(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return false;
    }
    return preset_bank[slot].is_valid;
}

/**
 * @brief Get total slot count
 */
uint8_t PresetManager_GetSlotCount(void)
{
    return MAX_PRESETS;
}

/**
 * @brief Get valid preset count
 */
uint8_t PresetManager_GetValidPresetCount(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (preset_bank[i].is_valid) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Clear a preset slot
 */
bool PresetManager_ClearSlot(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return false;
    }
    
    memset(&preset_bank[slot], 0, sizeof(PresetData));
    preset_bank[slot].is_valid = false;
    snprintf(preset_bank[slot].name, PRESET_NAME_LENGTH, "Empty %d", slot + 1);
    
    if (current_slot == slot) {
        current_slot = 0xFF;
    }
    
    return true;
}

/**
 * @brief Copy preset from one slot to another
 */
bool PresetManager_CopyPreset(uint8_t from_slot, uint8_t to_slot)
{
    if (from_slot >= MAX_PRESETS || to_slot >= MAX_PRESETS) {
        return false;
    }
    
    if (!preset_bank[from_slot].is_valid) {
        return false;
    }
    
    memcpy(&preset_bank[to_slot], &preset_bank[from_slot], sizeof(PresetData));
    
    // Update name to indicate it's a copy (truncate original if needed to fit " (Copy)")
    char new_name[PRESET_NAME_LENGTH + 7];  // Extra space for " (Copy)" suffix
    snprintf(new_name, PRESET_NAME_LENGTH + 7, "%.12s (Copy)", preset_bank[from_slot].name);
    strncpy(preset_bank[to_slot].name, new_name, PRESET_NAME_LENGTH - 1);
    preset_bank[to_slot].name[PRESET_NAME_LENGTH - 1] = '\0';
    
    return true;
}

/**
 * @brief Swap two presets
 */
bool PresetManager_SwapPresets(uint8_t slot_a, uint8_t slot_b)
{
    if (slot_a >= MAX_PRESETS || slot_b >= MAX_PRESETS) {
        return false;
    }
    
    PresetData temp;
    memcpy(&temp, &preset_bank[slot_a], sizeof(PresetData));
    memcpy(&preset_bank[slot_a], &preset_bank[slot_b], sizeof(PresetData));
    memcpy(&preset_bank[slot_b], &temp, sizeof(PresetData));
    
    // Update current slot if needed
    if (current_slot == slot_a) {
        current_slot = slot_b;
    } else if (current_slot == slot_b) {
        current_slot = slot_a;
    }
    
    return true;
}

/**
 * @brief Get preset data (read-only)
 */
const PresetData* PresetManager_GetPresetData(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return NULL;
    }
    return &preset_bank[slot];
}

/* Configuration setters/getters */
void PresetManager_SetIncludeAmp(bool include) { include_amp = include; }
void PresetManager_SetIncludeCabinet(bool include) { include_cabinet = include; }
void PresetManager_SetIncludeNoiseGate(bool include) { include_noise_gate = include; }
void PresetManager_SetIncludeLevels(bool include) { include_levels = include; }

bool PresetManager_GetIncludeAmp(void) { return include_amp; }
bool PresetManager_GetIncludeCabinet(void) { return include_cabinet; }
bool PresetManager_GetIncludeNoiseGate(void) { return include_noise_gate; }
bool PresetManager_GetIncludeLevels(void) { return include_levels; }

/**
 * @brief Reset all presets to factory defaults
 */
void PresetManager_ResetToFactory(void)
{
    // Clear all
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        PresetManager_ClearSlot(i);
    }
    
    // Recreate factory presets
    CreateFactoryPresets();
    
    current_slot = 0xFF;
}

/**
 * @brief Reset a single slot to factory (if it has a factory preset)
 */
bool PresetManager_ResetSlotToFactory(uint8_t slot)
{
    if (slot >= MAX_PRESETS) {
        return false;
    }
    
    // All 8 slots have factory presets
    
    // Clear and recreate by calling the factory setup
    PresetManager_ClearSlot(slot);
    
    // Re-run factory preset creation (it will only write valid ones)
    // This is a bit wasteful but keeps the code simple
    // In a real implementation, you might want separate factory preset definitions
    CreateFactoryPresets();
    
    return preset_bank[slot].is_valid;
}

/* ============================================================================
 * FACTORY PRESETS
 * ============================================================================
 * These are pre-configured presets that ship with the pedal.
 * Feel free to customize these to your taste!
 */

static void CreateFactoryPresets(void)
{
    PresetData* p;
    
    // ========================================================================
    // PRESET 0: Clean Sparkle
    // Light compression + chorus for a shimmering clean tone
    // ========================================================================
    p = &preset_bank[0];
    strncpy(p->name, "Clean Sparkle", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Compressor (light)
    p->effects[0].type = EFFECT_COMPRESSOR;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = -20.0f;  // threshold (dB)
    p->effects[0].params[1] = 3.0f;    // ratio
    p->effects[0].params[2] = 10.0f;   // attack (ms)
    p->effects[0].params[3] = 150.0f;  // release (ms)
    
    // Chorus
    p->effects[1].type = EFFECT_CHORUS;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 1.2f;    // rate (Hz)
    p->effects[1].params[1] = 0.4f;    // depth
    p->effects[1].params[2] = 0.35f;   // mix
    
    // Amp: Clean
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 0;       // Clean amp
    p->amp.gain = 0.3f;
    p->amp.bass = 0.5f;
    p->amp.mid = 0.5f;
    p->amp.treble = 0.6f;
    p->amp.presence = 0.5f;
    p->amp.master = 0.6f;
    
    // Cabinet
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 1;  // American Twin
    
    // No noise gate for clean
    p->include_noise_gate = true;
    p->noise_gate.enabled = false;
    
    // Levels
    p->include_levels = true;
    p->input_gain = 0.3f;
    p->output_level = 0.8f;
    
    // ========================================================================
    // PRESET 1: Blues Crunch
    // Overdrive with warm settings for blues playing
    // ========================================================================
    p = &preset_bank[1];
    strncpy(p->name, "Blues Crunch", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Compressor
    p->effects[0].type = EFFECT_COMPRESSOR;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = -25.0f;  // threshold
    p->effects[0].params[1] = 4.0f;    // ratio
    p->effects[0].params[2] = 5.0f;    // attack
    p->effects[0].params[3] = 100.0f;  // release
    
    // Overdrive
    p->effects[1].type = EFFECT_OVERDRIVE;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 4.0f;    // gain
    p->effects[1].params[1] = 0.6f;    // level
    p->effects[1].params[2] = 0.55f;   // tone (slightly warm)
    
    // Amp: Crunch
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 1;       // Crunch amp
    p->amp.gain = 0.5f;
    p->amp.bass = 0.55f;
    p->amp.mid = 0.6f;
    p->amp.treble = 0.5f;
    p->amp.presence = 0.45f;
    p->amp.master = 0.6f;
    
    // Cabinet
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 4;  // Brown Deluxe
    
    // Noise gate
    p->include_noise_gate = true;
    p->noise_gate.enabled = true;
    p->noise_gate.threshold_db = -45.0f;
    p->noise_gate.ratio = 5.0f;
    
    // Levels
    p->include_levels = true;
    p->input_gain = 0.35f;
    p->output_level = 0.75f;
    
    // ========================================================================
    // PRESET 2: Classic Rock
    // Marshall-style crunch with delay
    // ========================================================================
    p = &preset_bank[2];
    strncpy(p->name, "Classic Rock", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Overdrive
    p->effects[0].type = EFFECT_OVERDRIVE;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = 6.0f;    // gain
    p->effects[0].params[1] = 0.6f;    // level
    p->effects[0].params[2] = 0.5f;    // tone
    
    // Delay (slapback)
    p->effects[1].type = EFFECT_DELAY;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 180.0f;  // time (ms)
    p->effects[1].params[1] = 0.2f;    // feedback
    p->effects[1].params[2] = 0.6f;    // tone
    p->effects[1].params[3] = 0.25f;   // mix
    
    // Amp: British
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 2;       // High Gain / Marshall style
    p->amp.gain = 0.55f;
    p->amp.bass = 0.5f;
    p->amp.mid = 0.7f;
    p->amp.treble = 0.55f;
    p->amp.presence = 0.55f;
    p->amp.master = 0.65f;
    
    // Cabinet: British 4x12
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 3;
    
    // Noise gate
    p->include_noise_gate = true;
    p->noise_gate.enabled = true;
    p->noise_gate.threshold_db = -42.0f;
    p->noise_gate.ratio = 8.0f;
    
    // Levels
    p->include_levels = true;
    p->input_gain = 0.35f;
    p->output_level = 0.7f;
    
    // ========================================================================
    // PRESET 3: Heavy Metal
    // High gain distortion with tight response
    // ========================================================================
    p = &preset_bank[3];
    strncpy(p->name, "Heavy Metal", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Compressor (for tightness)
    p->effects[0].type = EFFECT_COMPRESSOR;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = -30.0f;
    p->effects[0].params[1] = 6.0f;
    p->effects[0].params[2] = 2.0f;
    p->effects[0].params[3] = 80.0f;
    
    // Distortion
    p->effects[1].type = EFFECT_DISTORTION;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 15.0f;   // gain
    p->effects[1].params[1] = 0.55f;   // level
    p->effects[1].params[2] = 0.45f;   // tone
    
    // Amp: High Gain
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 3;
    p->amp.gain = 0.75f;
    p->amp.bass = 0.6f;
    p->amp.mid = 0.45f;     // Scooped mids
    p->amp.treble = 0.6f;
    p->amp.presence = 0.65f;
    p->amp.master = 0.6f;
    
    // Cabinet: Modern 4x12
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 6;  // Modern Boutique
    
    // Tight noise gate
    p->include_noise_gate = true;
    p->noise_gate.enabled = true;
    p->noise_gate.threshold_db = -38.0f;
    p->noise_gate.ratio = 15.0f;
    
    // Levels
    p->include_levels = true;
    p->input_gain = 0.4f;
    p->output_level = 0.65f;
    
    // ========================================================================
    // PRESET 4: Ambient Clean
    // Clean with hall reverb and subtle modulation
    // ========================================================================
    p = &preset_bank[4];
    strncpy(p->name, "Ambient Clean", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 3;
    
    // Chorus (subtle)
    p->effects[0].type = EFFECT_CHORUS;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = 0.8f;    // rate
    p->effects[0].params[1] = 0.3f;    // depth
    p->effects[0].params[2] = 0.25f;   // mix
    
    // Delay
    p->effects[1].type = EFFECT_DELAY;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 350.0f;  // time
    p->effects[1].params[1] = 0.35f;   // feedback
    p->effects[1].params[2] = 0.7f;    // tone
    p->effects[1].params[3] = 0.3f;    // mix
    
    // Hall Reverb
    p->effects[2].type = EFFECT_HALL_REVERB;
    p->effects[2].enabled = true;
    p->effects[2].params[0] = 4.0f;    // time
    p->effects[2].params[1] = 0.4f;    // damping
    p->effects[2].params[2] = 30.0f;   // predelay
    p->effects[2].params[3] = 0.4f;    // mix
    
    // Amp: Clean
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 0;
    p->amp.gain = 0.25f;
    p->amp.bass = 0.45f;
    p->amp.mid = 0.5f;
    p->amp.treble = 0.55f;
    p->amp.presence = 0.5f;
    p->amp.master = 0.65f;
    
    // Cabinet
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 8;  // Vox AC30
    
    p->include_noise_gate = true;
    p->noise_gate.enabled = false;
    
    p->include_levels = true;
    p->input_gain = 0.3f;
    p->output_level = 0.75f;
    
    // ========================================================================
    // PRESET 5: Shimmer
    // Ethereal shimmer reverb with octave-up
    // ========================================================================
    p = &preset_bank[5];
    strncpy(p->name, "Shimmer", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Delay
    p->effects[0].type = EFFECT_DELAY;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = 400.0f;
    p->effects[0].params[1] = 0.4f;
    p->effects[0].params[2] = 0.65f;
    p->effects[0].params[3] = 0.3f;
    
    // Shimmer Reverb
    p->effects[1].type = EFFECT_SHIMMER_REVERB;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 5.0f;    // time
    p->effects[1].params[1] = 0.02f;   // depth
    p->effects[1].params[2] = 0.5f;    // rate
    p->effects[1].params[3] = 0.5f;    // mix
    
    // Amp: Clean
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 0;
    p->amp.gain = 0.2f;
    p->amp.bass = 0.4f;
    p->amp.mid = 0.5f;
    p->amp.treble = 0.6f;
    p->amp.presence = 0.55f;
    p->amp.master = 0.6f;
    
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 2;  // British Alnico
    
    p->include_noise_gate = true;
    p->noise_gate.enabled = false;
    
    p->include_levels = true;
    p->input_gain = 0.3f;
    p->output_level = 0.7f;
    
    // ========================================================================
    // PRESET 6: Funk Machine
    // Auto-wah with tight compression
    // ========================================================================
    p = &preset_bank[6];
    strncpy(p->name, "Funk Machine", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 2;
    
    // Compressor
    p->effects[0].type = EFFECT_COMPRESSOR;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = -22.0f;
    p->effects[0].params[1] = 5.0f;
    p->effects[0].params[2] = 3.0f;
    p->effects[0].params[3] = 60.0f;
    
    // Auto-Wah
    p->effects[1].type = EFFECT_AUTOWAH;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 0.7f;    // wah (resonance)
    p->effects[1].params[1] = 0.6f;    // level (sensitivity)
    p->effects[1].params[2] = 0.8f;    // mix
    p->effects[1].params[3] = 2.0f;    // output boost
    
    // Amp: Clean with slight grit
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 0;
    p->amp.gain = 0.4f;
    p->amp.bass = 0.55f;
    p->amp.mid = 0.6f;
    p->amp.treble = 0.5f;
    p->amp.presence = 0.45f;
    p->amp.master = 0.65f;
    
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 7;  // Tweed Combo
    
    p->include_noise_gate = true;
    p->noise_gate.enabled = true;
    p->noise_gate.threshold_db = -48.0f;
    p->noise_gate.ratio = 4.0f;
    
    p->include_levels = true;
    p->input_gain = 0.35f;
    p->output_level = 0.7f;
    
    // ========================================================================
    // PRESET 7: 80s Lead
    // Phaser + delay + chorus for that 80s vibe
    // ========================================================================
    p = &preset_bank[7];
    strncpy(p->name, "80s Lead", PRESET_NAME_LENGTH - 1);
    p->is_valid = true;
    p->effect_count = 4;
    
    // Overdrive
    p->effects[0].type = EFFECT_OVERDRIVE;
    p->effects[0].enabled = true;
    p->effects[0].params[0] = 5.5f;
    p->effects[0].params[1] = 0.55f;
    p->effects[0].params[2] = 0.55f;
    
    // Chorus
    p->effects[1].type = EFFECT_CHORUS;
    p->effects[1].enabled = true;
    p->effects[1].params[0] = 1.5f;
    p->effects[1].params[1] = 0.45f;
    p->effects[1].params[2] = 0.35f;
    
    // Delay
    p->effects[2].type = EFFECT_DELAY;
    p->effects[2].enabled = true;
    p->effects[2].params[0] = 320.0f;
    p->effects[2].params[1] = 0.35f;
    p->effects[2].params[2] = 0.6f;
    p->effects[2].params[3] = 0.3f;
    
    // Plate Reverb
    p->effects[3].type = EFFECT_PLATE_REVERB;
    p->effects[3].enabled = true;
    p->effects[3].params[0] = 2.0f;
    p->effects[3].params[1] = 0.5f;
    p->effects[3].params[2] = 0.6f;
    p->effects[3].params[3] = 0.3f;
    
    // Amp
    p->include_amp = true;
    p->amp.enabled = true;
    p->amp.model = 1;
    p->amp.gain = 0.55f;
    p->amp.bass = 0.5f;
    p->amp.mid = 0.65f;
    p->amp.treble = 0.55f;
    p->amp.presence = 0.6f;
    p->amp.master = 0.6f;
    
    p->include_cabinet = true;
    p->cabinet.enabled = true;
    p->cabinet.cabinet_index = 11;  // Vintage Marshall
    
    p->include_noise_gate = true;
    p->noise_gate.enabled = true;
    p->noise_gate.threshold_db = -44.0f;
    p->noise_gate.ratio = 6.0f;
    
    p->include_levels = true;
    p->input_gain = 0.35f;
    p->output_level = 0.7f;
}
