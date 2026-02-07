#ifndef DSP_UTILS_H
#define DSP_UTILS_H

/**
 * @file dsp_utils.h
 * @brief Audio DSP utility functions from DaisySP
 * 
 * Collection of efficient DSP helper functions for audio effects.
 * Based on DaisySP library by Electrosmith.
 */

#include <math.h>

//=============================================================================
// CONSTANTS
//=============================================================================

#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)
#define HALFPI_F (PI_F * 0.5f)

//=============================================================================
// BASIC MATH UTILITIES
//=============================================================================

/**
 * @brief Fast floating-point clamp
 * @param in Input value
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value between min and max
 */
static inline float fclamp(float in, float min, float max) {
    return (in < min) ? min : (in > max) ? max : in;
}

/**
 * @brief Fast floating-point minimum
 * @param a First value
 * @param b Second value
 * @return Minimum of a and b
 */
static inline float fmin_dsp(float a, float b) {
    return (a < b) ? a : b;
}

/**
 * @brief Fast floating-point maximum
 * @param a First value
 * @param b Second value
 * @return Maximum of a and b
 */
static inline float fmax_dsp(float a, float b) {
    return (a > b) ? a : b;
}

//=============================================================================
// SOFT CLIPPING (MUCH BETTER THAN TANH!)
//=============================================================================

/**
 * @brief Soft limiting function
 * @param x Input signal
 * @return Softly limited output
 * 
 * Piecewise polynomial limiter, much smoother than hard clipping.
 * Extracted from Mutable Instruments / pichenettes
 */
static inline float SoftLimit(float x) {
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

/**
 * @brief Soft clipping function (RECOMMENDED for overdrive/distortion)
 * @param x Input signal
 * @return Soft-clipped output (-1.0 to +1.0)
 * 
 * This is 5-10x faster than tanhf() and sounds much smoother!
 * Perfect for guitar overdrive/distortion.
 * 
 * Behavior:
 * - x < -3.0: Hard limit at -1.0
 * - -3.0 < x < 3.0: Smooth polynomial curve
 * - x > 3.0: Hard limit at +1.0
 */
static inline float SoftClip(float x) {
    if(x < -3.0f)
        return -1.0f;
    else if(x > 3.0f)
        return 1.0f;
    else
        return SoftLimit(x);
}

/**
 * @brief Asymmetric soft clipping (for tube-like distortion)
 * @param x Input signal
 * @param asymmetry Asymmetry amount (0.0 = symmetric, 1.0 = max asymmetry)
 * @return Asymmetrically clipped output
 */
static inline float SoftClipAsym(float x, float asymmetry) {
    float bias = asymmetry * 0.5f;
    return SoftClip(x + bias) - bias * 0.5f;
}

//=============================================================================
// FILTERS
//=============================================================================

/**
 * @brief One-pole low-pass filter (in-place)
 * @param out Pointer to output (also holds previous state)
 * @param in Input sample
 * @param coeff Filter coefficient (0.0 to 1.0)
 * 
 * coeff = 1.0 / (time_constant * sample_rate)
 * where time_constant is in seconds
 * 
 * Example: 10ms time constant at 48kHz
 * coeff = 1.0 / (0.01 * 48000) = 0.002083
 */
static inline void OnePole(float *out, float in, float coeff) {
    *out += coeff * (in - *out);
}

/**
 * @brief Calculate one-pole filter coefficient from frequency
 * @param freq Cutoff frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @return Filter coefficient
 */
static inline float OnePoleCoeff(float freq, float sample_rate) {
    return 1.0f - expf(-TWOPI_F * freq / sample_rate);
}

//=============================================================================
// INTERPOLATION
//=============================================================================

/**
 * @brief Linear interpolation
 * @param a First value
 * @param b Second value
 * @param t Interpolation factor (0.0 to 1.0)
 * @return Interpolated value
 */
static inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief Cubic interpolation (Hermite)
 * @param xm1 Sample at x-1
 * @param x0 Sample at x
 * @param x1 Sample at x+1
 * @param x2 Sample at x+2
 * @param t Fractional position (0.0 to 1.0)
 * @return Interpolated value
 */
static inline float CubicInterp(float xm1, float x0, float x1, float x2, float t) {
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

//=============================================================================
// CONVERSION UTILITIES
//=============================================================================

/**
 * @brief MIDI note to frequency
 * @param m MIDI note number (69 = A440)
 * @return Frequency in Hz
 */
static inline float MidiToFreq(float m) {
    return powf(2.0f, (m - 69.0f) / 12.0f) * 440.0f;
}

/**
 * @brief Decibels to linear gain
 * @param db Decibels
 * @return Linear gain
 */
static inline float DbToGain(float db) {
    return powf(10.0f, db * 0.05f);
}

/**
 * @brief Linear gain to decibels
 * @param gain Linear gain
 * @return Decibels
 */
static inline float GainToDb(float gain) {
    return 20.0f * log10f(gain);
}

//=============================================================================
// WAVEFORM GENERATORS
//=============================================================================

/**
 * @brief Generate sine wave (0.0 to 1.0)
 * @param phase Phase in radians (0 to 2*PI)
 * @return Sine wave value (0.0 to 1.0)
 */
static inline float SineWave01(float phase) {
    return (sinf(phase) + 1.0f) * 0.5f;
}

/**
 * @brief Generate triangle wave (0.0 to 1.0)
 * @param phase Phase in radians (0 to 2*PI)
 * @return Triangle wave value (0.0 to 1.0)
 */
static inline float TriangleWave01(float phase) {
    return 1.0f - fabsf((phase / PI_F) - 1.0f);
}

/**
 * @brief Generate square wave (0.0 to 1.0)
 * @param phase Phase in radians (0 to 2*PI)
 * @return Square wave value (0.0 or 1.0)
 */
static inline float SquareWave01(float phase) {
    return (phase < PI_F) ? 1.0f : 0.0f;
}

/**
 * @brief Generate sawtooth wave (0.0 to 1.0)
 * @param phase Phase in radians (0 to 2*PI)
 * @return Sawtooth wave value (0.0 to 1.0)
 */
static inline float SawWave01(float phase) {
    return phase / TWOPI_F;
}

//=============================================================================
// PARAMETER MAPPING
//=============================================================================

/**
 * @brief Map linear value to exponential curve
 * @param in Input (0.0 to 1.0)
 * @param min Minimum output value
 * @param max Maximum output value
 * @return Exponentially mapped value
 * 
 * Useful for frequency controls (sounds more natural)
 */
static inline float MapExp(float in, float min, float max) {
    return fclamp(min + (in * in) * (max - min), min, max);
}

/**
 * @brief Map linear value to logarithmic curve
 * @param in Input (0.0 to 1.0)
 * @param min Minimum output value (must be > 0)
 * @param max Maximum output value (must be > 0)
 * @return Logarithmically mapped value
 * 
 * Useful for amplitude controls (sounds more natural)
 */
static inline float MapLog(float in, float min, float max) {
    float a = 1.0f / log10f(max / min);
    return fclamp(min * powf(10.0f, in / a), min, max);
}

//=============================================================================
// STEREO UTILITIES
//=============================================================================

/**
 * @brief Pan law (constant power)
 * @param pan Pan position (0.0 = left, 1.0 = right)
 * @param gainL Pointer to left gain output
 * @param gainR Pointer to right gain output
 */
static inline void PanConstantPower(float pan, float *gainL, float *gainR) {
    pan = fclamp(pan, 0.0f, 1.0f);
    float angle = pan * HALFPI_F;
    *gainL = cosf(angle);
    *gainR = sinf(angle);
}

/**
 * @brief Mid/Side encoding
 * @param left Left channel input
 * @param right Right channel input
 * @param mid Pointer to mid output
 * @param side Pointer to side output
 */
static inline void MidSideEncode(float left, float right, float *mid, float *side) {
    *mid = (left + right) * 0.5f;
    *side = (left - right) * 0.5f;
}

/**
 * @brief Mid/Side decoding
 * @param mid Mid channel input
 * @param side Side channel input
 * @param left Pointer to left output
 * @param right Pointer to right output
 */
static inline void MidSideDecode(float mid, float side, float *left, float *right) {
    *left = mid + side;
    *right = mid - side;
}

//=============================================================================
// OVERDRIVE/DISTORTION HELPERS
//=============================================================================

/**
 * @brief Calculate overdrive gain staging (DaisySP style)
 * @param drive Drive amount (0.0 to 1.0)
 * @param pre_gain Pointer to pre-gain output
 * @param post_gain Pointer to post-gain output (for volume compensation)
 */
static inline void OverdriveGainStaging(float drive, float *pre_gain, float *post_gain) {
    drive = fclamp(drive, 0.0f, 1.0f);
    float drive_scaled = 2.0f * drive;
    float drive_2 = drive_scaled * drive_scaled;
    
    // Pre-gain: gentle at low drive, aggressive at high drive
    float pre_gain_a = drive_scaled * 0.5f;
    float pre_gain_b = drive_2 * drive_2 * drive_scaled * 24.0f;
    *pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;
    
    // Post-gain: volume compensation
    float drive_squashed = drive_scaled * (2.0f - drive_scaled);
    *post_gain = 1.0f / SoftClip(0.33f + drive_squashed * (*pre_gain - 0.33f));
}

#endif // DSP_UTILS_H
