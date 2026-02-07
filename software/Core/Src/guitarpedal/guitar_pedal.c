/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    guitar_pedal.c
  * @brief   Guitar pedal passthrough - STM32H743 + PCM3060
  *          WITH AMP SIMULATION INTEGRATED
  ******************************************************************************
  */
/* USER CODE END Header */

#include <guitar_pedal.h>
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <math.h>
#include "looper.h"
#include "main.h"
#include "amp_sim.h"  // ADD THIS - Amp simulation module

#include "../effectsmanager/multieffects_manager.h"
#define ARM_MATH_CM7
#include "arm_math.h"

// Include Origin Effects cabinet IRs (8 total - 256 taps each)
#include "cabinet_ir_256.h"
#include "cabinet_ir_american_twin.h"
#include "cabinet_ir_british_alnico.h"
#include "cabinet_ir_british_4x12.h"
#include "cabinet_ir_brown_deluxe.h"
#include "cabinet_ir_magma_vintage.h"
#include "cabinet_ir_modern_boutique.h"
#include "cabinet_ir_tweed_combo.h"
#include "cabinet_ir_ac30_vox.h"
#include "cabinet_ir_jcm900.h"
#include "cabinet_ir_orange.h"
#include "cabinet_ir_vintage_marshall.h"
#include "cabinet_ir_jmp_plexi.h"
#include "cabinet_ir_guitarhack_edge.h"
#include "cabinet_ir_fair_fredman.h"
#include "cabinet_ir_london_city.h"

extern Looper looper; // Reference global instance from looper.c

/* External SAI handles from main.c */
extern SAI_HandleTypeDef hsai_BlockA1;  // Output (DAC)
extern SAI_HandleTypeDef hsai_BlockB1;  // Input (ADC)

#define FBIPMAX 0.999985f             /**< close to 1.0f-LSB at 16 bit */
#define FBIPMIN (-FBIPMAX)            /**< - (1 - LSB) */
#define U82F_SCALE 0.0078740f         /**< 1 / 127 */
#define F2U8_SCALE 127.0f             /**< 128 - 1 */
#define S82F_SCALE 0.0078125f         /**< 1 / (2**7) */
#define F2S8_SCALE 127.0f             /**< (2 ** 7) - 1 */
#define S162F_SCALE 3.0517578125e-05f /**< 1 / (2** 15) */
#define F2S16_SCALE 32767.0f          /**< (2 ** 15) - 1 */
#define F2S24_SCALE 8388608.0f        /**< 2 ** 23 */
#define S242F_SCALE 1.192092896e-07f  /**< 1 / (2 ** 23) */
#define S24SIGN 0x800000              /**< 2 ** 23 */
#define S322F_SCALE 4.6566129e-10f    /**< 1 / (2** 31) */
#define F2S32_SCALE 2147483647.f      /**< (2 ** 31) - 1 */

/* Audio buffers - small for low latency */
/* place buffers in normal RAM but aligned for cache ops */
__attribute__((section(".adcarray"), aligned(32))) static AudioSample_t audio_input_buffer[AUDIO_BUFFER_SIZE];
__attribute__((section(".adcarray"), aligned(32))) static AudioSample_t audio_output_buffer[AUDIO_BUFFER_SIZE];

/* Static float buffers for audio processing */
__attribute__((section(".adcarray"), aligned(32))) static float in_left[AUDIO_BUFFER_SIZE];
__attribute__((section(".adcarray"), aligned(32))) static float in_right[AUDIO_BUFFER_SIZE];
__attribute__((section(".adcarray"), aligned(32))) static float out_left[AUDIO_BUFFER_SIZE];
__attribute__((section(".adcarray"), aligned(32))) static float out_right[AUDIO_BUFFER_SIZE];

/* Status variables */
static volatile uint32_t rx_callback_count = 0;
static volatile uint32_t tx_callback_count = 0;
static volatile uint32_t error_count = 0;
static volatile uint8_t audio_ready = 0;
float looper_mix = 1.0f; // Range: 0.0 (dry only) to 1.0 (looper only)

static float input_gain = 0.3f; // Reduced from 0.5 - prevents clipping with hot pickups
static float output_level = 1.0f; // Full output - amp is now properly gain-staged

/* Output level meter (peak detection with decay) */
static volatile float output_peak_level = 0.0f;  // Current peak level (0.0 to 1.0+)
static const float peak_decay_coeff = 0.995f;    // Fast decay (~10ms at 48kHz)

/* Noise Gate */
typedef struct {
    float threshold;     // Threshold in linear amplitude (0.0 to 1.0)
    float attack_coeff;  // Attack time coefficient
    float release_coeff; // Release time coefficient
    float ratio;         // Expansion ratio (1.0 = off, higher = more aggressive)
    uint8_t enabled;     // Gate on/off
    float envelope_left; // Current envelope for left channel
    float envelope_right;// Current envelope for right channel
} NoiseGate_t;

static NoiseGate_t noise_gate = {
    .threshold = 0.01f,      // -40dB default threshold
    .attack_coeff = 0.0f,    // Will be calculated in init
    .release_coeff = 0.0f,   // Will be calculated in init
    .ratio = 10.0f,          // 10:1 expansion ratio (pretty aggressive)
    .enabled = 0,            // Disabled by default
    .envelope_left = 0.0f,
    .envelope_right = 0.0f
};

/* ADD THIS - Amp simulation instance (stereo - left and right) */
static AmpSim_t amp_sim_left;
static AmpSim_t amp_sim_right;

/* Cabinet simulation using ARM CMSIS-DSP (PROPER IMPLEMENTATION) */
#define CABINET_IR_LENGTH 256
#define CABINET_STATE_SIZE (CABINET_IR_LENGTH + AUDIO_BUFFER_SIZE - 1)
#define CABINET_COUNT 16

static uint8_t cabinet_enabled = 0;  // DISABLED by default
static uint8_t current_cabinet = 0;  // Currently selected cabinet (0-15)

// ARM CMSIS-DSP FIR filter instances
static arm_fir_instance_f32 fir_left;
static arm_fir_instance_f32 fir_right;

// State buffers (size must be IR_LENGTH + BLOCK_SIZE - 1)
static float cabinet_state_left[CABINET_STATE_SIZE];
static float cabinet_state_right[CABINET_STATE_SIZE];

// Array of pointers to all 16 cabinet IRs
static const float* cabinet_ir_array[CABINET_COUNT] = {
    // Origin Effects IR Cab Library (0-7)
    ir_1960_g12m25_sm57_cap_0in,           // 0: Marshall 1960 (Origin)
    ir_american_twin_2x12_medium_87,       // 1: American Twin (Origin)
    ir_british_alnico_2x12_medium_87,      // 2: British Alnico (Origin)
    ir_british_straight_4x12_bright_57,    // 3: British 4x12 (Origin)
    ir_brown_deluxe_1x12_medium_57,        // 4: Brown Deluxe (Origin)
    ir_magma_vintage_1x12_medium_87,       // 5: Magma Vintage (Origin)
    ir_modern_boutique_4x12_medium_57,     // 6: Modern Boutique (Origin)
    ir_tweed_combo_1x12_medium_57,         // 7: Tweed Combo (Origin)
    ir_ac30_brilliant_bx44_neve_close_dc,  // 8: Vox AC30 Brilliant
    ir_jcm900_1_sm57,                       // 9: Marshall JCM900
    ir_orange_sm57,                         // 10: Orange
    ir_marshall_1_impact,                   // 11: Vintage Marshall
    ir_marshall_sm57_on_axis,               // 12: JMP Plexi
    ir_guitarhack_original_edge,            // 13: GuitarHack Edge
    ir_fair_mr_iamthefredman_tube,          // 14: faIR Fredman
    ir_lc100_sm57_stalevel_dc               // 15: London City
};

// bool or static bool ?
static bool bypass_enabled = false; // Bypass flag

// Cabinet names for debugging/UI
static const char* cabinet_names[CABINET_COUNT] = {
    // Origin Effects (0-7)
    "Marshall 1960",
    "American Twin",
    "British Alnico",
    "British 4x12",
    "Brown Deluxe",
    "Magma Vintage",
    "Modern Boutique",
    "Tweed Combo",
    "Vox AC30",
    "JCM900",
    "Orange",
    "Vintage Marshall",
    "JMP Plexi",
    "GuitarHack Edge",
    "faIR Fredman",
    "London City"
};

/**
  * @brief  Fast soft clipping using rational function approximation of tanh
  * @param  x: Input sample
  * @retval Soft-clipped output in range [-1.0, 1.0]
  * @note   This is much more efficient than tanhf() and avoids harsh distortion
  */
static inline float soft_clip(float x) {
    // Hard limit at extremes for safety
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;

    // Rational function approximation: x(27 + x²) / (27 + 9x²)
    // This closely approximates tanh(x) and is very efficient
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
  * @brief  Initialize cabinet simulation FIR filters (ARM CMSIS-DSP)
  */
static void Cabinet_Init(void)
{
    // Initialize ARM FIR filters for left and right channels
    // NOTE: arm_fir_init_f32 parameters:
    //   - instance: pointer to FIR structure
    //   - numTaps: length of the filter (IR length)
    //   - pCoeffs: pointer to filter coefficients
    //   - pState: pointer to state buffer
    //   - blockSize: number of samples to process per call

    arm_fir_init_f32(&fir_left,
                     CABINET_IR_LENGTH,
                     (float32_t*)cabinet_ir_array[current_cabinet],
                     cabinet_state_left,
                     AUDIO_BUFFER_SIZE / 2);  // We process half-buffers

    arm_fir_init_f32(&fir_right,
                     CABINET_IR_LENGTH,
                     (float32_t*)cabinet_ir_array[current_cabinet],
                     cabinet_state_right,
                     AUDIO_BUFFER_SIZE / 2);  // We process half-buffers
}

/**
  * @brief  Enable or disable cabinet simulation
  * @param  enable: 1 to enable, 0 to disable
  */
void GuitarPedal_EnableCabinet(uint8_t enable)
{
    cabinet_enabled = enable ? 1 : 0;

    // Clear state buffers when toggling
    if (!cabinet_enabled) {
        memset(cabinet_state_left, 0, sizeof(cabinet_state_left));
        memset(cabinet_state_right, 0, sizeof(cabinet_state_right));
    }
}

/**
  * @brief  Check if cabinet simulation is enabled
  * @retval 1 if enabled, 0 if disabled
  */
uint8_t GuitarPedal_IsCabinetEnabled(void)
{
    return cabinet_enabled;
}

/**
  * @brief  Set the active cabinet type
  * @param  cabinet_index: Cabinet index (0-15)
  */
void GuitarPedal_SetCabinet(uint8_t cabinet_index)
{
    if (cabinet_index < CABINET_COUNT) {
        current_cabinet = cabinet_index;

        // Reinitialize FIR filters with new IR
        arm_fir_init_f32(&fir_left,
                         CABINET_IR_LENGTH,
                         (float32_t*)cabinet_ir_array[cabinet_index],
                         cabinet_state_left,
                         AUDIO_BUFFER_SIZE / 2);

        arm_fir_init_f32(&fir_right,
                         CABINET_IR_LENGTH,
                         (float32_t*)cabinet_ir_array[cabinet_index],
                         cabinet_state_right,
                         AUDIO_BUFFER_SIZE / 2);

        // Clear state buffers to prevent artifacts
        memset(cabinet_state_left, 0, sizeof(cabinet_state_left));
        memset(cabinet_state_right, 0, sizeof(cabinet_state_right));
    }
}

/**
  * @brief  Get the current cabinet type
  * @retval Current cabinet index (0-15)
  */
uint8_t GuitarPedal_GetCabinet(void)
{
    return current_cabinet;
}

/**
  * @brief  Get cabinet name by index
  * @param  cabinet_index: Cabinet index (0-15)
  * @retval Pointer to cabinet name string
  */
const char* GuitarPedal_GetCabinetName(uint8_t cabinet_index)
{
    if (cabinet_index < CABINET_COUNT) {
        return cabinet_names[cabinet_index];
    }
    return "Unknown";
}

/**
  * @brief  Get current cabinet name
  * @retval Pointer to current cabinet name string
  */
const char* GuitarPedal_GetCurrentCabinetName(void)
{
    return cabinet_names[current_cabinet];
}

/**
  * @brief  Initialize guitar pedal
  */
void GuitarPedal_Init(void)
{
    // Initialize cabinet simulation
    Cabinet_Init();

    // Initialize noise gate time constants
    // Attack: 1ms, Release: 50ms (typical for guitar)
    float attack_time = 0.001f;  // 1ms
    float release_time = 0.050f; // 50ms
    noise_gate.attack_coeff = expf(-1.0f / (attack_time * AUDIO_SAMPLE_RATE));
    noise_gate.release_coeff = expf(-1.0f / (release_time * AUDIO_SAMPLE_RATE));

    // ADD THIS - Initialize amp simulation for both channels
    AmpSim_Init(&amp_sim_left, AUDIO_SAMPLE_RATE);
    AmpSim_Init(&amp_sim_right, AUDIO_SAMPLE_RATE);

    // Set default amp settings (both channels get same settings)
    AmpSim_SetModel(&amp_sim_left, AMP_MODEL_CRUNCH);
    AmpSim_SetModel(&amp_sim_right, AMP_MODEL_CRUNCH);

    AmpSim_SetGain(&amp_sim_left, 0.5f);
    AmpSim_SetGain(&amp_sim_right, 0.5f);

    AmpSim_SetBass(&amp_sim_left, 0.5f);
    AmpSim_SetBass(&amp_sim_right, 0.5f);

    AmpSim_SetMid(&amp_sim_left, 0.5f);
    AmpSim_SetMid(&amp_sim_right, 0.5f);

    AmpSim_SetTreble(&amp_sim_left, 0.5f);
    AmpSim_SetTreble(&amp_sim_right, 0.5f);

    AmpSim_SetPresence(&amp_sim_left, 0.3f);
    AmpSim_SetPresence(&amp_sim_right, 0.3f);

    AmpSim_SetMaster(&amp_sim_left, 0.7f);
    AmpSim_SetMaster(&amp_sim_right, 0.7f);
}

/**
  * @brief  Start guitar pedal audio processing
  */
void GuitarPedal_Start(void)
{
	memset(audio_output_buffer, 0, sizeof(audio_output_buffer));
    HAL_SAI_Receive_DMA(&hsai_BlockB1, (uint8_t*)audio_input_buffer,AUDIO_BUFFER_SIZE * 2);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)audio_output_buffer,AUDIO_BUFFER_SIZE * 2);
    audio_ready = 1;
}

/**
  * @brief  Stop guitar pedal audio processing
  */
void GuitarPedal_Stop(void)
{
    HAL_SAI_DMAStop(&hsai_BlockA1);
    HAL_SAI_DMAStop(&hsai_BlockB1);
    audio_ready = 0;
}

/**
  * @brief  Set looper mix level
  * @param  level: Mix level from 0.0 (silent) to 1.0 (full volume)
  */
void GuitarPedal_SetLooperLevel(float level)
{
    // Clamp to valid range
    if (level < 0.0f) level = 0.0f;
    if (level > 2.0f) level = 2.0f;  // Allow up to 2.0 for extra boost if needed
    looper_mix = level;
}

/**
  * @brief  Get current looper mix level
  * @retval Current looper mix level (0.0 to 1.0+)
  */
float GuitarPedal_GetLooperLevel(void)
{
    return looper_mix;
}

/**
  * @brief  Set output level (master volume before soft clipping)
  * @param  level: Output level from 0.0 (silent) to 1.0 (full volume)
  * @note   Lower values provide more headroom for clean tones
  */
void GuitarPedal_SetOutputLevel(float level)
{
    // Clamp to valid range
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    output_level = level;
}

/**
  * @brief  Get current output level
  * @retval Current output level (0.0 to 1.0)
  */
float GuitarPedal_GetOutputLevel(void)
{
    return output_level;
}

/**
  * @brief  Set input gain (attenuation before processing)
  * @param  gain: Input gain from 0.0 (silent) to 1.0 (unity gain)
  * @note   Use lower values (0.3-0.5) to match bypass level and prevent distortion
  */
void GuitarPedal_SetInputGain(float gain)
{
    // Clamp to valid range
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    input_gain = gain;
}

/**
  * @brief  Get current input gain
  * @retval Current input gain (0.0 to 1.0)
  */
float GuitarPedal_GetInputGain(void)
{
    return input_gain;
}

/**
  * @brief  Get pedal status
  */
void GuitarPedal_GetStatus(uint32_t* rx_count, uint32_t* tx_count, uint32_t* err_count, uint8_t* ready)
{
    *rx_count = rx_callback_count;
    *tx_count = tx_callback_count;
    *err_count = error_count;
    *ready = audio_ready;
}

/**
  * @brief  Main audio processing function - WITH AMP SIM
  *
  * SIGNAL CHAIN:
  * Input → Input Gain → Effects → Amp Sim → Looper → Cabinet IR → Output Level
  */
void GuitarPedal_ProcessAudio(AudioSample_t* input, AudioSample_t* output, uint16_t size)
{
    // Convert input to float and apply input gain
    for (uint16_t i = 0; i < size; i++) {
        in_left[i] = s242f(input[i].left) * input_gain;
        in_right[i] = s242f(input[i].right) * input_gain;
    }

    // ===== NOISE GATE (applied to input before all processing) =====
    if (noise_gate.enabled) {
        for (uint16_t i = 0; i < size; i++) {
            // Compute input level (absolute value)
            float input_level_left = fabsf(in_left[i]);
            float input_level_right = fabsf(in_right[i]);

            // Envelope follower (smooth the input level)
            if (input_level_left > noise_gate.envelope_left) {
                // Attack
                noise_gate.envelope_left = noise_gate.attack_coeff * noise_gate.envelope_left +
                                          (1.0f - noise_gate.attack_coeff) * input_level_left;
            } else {
                // Release
                noise_gate.envelope_left = noise_gate.release_coeff * noise_gate.envelope_left +
                                          (1.0f - noise_gate.release_coeff) * input_level_left;
            }

            if (input_level_right > noise_gate.envelope_right) {
                noise_gate.envelope_right = noise_gate.attack_coeff * noise_gate.envelope_right +
                                           (1.0f - noise_gate.attack_coeff) * input_level_right;
            } else {
                noise_gate.envelope_right = noise_gate.release_coeff * noise_gate.envelope_right +
                                           (1.0f - noise_gate.release_coeff) * input_level_right;
            }

            // Compute gain reduction
            float gain_left = 1.0f;
            float gain_right = 1.0f;

            if (noise_gate.envelope_left < noise_gate.threshold) {
                // Below threshold: apply expansion (attenuation)
                float diff = noise_gate.threshold - noise_gate.envelope_left;
                gain_left = 1.0f - (diff / noise_gate.threshold) * (1.0f - 1.0f / noise_gate.ratio);
                if (gain_left < 0.0f) gain_left = 0.0f;
            }

            if (noise_gate.envelope_right < noise_gate.threshold) {
                float diff = noise_gate.threshold - noise_gate.envelope_right;
                gain_right = 1.0f - (diff / noise_gate.threshold) * (1.0f - 1.0f / noise_gate.ratio);
                if (gain_right < 0.0f) gain_right = 0.0f;
            }

            // Apply gate
            in_left[i] *= gain_left;
            in_right[i] *= gain_right;
        }
    }

    if (bypass_enabled) {
        // True bypass - copy input directly to output
        for (uint16_t i = 0; i < size; i++) {
            output[i].left = input[i].left;
            output[i].right = input[i].right;
        }
        return;
    }

    // ===== STEP 1: EFFECTS PROCESSING =====
    MultiEffects_ProcessAudio(in_left, in_right, out_left, out_right, size);

    // ===== STEP 2: AMP SIMULATION (STEREO) =====
    // Process both channels through their respective amp instances
    // Note: Both amps have identical settings, but maintain separate state
    AmpSim_Process(&amp_sim_left, out_left, out_left, size);
    AmpSim_Process(&amp_sim_right, out_right, out_right, size);

    // ===== STEP 3: LOOPER INTEGRATION =====
    // Get looper state ONCE for the entire buffer (much more efficient)
    LooperState state = Looper_GetState(&looper);

    for (uint16_t i = 0; i < size; i++) {
        // Convert stereo post-amp signal to mono for looper
        float mono_in = (out_left[i] + out_right[i]) * 0.5f;
        float looper_out = 0.0f;

        // ALWAYS process the sample through the looper
        Looper_ProcessSample(&looper, mono_in, &looper_out);

        if (state == LOOPER_PLAYING || state == LOOPER_OVERDUBBING) {
            // Mix looper output with dry signal (post-amp)
            float dry = (out_left[i] + out_right[i]) * 0.5f;
            float mixed = (1.0f - looper_mix) * dry + looper_mix * looper_out;

            out_left[i]  = mixed;
            out_right[i] = mixed;
        }
        else if (state == LOOPER_RECORDING) {
            // During recording: hear live signal (amp applied)
            // No need to reassign, already in out_left/out_right
        }
        // STOPPED or no loop: just pass through

        // Gentle limiting after looper mixing (only if needed)
        // Using soft_clip instead of tanhf for much better performance
        if (out_left[i] > 1.0f || out_left[i] < -1.0f) out_left[i] = soft_clip(out_left[i]);
        if (out_right[i] > 1.0f || out_right[i] < -1.0f) out_right[i] = soft_clip(out_right[i]);
    }

    // ===== STEP 4: CABINET IR SIMULATION =====
    if (cabinet_enabled) {
        arm_fir_f32(&fir_left, out_left, out_left, size);
        arm_fir_f32(&fir_right, out_right, out_right, size);

        // Hard clip after cabinet IR (IRs can have gain)
        for (uint16_t i = 0; i < size; i++) {
            if (out_left[i] > 1.0f) out_left[i] = 1.0f;
            if (out_left[i] < -1.0f) out_left[i] = -1.0f;
            if (out_right[i] > 1.0f) out_right[i] = 1.0f;
            if (out_right[i] < -1.0f) out_right[i] = -1.0f;
        }
    }

    // ===== STEP 5: OUTPUT LEVEL AND CONVERSION =====
    float block_peak = 0.0f;  // Track peak for this block
    for (uint16_t i = 0; i < size; i++) {
        float sample_l = out_left[i] * output_level;
        float sample_r = out_right[i] * output_level;
        
        // Track peak level (absolute value of both channels)
        float abs_l = (sample_l >= 0.0f) ? sample_l : -sample_l;
        float abs_r = (sample_r >= 0.0f) ? sample_r : -sample_r;
        if (abs_l > block_peak) block_peak = abs_l;
        if (abs_r > block_peak) block_peak = abs_r;
        
        output[i].left = f2s24(sample_l);
        output[i].right = f2s24(sample_r);
    }
    
    // Update peak with decay (peak hold with slow release)
    if (block_peak > output_peak_level) {
        output_peak_level = block_peak;  // Instant attack
    } else {
        output_peak_level *= peak_decay_coeff;  // Slow decay
    }
}


void GuitarPedal_EnableBypass(bool enable)
{
	bypass_enabled = enable ? true : false;
}

/**
  * @brief  Get output level in dB for level meter display
  * @retval Output level in dB (-60 to +6 range, clamped)
  * @note   Uses peak detection with slow decay for smooth visual display
  */
float GuitarPedal_GetOutputLevelDb(void)
{
    float peak = output_peak_level;
    
    // Avoid log(0) - minimum level is -60dB
    if (peak < 0.001f) {
        return -60.0f;
    }
    
    // Convert to dB: 20 * log10(level)
    // Fast approximation: log10(x) ≈ log2(x) * 0.30103
    // But we'll use the actual formula for accuracy
    float db = 20.0f * log10f(peak);
    
    // Clamp to display range
    if (db < -60.0f) db = -60.0f;
    if (db > 6.0f) db = 6.0f;
    
    return db;
}

/* ===== AMP SIMULATION CONTROL FUNCTIONS ===== */

/**
  * @brief  Enable/disable amp simulation
  */
void GuitarPedal_EnableAmp(bool enable)
{
    AmpSim_Enable(&amp_sim_left, enable);
    AmpSim_Enable(&amp_sim_right, enable);
}

/**
  * @brief  Check if amp simulation is enabled
  */
bool GuitarPedal_IsAmpEnabled(void)
{
    return AmpSim_IsEnabled(&amp_sim_left);
}

/**
  * @brief  Set amp model (applies to both channels)
  */
void GuitarPedal_SetAmpModel(uint8_t model)
{
    if (model < AMP_MODEL_COUNT) {
        AmpSim_SetModel(&amp_sim_left, (AmpModel_t)model);
        AmpSim_SetModel(&amp_sim_right, (AmpModel_t)model);
    }
}

/**
  * @brief  Get current amp model
  */
uint8_t GuitarPedal_GetAmpModel(void)
{
    return (uint8_t)AmpSim_GetModel(&amp_sim_left);
}

/**
  * @brief  Get amp model name
  */
const char* GuitarPedal_GetAmpModelName(uint8_t model)
{
    return AmpSim_GetModelName((AmpModel_t)model);
}

/**
  * @brief  Set amp gain (applies to both channels)
  */
void GuitarPedal_SetAmpGain(float gain)
{
    AmpSim_SetGain(&amp_sim_left, gain);
    AmpSim_SetGain(&amp_sim_right, gain);
}

/**
  * @brief  Set amp bass EQ (applies to both channels)
  */
void GuitarPedal_SetAmpBass(float bass)
{
    AmpSim_SetBass(&amp_sim_left, bass);
    AmpSim_SetBass(&amp_sim_right, bass);
}

/**
  * @brief  Set amp mid EQ (applies to both channels)
  */
void GuitarPedal_SetAmpMid(float mid)
{
    AmpSim_SetMid(&amp_sim_left, mid);
    AmpSim_SetMid(&amp_sim_right, mid);
}

/**
  * @brief  Set amp treble EQ (applies to both channels)
  */
void GuitarPedal_SetAmpTreble(float treble)
{
    AmpSim_SetTreble(&amp_sim_left, treble);
    AmpSim_SetTreble(&amp_sim_right, treble);
}

/**
  * @brief  Set amp presence (applies to both channels)
  */
void GuitarPedal_SetAmpPresence(float presence)
{
    AmpSim_SetPresence(&amp_sim_left, presence);
    AmpSim_SetPresence(&amp_sim_right, presence);
}

/**
  * @brief  Set amp master volume (applies to both channels)
  */
void GuitarPedal_SetAmpMaster(float master)
{
    AmpSim_SetMaster(&amp_sim_left, master);
    AmpSim_SetMaster(&amp_sim_right, master);
}

/* ===== AMP PARAMETER GETTERS (for display) ===== */

/**
  * @brief  Get current amp gain
  */
float GuitarPedal_GetAmpGain(void)
{
    return AmpSim_GetGain(&amp_sim_left);
}

/**
  * @brief  Get current amp bass
  */
float GuitarPedal_GetAmpBass(void)
{
    return AmpSim_GetBass(&amp_sim_left);
}

/**
  * @brief  Get current amp mid
  */
float GuitarPedal_GetAmpMid(void)
{
    return AmpSim_GetMid(&amp_sim_left);
}

/**
  * @brief  Get current amp treble
  */
float GuitarPedal_GetAmpTreble(void)
{
    return AmpSim_GetTreble(&amp_sim_left);
}

/**
  * @brief  Get current amp presence
  */
float GuitarPedal_GetAmpPresence(void)
{
    return AmpSim_GetPresence(&amp_sim_left);
}

/**
  * @brief  Get current amp master
  */
float GuitarPedal_GetAmpMaster(void)
{
    return AmpSim_GetMaster(&amp_sim_left);
}

/* ===== NOISE GATE CONTROL FUNCTIONS ===== */

/**
  * @brief  Enable/disable noise gate
  * @param  enable: 1 to enable, 0 to disable
  */
void GuitarPedal_EnableNoiseGate(uint8_t enable)
{
    noise_gate.enabled = enable ? 1 : 0;
    if (!enable) {
        // Reset envelope when disabling
        noise_gate.envelope_left = 0.0f;
        noise_gate.envelope_right = 0.0f;
    }
}

/**
  * @brief  Check if noise gate is enabled
  * @retval 1 if enabled, 0 if disabled
  */
uint8_t GuitarPedal_IsNoiseGateEnabled(void)
{
    return noise_gate.enabled;
}

/**
  * @brief  Set noise gate threshold
  * @param  threshold_db: Threshold in dB (-60 to 0)
  * @note   Typical range: -60dB (very sensitive) to -20dB (less sensitive)
  */
void GuitarPedal_SetNoiseGateThreshold(float threshold_db)
{
    // Clamp to reasonable range
    if (threshold_db > 0.0f) threshold_db = 0.0f;
    if (threshold_db < -60.0f) threshold_db = -60.0f;
    
    // Convert dB to linear
    noise_gate.threshold = powf(10.0f, threshold_db / 20.0f);
}

/**
  * @brief  Get noise gate threshold in dB
  * @retval Threshold in dB
  */
float GuitarPedal_GetNoiseGateThreshold(void)
{
    // Convert linear to dB
    if (noise_gate.threshold > 0.0f) {
        return 20.0f * log10f(noise_gate.threshold);
    }
    return -60.0f; // Minimum
}

/**
  * @brief  Set noise gate attack time
  * @param  attack_ms: Attack time in milliseconds (0.1 to 50)
  * @note   Fast attack (1-5ms) for tight gating, slower for smoother
  */
void GuitarPedal_SetNoiseGateAttack(float attack_ms)
{
    if (attack_ms < 0.1f) attack_ms = 0.1f;
    if (attack_ms > 50.0f) attack_ms = 50.0f;
    
    float attack_time = attack_ms / 1000.0f; // Convert to seconds
    noise_gate.attack_coeff = expf(-1.0f / (attack_time * AUDIO_SAMPLE_RATE));
}

/**
  * @brief  Set noise gate release time
  * @param  release_ms: Release time in milliseconds (10 to 500)
  * @note   Fast release (20-50ms) can sound choppy; slower (100-200ms) is more natural
  */
void GuitarPedal_SetNoiseGateRelease(float release_ms)
{
    if (release_ms < 10.0f) release_ms = 10.0f;
    if (release_ms > 500.0f) release_ms = 500.0f;
    
    float release_time = release_ms / 1000.0f; // Convert to seconds
    noise_gate.release_coeff = expf(-1.0f / (release_time * AUDIO_SAMPLE_RATE));
}

/**
  * @brief  Set noise gate ratio (expansion ratio)
  * @param  ratio: Ratio from 1.0 (off) to 100.0 (hard gate)
  * @note   Low ratio (2-5) = gentle expander; high ratio (10+) = aggressive gate
  */
void GuitarPedal_SetNoiseGateRatio(float ratio)
{
    if (ratio < 1.0f) ratio = 1.0f;
    if (ratio > 100.0f) ratio = 100.0f;
    
    noise_gate.ratio = ratio;
}

/**
  * @brief  Get noise gate ratio
  * @retval Current ratio
  */
float GuitarPedal_GetNoiseGateRatio(void)
{
    return noise_gate.ratio;
}

/* DMA Callback Functions */

/**
  * @brief  SAI RX DMA Half Transfer Complete callback
  */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockB1 && audio_ready) {
        rx_callback_count++;
        
        // CRITICAL: Invalidate D-cache for input buffer before reading DMA data
        SCB_InvalidateDCache_by_Addr((uint32_t*)&audio_input_buffer[0], 
                                      (AUDIO_BUFFER_SIZE / 2) * sizeof(AudioSample_t));
        
        // Process audio directly in ISR for real-time performance
        GuitarPedal_ProcessAudio(&audio_input_buffer[0],
                                &audio_output_buffer[0],
                                AUDIO_BUFFER_SIZE / 2);
        
        // CRITICAL: Clean D-cache for output buffer so DMA sees updated data
        SCB_CleanDCache_by_Addr((uint32_t*)&audio_output_buffer[0], 
                                 (AUDIO_BUFFER_SIZE / 2) * sizeof(AudioSample_t));
    }
}

/**
  * @brief  SAI RX DMA Transfer Complete callback
  */
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockB1 && audio_ready) {
        rx_callback_count++;
        
        // CRITICAL: Invalidate D-cache for input buffer before reading DMA data
        SCB_InvalidateDCache_by_Addr((uint32_t*)&audio_input_buffer[AUDIO_BUFFER_SIZE / 2], 
                                      (AUDIO_BUFFER_SIZE / 2) * sizeof(AudioSample_t));
        
        // Process audio directly in ISR for real-time performance
        GuitarPedal_ProcessAudio(&audio_input_buffer[AUDIO_BUFFER_SIZE / 2],
                                &audio_output_buffer[AUDIO_BUFFER_SIZE / 2],
                                AUDIO_BUFFER_SIZE / 2);
        
        // CRITICAL: Clean D-cache for output buffer so DMA sees updated data
        SCB_CleanDCache_by_Addr((uint32_t*)&audio_output_buffer[AUDIO_BUFFER_SIZE / 2], 
                                 (AUDIO_BUFFER_SIZE / 2) * sizeof(AudioSample_t));
    }
}

/**
  * @brief  SAI TX callbacks
  */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1) {
        tx_callback_count++;
    }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_BlockA1) {
        tx_callback_count++;
    }
}

/**
  * @brief  Error callback
  */
void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    error_count++;
    // Auto-recovery attempt
    if (audio_ready) {
        GuitarPedal_Stop();
        HAL_Delay(10);
        GuitarPedal_Start();
    }

}

float s242f(int32_t x) {
    x = (x ^ 0x800000) - 0x800000;
    return (float)x * 1.192092896e-07f;
}

int32_t f2s24(float x) {
    x = soft_clip(x);
    return (int32_t)(x * 8388608.0f);
}
