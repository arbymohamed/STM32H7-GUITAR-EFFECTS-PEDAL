/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    guitar_pedal.h
  * @brief   Simple guitar pedal passthrough for STM32H743 + PCM3060
  *          WITH AMP SIMULATION
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __GUITAR_PEDAL_H
#define __GUITAR_PEDAL_H

#include "main.h"
#include <stdint.h>
#include "looper.h"

/* Guitar pedal configuration */
#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_BUFFER_SIZE       512      // Small buffer for low latency (~1.3ms)
#define AUDIO_CHANNELS          2       // Stereo (but we'll use mono input)



/* Audio sample format - 32-bit container for 24-bit data */
typedef struct {
    int32_t left;
    int32_t right;
} AudioSample_t;

/* Function prototypes */
void GuitarPedal_Init(void);
void GuitarPedal_Start(void);
void GuitarPedal_Stop(void);
void GuitarPedal_ProcessAudio(AudioSample_t* input, AudioSample_t* output, uint16_t size);
void GuitarPedal_GetStatus(uint32_t* rx_count, uint32_t* tx_count, uint32_t* err_count, uint8_t* ready);

/* Cabinet simulation control */
void GuitarPedal_EnableCabinet(uint8_t enable);      // Enable/disable cabinet simulation
uint8_t GuitarPedal_IsCabinetEnabled(void);          // Check if cabinet is enabled
void GuitarPedal_SetCabinet(uint8_t cabinet_index);  // Set active cabinet (0-15)
uint8_t GuitarPedal_GetCabinet(void);                // Get current cabinet index
const char* GuitarPedal_GetCabinetName(uint8_t cabinet_index);  // Get cabinet name by index
const char* GuitarPedal_GetCurrentCabinetName(void); // Get current cabinet name

void AudioSampleToFloat(const AudioSample_t* in, float* out, size_t len);
void FloatToAudioSample(const float* in, AudioSample_t* out, size_t len);
float s242f(int32_t x);
int32_t f2s24(float x);

/* Bypass control */
void GuitarPedal_EnableBypass(bool enable);      // Enable/disable bypass

/* Looper level control */
void GuitarPedal_SetLooperLevel(float level);  // Set looper mix level (0.0 to 1.0)
float GuitarPedal_GetLooperLevel(void);        // Get current looper mix level

/* Output level control */
void GuitarPedal_SetOutputLevel(float level);  // Set output level (0.0 to 1.0)
float GuitarPedal_GetOutputLevel(void);        // Get current output level
float GuitarPedal_GetOutputLevelDb(void);      // Get output level in dB for meter (-60 to +6)

/* Input gain control */
void GuitarPedal_SetInputGain(float gain);     // Set input gain (0.0 to 1.0)
float GuitarPedal_GetInputGain(void);          // Get current input gain

/* ===== AMP SIMULATION CONTROL (NEW) ===== */
void GuitarPedal_EnableAmp(bool enable);              // Enable/disable amp simulation
bool GuitarPedal_IsAmpEnabled(void);                  // Check if amp is enabled
void GuitarPedal_SetAmpModel(uint8_t model);          // Set amp model (0-4)
uint8_t GuitarPedal_GetAmpModel(void);                // Get current amp model
const char* GuitarPedal_GetAmpModelName(uint8_t model); // Get amp model name
void GuitarPedal_SetAmpGain(float gain);              // Set amp gain (0.0 to 1.0)
void GuitarPedal_SetAmpBass(float bass);              // Set amp bass (0.0 to 1.0, 0.5 = neutral)
void GuitarPedal_SetAmpMid(float mid);                // Set amp mid (0.0 to 1.0, 0.5 = neutral)
void GuitarPedal_SetAmpTreble(float treble);          // Set amp treble (0.0 to 1.0, 0.5 = neutral)
void GuitarPedal_SetAmpPresence(float presence);      // Set amp presence (0.0 to 1.0)
void GuitarPedal_SetAmpMaster(float master);          // Set amp master (0.0 to 1.0)

/* Amp parameter getters (for display) */
float GuitarPedal_GetAmpGain(void);                   // Get current amp gain
float GuitarPedal_GetAmpBass(void);                   // Get current amp bass
float GuitarPedal_GetAmpMid(void);                    // Get current amp mid
float GuitarPedal_GetAmpTreble(void);                 // Get current amp treble
float GuitarPedal_GetAmpPresence(void);               // Get current amp presence
float GuitarPedal_GetAmpMaster(void);                 // Get current amp master

/* ===== NOISE GATE CONTROL ===== */
void GuitarPedal_EnableNoiseGate(uint8_t enable);           // Enable/disable noise gate
uint8_t GuitarPedal_IsNoiseGateEnabled(void);               // Check if gate is enabled
void GuitarPedal_SetNoiseGateThreshold(float threshold_db); // Set threshold in dB (-60 to 0)
float GuitarPedal_GetNoiseGateThreshold(void);              // Get threshold in dB
void GuitarPedal_SetNoiseGateAttack(float attack_ms);       // Set attack time (0.1 to 50ms)
void GuitarPedal_SetNoiseGateRelease(float release_ms);     // Set release time (10 to 500ms)
void GuitarPedal_SetNoiseGateRatio(float ratio);            // Set expansion ratio (1.0 to 100.0)
float GuitarPedal_GetNoiseGateRatio(void);                  // Get expansion ratio

#endif /* __GUITAR_PEDAL_H */
