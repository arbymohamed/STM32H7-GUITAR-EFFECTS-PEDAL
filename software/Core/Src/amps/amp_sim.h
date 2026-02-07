/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    amp_sim.h
  * @brief   Guitar amplifier simulation for STM32H743
  *          Provides multiple amp models with tone controls
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __AMP_SIM_H
#define __AMP_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Amp model types */
typedef enum {
    AMP_MODEL_CLEAN,        // Clean Fender-style amp
    AMP_MODEL_CRUNCH,       // British crunch (Marshall-style)
    AMP_MODEL_LEAD,         // High-gain lead (Mesa-style)
    AMP_MODEL_BLUES,        // Warm blues tone
    AMP_MODEL_METAL,        // Modern high-gain metal
    AMP_MODEL_COUNT         // Total number of models
} AmpModel_t;

/* Amp simulation structure */
typedef struct {
    AmpModel_t model;
    
    /* Tone controls (0.0 to 1.0) */
    float gain;             // Pre-amp gain
    float bass;             // Bass EQ
    float mid;              // Mid EQ
    float treble;           // Treble EQ
    float presence;         // Presence/air
    float master;           // Master volume
    
    /* Internal state */
    bool enabled;
    
    /* EQ filter states (biquad filters) */
    float bass_z1, bass_z2;
    float mid_z1, mid_z2;
    float treble_z1, treble_z2;
    float presence_z1, presence_z2;
    
    /* Tube-style soft clipping state */
    float dc_block_in_z1;   // DC blocking filter input history
    float dc_block_out_z1;  // DC blocking filter output history
    
    /* Cached filter coefficients (calculated only when tone changes) */
    float bass_b0, bass_b1, bass_b2, bass_a1, bass_a2;
    float mid_b0, mid_b1, mid_b2, mid_a1, mid_a2;
    float treble_b0, treble_b1, treble_b2, treble_a1, treble_a2;
    float presence_b0, presence_b1, presence_b2, presence_a1, presence_a2;
    
    /* Previous tone settings to detect changes */
    float last_bass, last_mid, last_treble, last_presence;
    
    /* Sample rate */
    uint32_t sample_rate;
} AmpSim_t;

/* Function prototypes */

/**
 * @brief Initialize amp simulation
 * @param amp Pointer to amp simulation structure
 * @param sample_rate Audio sample rate in Hz
 */
void AmpSim_Init(AmpSim_t* amp, uint32_t sample_rate);

/**
 * @brief Process audio through amp simulation
 * @param amp Pointer to amp simulation structure
 * @param input Input buffer (mono)
 * @param output Output buffer (mono)
 * @param size Number of samples to process
 */
void AmpSim_Process(AmpSim_t* amp, const float* input, float* output, size_t size);

/**
 * @brief Set amp model
 * @param amp Pointer to amp simulation structure
 * @param model Amp model to use
 */
void AmpSim_SetModel(AmpSim_t* amp, AmpModel_t model);

/**
 * @brief Get current amp model
 * @param amp Pointer to amp simulation structure
 * @return Current amp model
 */
AmpModel_t AmpSim_GetModel(const AmpSim_t* amp);

/**
 * @brief Enable/disable amp simulation
 * @param amp Pointer to amp simulation structure
 * @param enable true to enable, false to bypass
 */
void AmpSim_Enable(AmpSim_t* amp, bool enable);

/**
 * @brief Check if amp is enabled
 * @param amp Pointer to amp simulation structure
 * @return true if enabled, false if bypassed
 */
bool AmpSim_IsEnabled(const AmpSim_t* amp);

/**
 * @brief Set gain control (pre-amp drive)
 * @param amp Pointer to amp simulation structure
 * @param gain Gain value (0.0 to 1.0)
 */
void AmpSim_SetGain(AmpSim_t* amp, float gain);

/**
 * @brief Set bass EQ control
 * @param amp Pointer to amp simulation structure
 * @param bass Bass value (0.0 to 1.0, 0.5 = neutral)
 */
void AmpSim_SetBass(AmpSim_t* amp, float bass);

/**
 * @brief Set mid EQ control
 * @param amp Pointer to amp simulation structure
 * @param mid Mid value (0.0 to 1.0, 0.5 = neutral)
 */
void AmpSim_SetMid(AmpSim_t* amp, float mid);

/**
 * @brief Set treble EQ control
 * @param amp Pointer to amp simulation structure
 * @param treble Treble value (0.0 to 1.0, 0.5 = neutral)
 */
void AmpSim_SetTreble(AmpSim_t* amp, float treble);

/**
 * @brief Set presence control (high-frequency air)
 * @param amp Pointer to amp simulation structure
 * @param presence Presence value (0.0 to 1.0)
 */
void AmpSim_SetPresence(AmpSim_t* amp, float presence);

/**
 * @brief Set master volume
 * @param amp Pointer to amp simulation structure
 * @param master Master volume (0.0 to 1.0)
 */
void AmpSim_SetMaster(AmpSim_t* amp, float master);

/**
 * @brief Get amp model name as string
 * @param model Amp model
 * @return Name of the amp model
 */
const char* AmpSim_GetModelName(AmpModel_t model);

/**
 * @brief Get current gain value
 * @param amp Pointer to amp simulation structure
 * @return Current gain (0.0 to 1.0)
 */
float AmpSim_GetGain(const AmpSim_t* amp);

/**
 * @brief Get current bass value
 * @param amp Pointer to amp simulation structure
 * @return Current bass (0.0 to 1.0)
 */
float AmpSim_GetBass(const AmpSim_t* amp);

/**
 * @brief Get current mid value
 * @param amp Pointer to amp simulation structure
 * @return Current mid (0.0 to 1.0)
 */
float AmpSim_GetMid(const AmpSim_t* amp);

/**
 * @brief Get current treble value
 * @param amp Pointer to amp simulation structure
 * @return Current treble (0.0 to 1.0)
 */
float AmpSim_GetTreble(const AmpSim_t* amp);

/**
 * @brief Get current presence value
 * @param amp Pointer to amp simulation structure
 * @return Current presence (0.0 to 1.0)
 */
float AmpSim_GetPresence(const AmpSim_t* amp);

/**
 * @brief Get current master volume
 * @param amp Pointer to amp simulation structure
 * @return Current master (0.0 to 1.0)
 */
float AmpSim_GetMaster(const AmpSim_t* amp);

#endif /* __AMP_SIM_H */
