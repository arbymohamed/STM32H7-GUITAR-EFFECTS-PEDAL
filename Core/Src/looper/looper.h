#pragma once
#include <stdint.h>
#include <stdbool.h>

// Boss RC-1 style states - simplified!
typedef enum {
    LOOPER_STOPPED,      // No loop, or loop stopped
    LOOPER_RECORDING,    // Recording first loop
    LOOPER_PLAYING,      // Playing loop
    LOOPER_OVERDUBBING   // Adding layers
} LooperState;

// Pending commands to be executed sample-accurately
typedef enum {
    CMD_NONE = 0,
    CMD_START_RECORDING,
    CMD_STOP_RECORDING,
    CMD_START_OVERDUB,
    CMD_STOP_OVERDUB,
    CMD_STOP_PLAYBACK,
    CMD_RESUME_PLAYBACK,  // Resume from stopped state
    CMD_CLEAR,
    CMD_UNDO
} LooperCommand;

typedef struct {
    // Current state
    LooperState state;

    // Buffers
    float *buffer;              // Main loop buffer
    float *undo_buffer;         // Single undo buffer for last overdub
    uint32_t max_length_samples;
    uint32_t loop_length;       // Length of the recorded loop
    uint32_t position;          // Current playback/record position

    // Flags
    bool has_loop;              // True when a loop exists
    bool has_undo;              // True when undo buffer is valid

    // Pending command (set by button, executed in audio callback)
    volatile LooperCommand pending_cmd;

    // Quantization - execute command at loop boundary
    bool quantize_enabled;

    // Windowing (crossfade to prevent clicks at loop boundaries)
    uint32_t window_idx;        // Current window position (0-1200)
    bool windowing_active;      // True during fade in/out

    // Background undo buffer copy strategy:
    // Copy happens DURING PLAYBACK (before overdub is pressed)
    // By the time user presses overdub, copy is already done!
    bool undo_buffer_synced;    // True when undo buffer matches main buffer
    uint32_t last_sync_position; // Last position where we synced undo buffer

} Looper;

// Initialization
void Looper_Init(Looper *looper, uint32_t max_length_samples);

// Main processing - CALL THIS FROM AUDIO CALLBACK
void Looper_ProcessSample(Looper *looper, float input, float *output);

// Command interface - CALL THESE FROM BUTTON HANDLER
void Looper_RequestCommand(Looper *looper, LooperCommand cmd);
LooperCommand Looper_GetPendingCommand(const Looper *looper);

// State queries
LooperState Looper_GetState(const Looper *looper);
bool Looper_HasLoop(const Looper *looper);
bool Looper_CanUndo(const Looper *looper);
uint32_t Looper_GetPosition(const Looper *looper);
uint32_t Looper_GetLoopLength(const Looper *looper);
uint8_t Looper_GetProgressPercent(const Looper *looper);
