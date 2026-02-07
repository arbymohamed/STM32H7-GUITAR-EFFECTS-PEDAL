#include "looper.h"
#include <string.h>
#include <math.h>
#include "main.h"  // For SCB_CleanDCache_by_Addr
// max samples
#define MAX_LOOP_SAMPLES (48000 * 60)  // 1 minute and 29 seconds at 48kHz
// Boss RC-1 style overdub mixing
#define OVERDUB_FEEDBACK 0.85f  // How much of existing loop to keep (85%)
#define OVERDUB_INPUT 0.5f      // How much new input to add (50%)
// This allows ~5-10 overdub layers before noticeable fading

// Windowing (crossfade) to eliminate clicks at loop boundaries
#define WINDOW_SAMPLES 1200     // 25ms @ 48kHz (same as DaisySP)
#define WINDOW_FACTOR (1.0f / WINDOW_SAMPLES)

// Background undo copy strategy: copy AHEAD of playhead during normal playback
// This way, by the time user presses overdub, the buffer is already copied!
#define UNDO_SYNC_AHEAD_SAMPLES 4800  // Stay 100ms ahead of playhead (4800 samples @ 48kHz)
#define UNDO_SYNC_CHUNK_SIZE 480      // Copy 10ms chunks (480 samples @ 48kHz)

// Buffers in SDRAM
__attribute__((section(".sdram"), aligned(32)))
static float looper_buffer[MAX_LOOP_SAMPLES];

__attribute__((section(".sdram"), aligned(32)))
static float looper_undo_buffer[MAX_LOOP_SAMPLES];

Looper looper; // global instance

// ============================================================================
// WINDOWING FUNCTIONS (Eliminate clicks at loop boundaries)
// ============================================================================

// Sine-based window function for smooth crossfade (DaisySP style)
// Input: 0.0 to 1.0
// Output: 0.0 to 1.0 (sine curve)
static inline float window_value(float x) {
    // sin(π/2 × x) creates a smooth S-curve from 0 to 1
    return sinf(1.5707963267948966f * x);  // 1.5707... = π/2
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void Looper_Init(Looper *looper, uint32_t max_length_samples) {
    memset(looper, 0, sizeof(Looper));

    looper->buffer = looper_buffer;
    looper->undo_buffer = looper_undo_buffer;
    looper->max_length_samples = max_length_samples;
    looper->state = LOOPER_STOPPED;
    looper->pending_cmd = CMD_NONE;
    looper->quantize_enabled = true;  // RC-1 style: commands execute at loop boundary
    looper->undo_buffer_synced = false;
    looper->last_sync_position = 0;

    // Clear buffers
    memset(looper->buffer, 0, max_length_samples * sizeof(float));
    memset(looper->undo_buffer, 0, max_length_samples * sizeof(float));
}

// ============================================================================
// COMMAND INTERFACE (Called from button handler)
// ============================================================================
void Looper_RequestCommand(Looper *looper, LooperCommand cmd) {
    looper->pending_cmd = cmd;
}

LooperCommand Looper_GetPendingCommand(const Looper *looper) {
    return looper->pending_cmd;
}

// ============================================================================
// STATE QUERIES
// ============================================================================
LooperState Looper_GetState(const Looper *looper) {
    return looper->state;
}

bool Looper_HasLoop(const Looper *looper) {
    return looper->has_loop;
}

bool Looper_CanUndo(const Looper *looper) {
    return looper->has_undo;
}

uint32_t Looper_GetPosition(const Looper *looper) {
    return looper->position;
}

uint32_t Looper_GetLoopLength(const Looper *looper) {
    return looper->loop_length;
}

uint8_t Looper_GetProgressPercent(const Looper *looper) {
    if (looper->loop_length == 0) return 0;
    return (uint8_t)((looper->position * 100) / looper->loop_length);
}

// ============================================================================
// INTERNAL: Execute commands (called from audio callback)
// ============================================================================
static void execute_command(Looper *looper, LooperCommand cmd) {
    switch (cmd) {
        case CMD_START_RECORDING:
            looper->state = LOOPER_RECORDING;
            looper->position = 0;
            looper->loop_length = 0;
            looper->has_loop = false;
            looper->has_undo = false;
            looper->window_idx = 0;          // Reset window
            looper->windowing_active = true;  // Start with fade-in
            break;

        case CMD_STOP_RECORDING:
            if (looper->state == LOOPER_RECORDING && looper->loop_length > 0) {
                looper->state = LOOPER_PLAYING;
                looper->has_loop = true;
                looper->position = 0;  // Start playback from beginning
                looper->window_idx = 0;          // Reset window for seamless loop
                looper->windowing_active = true;  // Fade out input at loop end

                // Start background sync of undo buffer
                looper->undo_buffer_synced = false;
                looper->last_sync_position = 0;
            }
            break;

        case CMD_START_OVERDUB:
            if (looper->has_loop) {
                if (looper->state == LOOPER_PLAYING || looper->state == LOOPER_STOPPED) {
                    looper->state = LOOPER_OVERDUBBING;
                    looper->window_idx = 0;          // Reset window for smooth overdub start
                    looper->windowing_active = true;  // Fade in new input

                    // OPTION 1: Disable undo copying (no glitches, no undo)
                    looper->has_undo = false;
                    looper->undo_buffer_synced = false;

                    // OPTION 2: Only enable undo if buffer was already synced
                    // if (looper->undo_buffer_synced) {
                    //     looper->has_undo = true;
                    //     looper->undo_buffer_synced = false; // Will need to re-sync for next overdub
                    // }
                }
            }
            break;

        case CMD_STOP_OVERDUB:
            if (looper->state == LOOPER_OVERDUBBING) {
                looper->state = LOOPER_PLAYING;
                looper->windowing_active = false;  // No windowing needed when just playing
            }
            break;

        case CMD_STOP_PLAYBACK:
            if (looper->has_loop) {
                looper->state = LOOPER_STOPPED;
            }
            break;

        case CMD_RESUME_PLAYBACK:
            if (looper->has_loop && looper->state == LOOPER_STOPPED) {
                looper->state = LOOPER_PLAYING;
            }
            break;

        case CMD_UNDO:
            if (looper->has_undo && looper->has_loop) {
                // Ensure cache coherency before swap (read fresh data from SDRAM)
                SCB_InvalidateDCache_by_Addr((uint32_t*)looper->buffer,
                                             looper->loop_length * sizeof(float));
                SCB_InvalidateDCache_by_Addr((uint32_t*)looper->undo_buffer,
                                             looper->loop_length * sizeof(float));

                // Swap buffers
                float *tmp = looper->buffer;
                looper->buffer = looper->undo_buffer;
                looper->undo_buffer = tmp;

                // If we were overdubbing, stop
                if (looper->state == LOOPER_OVERDUBBING) {
                    looper->state = LOOPER_PLAYING;
                }

                // Undo is now used up (RC-1 has single-level undo)
                looper->has_undo = false;
            }
            break;

        case CMD_CLEAR:
            looper->state = LOOPER_STOPPED;
            looper->has_loop = false;
            looper->has_undo = false;
            looper->loop_length = 0;
            looper->position = 0;
            looper->undo_buffer_synced = false;
            looper->last_sync_position = 0;

            // Clear buffers
            memset(looper->buffer, 0, looper->max_length_samples * sizeof(float));
            memset(looper->undo_buffer, 0, looper->max_length_samples * sizeof(float));

            // Ensure cache is properly synchronized with SDRAM (STM32H7 has cache)
            // Clean = write cache to SDRAM, Invalidate = discard cache, force read from SDRAM
            SCB_CleanInvalidateDCache_by_Addr((uint32_t*)looper->buffer,
                                              looper->max_length_samples * sizeof(float));
            SCB_CleanInvalidateDCache_by_Addr((uint32_t*)looper->undo_buffer,
                                              looper->max_length_samples * sizeof(float));
            break;

        default:
            break;
    }

    looper->pending_cmd = CMD_NONE;
}

// ============================================================================
// MAIN AUDIO PROCESSING (Called from audio callback)
// ============================================================================
void Looper_ProcessSample(Looper *looper, float input, float *output) {

    // No background copying - keeps audio glitch-free!
    // Undo feature is disabled to maintain perfect audio quality

    // Check if we should execute pending command
    // For quantized operation: execute at loop boundary (position == 0)
    // For immediate operation: execute right away
    if (looper->pending_cmd != CMD_NONE) {
        bool should_execute = false;

        if (!looper->quantize_enabled) {
            // Immediate execution
            should_execute = true;
        } else {
            // Quantized execution - only for timing-critical commands
            // STOP, CLEAR, UNDO execute immediately for instant response
            LooperCommand cmd = looper->pending_cmd;

            if (cmd == CMD_STOP_PLAYBACK ||
                cmd == CMD_STOP_OVERDUB ||
                cmd == CMD_CLEAR ||
                cmd == CMD_UNDO ||
                cmd == CMD_RESUME_PLAYBACK) {
                // Immediate execution - user expects instant response
                should_execute = true;
            } else if (looper->state == LOOPER_STOPPED ||
                       looper->state == LOOPER_RECORDING ||
                       !looper->has_loop) {
                // No loop yet - execute immediately
                should_execute = true;
            } else if (looper->position == 0) {
                // At loop boundary - execute now (for overdub start)
                should_execute = true;
            }
        }

        if (should_execute) {
            execute_command(looper, looper->pending_cmd);
        }
    }

    // Process audio based on current state
    switch (looper->state) {
        case LOOPER_STOPPED:
            *output = input;  // Pass through
            break;

        case LOOPER_RECORDING:
            if (looper->position < looper->max_length_samples) {
                // Apply fade-in window at the start to prevent clicks
                float window = 1.0f;
                if (looper->windowing_active && looper->window_idx < WINDOW_SAMPLES) {
                    window = window_value((float)looper->window_idx * WINDOW_FACTOR);
                    looper->window_idx++;
                } else {
                    looper->windowing_active = false;  // Window complete
                }

                looper->buffer[looper->position] = input * window;
                *output = input;  // Monitor full input (before window)
                looper->position++;
                looper->loop_length = looper->position;
            } else {
                // Hit max length - auto-stop recording
                looper->state = LOOPER_PLAYING;
                looper->has_loop = true;
                looper->position = 0;
                looper->window_idx = 0;
                looper->windowing_active = true;  // Start fade-out at loop end
                *output = looper->buffer[looper->position];
                looper->position++;
            }
            break;

        case LOOPER_PLAYING:
            if (looper->loop_length > 0) {
                // Output = loop playback + live input (dry monitoring)
                float loop = looper->buffer[looper->position];
                *output = loop + input;

                // Prevent clipping from mixing loop + input
                if (*output > 1.0f) *output = 1.0f;
                if (*output < -1.0f) *output = -1.0f;

                looper->position++;
                if (looper->position >= looper->loop_length) {
                    looper->position = 0;  // Loop around
                }
            } else {
                *output = input;
            }
            break;

        case LOOPER_OVERDUBBING:
            if (looper->loop_length > 0) {
                // Boss RC-1 style: feedback loop allows multiple overdub layers
                // Existing layers kept at 85%, new input added at 50%
                float existing = looper->buffer[looper->position];

                // Apply fade-in window to new input to prevent clicks
                float input_gain = OVERDUB_INPUT;
                if (looper->windowing_active && looper->window_idx < WINDOW_SAMPLES) {
                    float window = window_value((float)looper->window_idx * WINDOW_FACTOR);
                    input_gain *= window;  // Fade in from 0 to full gain
                    looper->window_idx++;
                } else {
                    looper->windowing_active = false;  // Window complete
                }

                float mixed = (existing * OVERDUB_FEEDBACK) + (input * input_gain);

                // Soft clipping to prevent harsh distortion on heavy overdubs
                if (mixed > 1.0f) mixed = 1.0f;
                if (mixed < -1.0f) mixed = -1.0f;

                looper->buffer[looper->position] = mixed;
                *output = mixed;

                looper->position++;
                if (looper->position >= looper->loop_length) {
                    looper->position = 0;  // Loop around
                }
            } else {
                *output = input;
            }
            break;

        default:
            *output = input;
            break;
    }
}
