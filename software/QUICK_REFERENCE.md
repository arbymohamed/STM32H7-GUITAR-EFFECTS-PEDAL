# Quick Reference: Audio Artifacts - Root Causes & Fixes

## 🔴 CRITICAL ISSUES FOUND

### Issue #1: Cache Incoherency (STM32H7)
**Symptom:** Random pops, clicks, glitches - especially when UI active
**Root Cause:** 
- D-Cache enabled but no cache maintenance on audio buffers
- DMA writes → CPU reads stale cache → corruption
- CPU writes → DMA reads stale cache → corruption

**Fix Applied:** ✅
```c
// guitar_pedal.c - SAI callbacks
SCB_InvalidateDCache_by_Addr(&audio_input_buffer[...]);  // Before CPU reads
GuitarPedal_ProcessAudio(...);
SCB_CleanDCache_by_Addr(&audio_output_buffer[...]);      // Before DMA reads
```

---

### Issue #2: Parameter Race Conditions
**Symptom:** Glitches when moving sliders, parameter "jumps"
**Root Cause:**
- UI thread: `MultiEffects_SetEffectParam()` writes to effect globals
- Audio ISR: `Effect_Process()` reads same globals
- NO synchronization → torn reads, inconsistent state

**Fix Applied:** ✅ (delay.c, compressor.c)
```c
static volatile float param;  // Now volatile

void Effect_SetParam(float value) {
    param = value;
    __DMB();  // Memory barrier
}
```

**Still TODO:** Apply to other 11 effect files (use script!)

---

### Issue #3: ISR Priority Too High
**Symptom:** UI freezes, sluggish touch response
**Root Cause:** SAI @ priority 0 blocks SysTick, DMA, USB

**Fix Applied:** ✅
```c
// sai.c
HAL_NVIC_SetPriority(SAI1_IRQn, 5, 0);  // Was 0,0 → now 5,0
```

---

### Issue #4: SDRAM Without MPU
**Symptom:** Intermittent glitches with delay/flanger
**Root Cause:** SDRAM buffers without proper MPU/cache config

**Fix Required:** ⚠️ Manual (see guide)

---

### Issue #5: Heavy Processing in ISR
**Symptom:** System instability, unpredictable timing
**Root Cause:** Complex effects (reverbs) run in ISR context

**Fix Required:** ⚠️ Future enhancement (double-buffer + thread)

---

## 🎯 WHAT TO DO NOW

### Immediate (Required):
1. ✅ Rebuild project with current changes
2. ⚠️ Run `./fix_remaining_effects.sh` 
3. ⚠️ Manually add `__DMB()` to all effect setters
4. ✅ Test with single effect → multi-effect chain

### Soon (Recommended):
1. ⚠️ Configure SDRAM MPU region (see guide)
2. ⚠️ Add error monitoring code
3. ⚠️ Profile CPU usage in audio callback

### Future (Optional):
1. Move processing out of ISR
2. Optimize with ARM CMSIS-DSP
3. Add overflow detection

---

## 📊 Expected Results

| Before | After |
|--------|-------|
| Pops when moving sliders | ✅ Smooth transitions |
| UI freezes during playback | ✅ Responsive UI |
| Random glitches | ✅ Clean audio |
| Artifacts with >2 effects | ✅ Stable chains |

---

## 🔧 Files Modified

**Automatically fixed:**
- ✅ `Core/Src/guitarpedal/guitar_pedal.c` (cache ops)
- ✅ `Core/Src/effects/delay.c` (volatile + barriers)
- ✅ `Core/Src/effects/compressor.c` (volatile + barriers)  
- ✅ `Core/Src/sai.c` (interrupt priority)

**Need manual attention:**
- ⚠️ 11 remaining effect files (use script!)
- ⚠️ `Core/Src/main.c` (MPU config - optional)

---

## 🧪 Testing Commands

```c
// Monitor errors in main loop:
char buf[64];
sprintf(buf, "RX:%lu TX:%lu ERR:%lu\r\n", 
        rx_callback_count, tx_callback_count, error_count);
debug_print(buf);

// Should be: RX ≈ TX, ERR = 0
```

---

## 📞 If Problems Persist

1. Check linker warnings for section overlaps
2. Verify SDRAM initialization (FMC_TEST passed?)
3. Test with bypass enabled (rules out effect bugs)
4. Measure ISR execution time with DWT cycle counter

---

**Created:** 2025-02-01
**Project:** DIY Guitar Pedal (STM32H743)
**Priority:** CRITICAL - affects core audio functionality
