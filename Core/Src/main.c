/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "quadspi.h"
#include "sai.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "guitarpedal/guitar_pedal.h"
#include "slider_conversions.h"
#include "stdio.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "chorus.h"
#include "delay.h"
#include "overdrive.h"
#include "tremolo.h"
#include "lvgl.h"
#include "ui.h"
#include "images.h"
#include <stdbool.h> /* Added for bool type support in C99 */
#include "looper.h"
#include "effectsmanager/multieffects_manager.h"
#include "st7796.h"
#include "GT911.h"



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VER_RES 320
#define HOR_RES 480

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define SDRAM_START_ADDRESS 0xC0000000
#define SDRAM_SIZE 0x1000000
#define SDRAM_TIMEOUT      ((uint32_t)0xFFFF)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

// Boss RC-1 Style Looper Timing
#define DOUBLE_PRESS_WINDOW_MS  400   // Time window for double press
#define HOLD_THRESHOLD_MS       2000  // Hold time for undo/clear
#define DEBOUNCE_MS             25    // Button debounce time

// Bar value range (0-100)
#define MAX_VALUE               100   // Maximum bar value for LVGL bars

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern Looper looper; // Reference global instance from looper.c

GT911_Config_t sampleConfig = {.X_Resolution = 480, .Y_Resolution = 320, .Number_Of_Touch_Support = 1, .ReverseY = true, .ReverseX = true, .SwithX2Y = false, .SoftwareNoiseReduction = true};
TouchCordinate_t cordinate[5];
uint8_t number;

__attribute__((section(".adcarray"))) uint16_t ADC_VAL[4];
static uint8_t buf1[HOR_RES * VER_RES / 10 * BYTES_PER_PIXEL]; //__attribute__((section(".sdram")));
static uint8_t buf2[HOR_RES * VER_RES / 10 * BYTES_PER_PIXEL]; //__attribute__((section(".sdram")));
/*
 * pot1 = ADC_VAL[3]
 * pot2 = ADC_VAL[2]
 * pot3 = ADC_VAL[0]
 * pot4 = ADC_VAL[1]
 * */
int16_t cnt = 0;
lv_indev_t * indev;
lv_display_t * display ;
lv_group_t * groups[8];
char buf[32];
char effect_being_edited[32];
int32_t index_of_effect;
// Helper arrays for pot pickup mode
FMC_SDRAM_CommandTypeDef Command;
HAL_StatusTypeDef status;

// RC-1 Style Button State
typedef struct {
    uint32_t last_press_time;
    uint32_t press_start_time;
    bool is_pressed;
    bool waiting_for_double;
    bool double_detected;
    LooperState state_before_stop;  // Remember state before stopping
    bool hold_icon_shown;  // Track if hold icon has been displayed
} ButtonRC1;

static ButtonRC1 rc1_btn = {0};

#define BUTTON_EVT_BUF_SIZE 8
typedef struct {
    uint8_t state;         // debounced state (0/1)
    uint32_t time_ms;      // event timestamp from ISR
} ButtonEvent;


volatile uint8_t isr_button_raw = 0;
volatile uint8_t isr_button_debounced = 0;
volatile uint8_t isr_button_stable_count = 0; // in ms (saturates)
volatile uint8_t isr_button_event = 0;        // set to 1 on debounced edge change
volatile uint32_t isr_button_event_time_ms = 0; // timestamp of edge (ms)
static ButtonEvent button_evt_buf[BUTTON_EVT_BUF_SIZE];
static volatile uint8_t button_evt_head = 0; // next write (ISR)
static volatile uint8_t button_evt_tail = 0; // next read (main)


uint32_t loop_count = 0;
uint32_t loop_acc_ms = 0;
uint32_t loop_max_ms = 0;
uint32_t loop_min_ms = 0xFFFFFFFF;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ISR-safe push: called from timer ISR context. If buffer full, drop the oldest event.
static inline void button_evt_push_isr(uint8_t state, uint32_t time_ms)
{
    uint8_t next = (button_evt_head + 1) & (BUTTON_EVT_BUF_SIZE - 1);
    if(next == button_evt_tail) {
        // buffer full; drop oldest to make room
        button_evt_tail = (button_evt_tail + 1) & (BUTTON_EVT_BUF_SIZE - 1);
    }
    button_evt_buf[button_evt_head].state = state;
    button_evt_buf[button_evt_head].time_ms = time_ms;
    // ensure data written before updating head
    __DSB();
    button_evt_head = next;
}

// Main-thread pop: returns true if an event was read
static inline bool button_evt_pop_main(uint8_t *out_state, uint32_t *out_time_ms)
{
    if(button_evt_tail == button_evt_head) return false;
    *out_state = button_evt_buf[button_evt_tail].state;
    *out_time_ms = button_evt_buf[button_evt_tail].time_ms;
    button_evt_tail = (button_evt_tail + 1) & (BUTTON_EVT_BUF_SIZE - 1);
    return true;
}

static void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command) {
	__IO uint32_t tmpmrd = 0;

	Command->CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
	Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
	Command->AutoRefreshNumber = 1;
	Command->ModeRegisterDefinition = 0;

	status = HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

	HAL_Delay(1);

	Command->CommandMode = FMC_SDRAM_CMD_PALL;
	Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
	Command->AutoRefreshNumber = 1;
	Command->ModeRegisterDefinition = 0;

	status = HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

	Command->CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
	Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
	Command->AutoRefreshNumber = 8;
	Command->ModeRegisterDefinition = 0;

	status = HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

	tmpmrd = (uint32_t) SDRAM_MODEREG_BURST_LENGTH_4 | SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | SDRAM_MODEREG_CAS_LATENCY_2 | SDRAM_MODEREG_OPERATING_MODE_STANDARD | SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
	Command->CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
	Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
	Command->AutoRefreshNumber = 1;
	Command->ModeRegisterDefinition = tmpmrd;

	status = HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

	hsdram->Instance->SDRTR |= ((uint32_t) ((761) << 1));
}

void FMC_TEST(){
    // 1. Erase SDRAM (set all to zero)
    volatile char *sdram_char_addr = (char *)SDRAM_START_ADDRESS;
    for(uint32_t i = 0; i < 128; i++) {
        sdram_char_addr[i] = 0;
    }
    SCB_CleanDCache_by_Addr((uint32_t*)sdram_char_addr, 128);
    SCB_InvalidateDCache_by_Addr((uint32_t*)sdram_char_addr, 128);
    // 2. Write string to SDRAM
    const char *test_str = "electronics project";
    size_t str_len = strlen(test_str);
    for(size_t i = 0; i < str_len; i++) {
        sdram_char_addr[i] = test_str[i];
    }
    sdram_char_addr[str_len] = '\0';
    SCB_CleanDCache_by_Addr((uint32_t*)sdram_char_addr, str_len+1);
    SCB_InvalidateDCache_by_Addr((uint32_t*)sdram_char_addr, str_len+1);
}

EffectType effect_name_to_enum(const char *name) {
    // Using first character as quick filter
    char first = name[0];

    switch(first) {
        case 'C':
            if(strcmp(name, "CHORUS") == 0) return EFFECT_CHORUS;
            if(strcmp(name, "COMPRESSOR") == 0) return EFFECT_COMPRESSOR;
            break;
        case 'D':
            if(strcmp(name, "DELAY") == 0) return EFFECT_DELAY;
            if(strcmp(name, "DISTORTION") == 0) return EFFECT_DISTORTION;
            break;
        case 'O':
            if(strcmp(name, "OVERDRIVE") == 0) return EFFECT_OVERDRIVE;
            break;
        case 'T':
            if(strcmp(name, "TREMOLO") == 0) return EFFECT_TREMOLO;
            break;
        case 'F':
            if(strcmp(name, "FLANGER") == 0) return EFFECT_FLANGER;
            break;
        case 'P':
            if(strcmp(name, "PHASER") == 0) return EFFECT_PHASER;
            if(strcmp(name, "PLATEREVERB") == 0) return EFFECT_PLATE_REVERB;
            break;
        case 'A':
            if(strcmp(name, "AUTOWAH") == 0) return EFFECT_AUTOWAH;
            break;
        case 'H':
            if(strcmp(name, "HALLREVERB") == 0) return EFFECT_HALL_REVERB;
            break;
        case 'S':
            if(strcmp(name, "SPRINGREVERB") == 0) return EFFECT_SPRING_REVERB;
            if(strcmp(name, "SHIMMERREVERB") == 0) return EFFECT_SHIMMER_REVERB;
            break;
    }
    return EFFECT_COUNT; // Invalid
}




void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map){
    uint16_t * buf16 = (uint16_t *)px_map;
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    uint32_t pixels = (uint32_t)w * (uint32_t)h;

    st7796_set_window(area->x1, area->y1, area->x2, area->y2);
    st7796_write_pixels(buf16, pixels);
    lv_display_flush_ready(display);
    // DO NOT call lv_display_flush_ready() here!
    // The driver will call it automatically when DMA completes all chunks
}



void read_touch_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    GT911_ReadTouch(cordinate, &number);
    if(number > 0) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = 480 - cordinate[0].y;
        data->point.y = cordinate[0].x;
        // Debug print removed - was causing audio artifacts!
        // Enable only for debugging: debug_print("Touch\\r\\n");
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void message_box(const char* title, const char* message) {
    // message
    lv_obj_t *obj = lv_msgbox_create(objects.main);

    lv_obj_set_pos(obj, 120, 96);
    lv_obj_set_size(obj, LV_PCT(50), LV_PCT(40));
    lv_obj_set_style_align(obj, LV_ALIGN_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xffe85a4f), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_t *btn = lv_msgbox_add_close_button(obj);
	lv_obj_set_style_bg_color(btn, 								lv_color_hex(0x000000), LV_PART_MAIN);
	lv_obj_set_style_bg_color(lv_msgbox_get_header(obj), 		lv_color_hex(0xE85A4F), LV_PART_MAIN);
	lv_obj_set_style_text_color(lv_msgbox_get_content(obj), 	lv_color_hex(0xffffff), LV_PART_MAIN);

	lv_msgbox_add_text(obj, message);
	lv_msgbox_add_title(obj, title);

}

static void cabs_dropdown_cb(lv_event_t * e){
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * dropdown = lv_event_get_target(e);
	if (dropdown == objects.cabs_dropdown){
		if(code == LV_EVENT_VALUE_CHANGED){
			GuitarPedal_SetCabinet(lv_dropdown_get_selected(objects.cabs_dropdown));
		}
	}

}

static void amps_dropdown_cb(lv_event_t * e){
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * dropdown = lv_event_get_target(e);
	if (dropdown == objects.amps_dropdown){
		if(code == LV_EVENT_VALUE_CHANGED){
			GuitarPedal_SetAmpModel(lv_dropdown_get_selected(objects.amps_dropdown));
		}
	}
}

void init_amp_sliders(){

	// set the slider values when amp model changes
	lv_slider_set_value(objects.gain_slider, GuitarPedal_GetAmpGain() * 100.0f, LV_ANIM_OFF);
	lv_slider_set_value(objects.bass_slider, GuitarPedal_GetAmpBass() * 100.0f, LV_ANIM_OFF);
	lv_slider_set_value(objects.mid_slider, GuitarPedal_GetAmpMid() * 100.0f, LV_ANIM_OFF);
	lv_slider_set_value(objects.treble_slider, GuitarPedal_GetAmpTreble() * 100.0f, LV_ANIM_OFF);
	lv_slider_set_value(objects.presence_slider, GuitarPedal_GetAmpPresence() * 100.0f, LV_ANIM_OFF);
	lv_slider_set_value(objects.master_slider, GuitarPedal_GetAmpMaster() * 100.0f, LV_ANIM_OFF);

	lv_obj_send_event(objects.gain_slider, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_send_event(objects.bass_slider, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_send_event(objects.mid_slider, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_send_event(objects.treble_slider, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_send_event(objects.presence_slider, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_send_event(objects.master_slider, LV_EVENT_VALUE_CHANGED, NULL);

}

static void amp_slider_cb(lv_event_t *e){
	lv_obj_t * slider = lv_event_get_target(e);
	int32_t slider_val = lv_slider_get_value(slider);

	if(slider == objects.gain_slider){
		float gain = slider_to_amp_gain(slider_val);
		GuitarPedal_SetAmpGain(gain);
		lv_label_set_text_fmt(objects.gain_value, "%ld%%", slider_val);
	} else if(slider == objects.bass_slider){
		float bass = slider_to_amp_bass(slider_val);
		GuitarPedal_SetAmpBass(bass);
		lv_label_set_text_fmt(objects.bass_value, "%+ld%%", slider_val - 50);  // Shows -50% to +50%
	} else if(slider == objects.mid_slider){
		float mid = slider_to_amp_mid(slider_val);
		GuitarPedal_SetAmpMid(mid);
		lv_label_set_text_fmt(objects.mid_value, "%+ld%%", slider_val - 50);  // Shows -50% to +50%
	} else if(slider == objects.treble_slider){
		float treble = slider_to_amp_treble(slider_val);
		GuitarPedal_SetAmpTreble(treble);
		lv_label_set_text_fmt(objects.treble_value, "%+ld%%", slider_val - 50);  // Shows -50% to +50%
	} else if(slider == objects.presence_slider){
		float presence = slider_to_amp_presence(slider_val);
		GuitarPedal_SetAmpPresence(presence);
		lv_label_set_text_fmt(objects.presence_value, "%ld%%", slider_val);
	} else if(slider == objects.master_slider){
		float master = slider_to_amp_master(slider_val);
		GuitarPedal_SetAmpMaster(master);
		lv_label_set_text_fmt(objects.master_value, "%ld%%", slider_val);
	}
}

static void effects_slider_cb(lv_event_t *e){
	lv_obj_t * slider = lv_event_get_target(e);
	// Get parameter index from user data (authoritative)
	int param_idx = (int)(uintptr_t)lv_event_get_user_data(e);
	if (param_idx < 0 || param_idx >= MAX_PARAMS) return;

	// which effects parameters are being changed?
	EffectInstance* instance = MultiEffects_GetInstance(index_of_effect);
	if(instance ){
		EffectType type = instance->type;
		int32_t slider_val = lv_slider_get_value(slider); // 0..100
		float param_value = MultiEffects_ConvertParam(type, param_idx, (uint32_t)slider_val);
		MultiEffects_SetEffectParam(index_of_effect, param_idx, param_value);
	}
}

static void slider_value_cb(lv_event_t * e){
	lv_obj_t * slider = lv_event_get_target(e);

	// Get parameter index from user data
	int param_idx = (int)(uintptr_t)lv_event_get_user_data(e);
	char buf[16];  // Increased size for formatted values

	// Get current effect instance and format the parameter value
	EffectInstance* instance = MultiEffects_GetInstance(index_of_effect);
	int bar_value = lv_slider_get_value(slider);
	if (instance) {
		MultiEffects_FormatParamValue(instance->type, param_idx, bar_value, buf, sizeof(buf));
	} else {
		// Fallback to raw value if no instance
		lv_snprintf(buf, sizeof(buf), "%d", (int)bar_value);
	}

	switch (param_idx) {
		case 0:
			lv_label_set_text(objects.effect_p1_value, buf);
			break;
		case 1:
			lv_label_set_text(objects.effect_p2_value, buf);
			break;
		case 2:
			lv_label_set_text(objects.effect_p3_value, buf);
			break;
		case 3:
			lv_label_set_text(objects.effect_p4_value, buf);
			break;
		default:
			break;
	}
}

static void list_button_cb(lv_event_t * e){
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * btn = lv_event_get_target(e);
	if(code == LV_EVENT_CLICKED){
		// Load the parameter page and set up sliders for the selected effect
		lv_screen_load(objects.parameters);
		lv_label_set_text(objects.current_effect_label, lv_list_get_button_text(objects.effect_chain_list, btn));
		// find selected effect instance
		const char *selected_button = lv_list_get_btn_text(objects.effect_chain_list, btn);
		index_of_effect = MultiEffects_FindEffectIndexByName(selected_button);
		EffectInstance* instance = MultiEffects_GetInstance(index_of_effect);
		if (instance) {
			// Initialize only the 4 shared sliders using inverse conversion so slider position
			// reflects the stored natural-unit parameter value.
			lv_slider_set_value(objects.effect_p1_slider, MultiEffects_ActualToSlider(instance->type, 0, instance->params[0]), LV_ANIM_OFF);
			lv_slider_set_value(objects.effect_p2_slider, MultiEffects_ActualToSlider(instance->type, 1, instance->params[1]), LV_ANIM_OFF);
			lv_slider_set_value(objects.effect_p3_slider, MultiEffects_ActualToSlider(instance->type, 2, instance->params[2]), LV_ANIM_OFF);
			lv_slider_set_value(objects.effect_p4_slider, MultiEffects_ActualToSlider(instance->type, 3, instance->params[3]), LV_ANIM_OFF);

			// send the event to update the value labels (pass param idx as user_data)
			lv_obj_send_event(objects.effect_p1_slider, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)0);
			lv_obj_send_event(objects.effect_p2_slider, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)1);
			lv_obj_send_event(objects.effect_p3_slider, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)2);
			lv_obj_send_event(objects.effect_p4_slider, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)3);
		}
		// Reset pickup state and last pot values for new effect (ignored by user as requested)
		// assign param labels based on effect type
		if (instance) {
			EffectType type = instance->type;
			switch (type) {
				case EFFECT_DELAY:
					lv_label_set_text(objects.effect_p1_label, "Time");
					lv_label_set_text(objects.effect_p2_label, "Fb");
					lv_label_set_text(objects.effect_p3_label, "Tone");
					lv_label_set_text(objects.effect_p4_label, "Mix");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_OVERDRIVE:
					lv_label_set_text(objects.effect_p1_label, "Drive");
					lv_label_set_text(objects.effect_p2_label, "Level");
					lv_label_set_text(objects.effect_p3_label, "Tone");
					lv_obj_add_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_add_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_CHORUS:
					lv_label_set_text(objects.effect_p1_label, "Rate");
					lv_label_set_text(objects.effect_p2_label, "Depth");
					lv_label_set_text(objects.effect_p3_label, "Mix");
					lv_obj_add_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_add_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_TREMOLO:
					lv_label_set_text(objects.effect_p1_label, "Rate");
					lv_label_set_text(objects.effect_p2_label, "Depth");
					lv_label_set_text(objects.effect_p3_label, "Mix");
					lv_obj_add_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_add_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_FLANGER:
					lv_label_set_text(objects.effect_p1_label, "Rate");
					lv_label_set_text(objects.effect_p2_label, "Depth");
					lv_label_set_text(objects.effect_p3_label, "Fb");
					lv_label_set_text(objects.effect_p4_label, "Delay");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_PHASER:
					lv_label_set_text(objects.effect_p1_label, "Rate");
					lv_label_set_text(objects.effect_p2_label, "Depth");
					lv_label_set_text(objects.effect_p3_label, "Fb");
					lv_label_set_text(objects.effect_p4_label, "Freq");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_DISTORTION:
					lv_label_set_text(objects.effect_p1_label, "Gain");
					lv_label_set_text(objects.effect_p2_label, "Level");
					lv_label_set_text(objects.effect_p3_label, "Tone");
					lv_obj_add_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_add_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_AUTOWAH:
					lv_label_set_text(objects.effect_p1_label, "Wah");
					lv_label_set_text(objects.effect_p2_label, "Level");
					lv_label_set_text(objects.effect_p3_label, "Mix");
					lv_label_set_text(objects.effect_p4_label, "Wet_boost");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_COMPRESSOR:
					lv_label_set_text(objects.effect_p1_label, "Compression");
					lv_label_set_text(objects.effect_p2_label, "Ratio");
					lv_label_set_text(objects.effect_p3_label, "Release");
					lv_label_set_text(objects.effect_p4_label, "Attack");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_HALL_REVERB:
					lv_label_set_text(objects.effect_p1_label, "Time");
					lv_label_set_text(objects.effect_p2_label, "Damp");
					lv_label_set_text(objects.effect_p3_label, "PreD");
					lv_label_set_text(objects.effect_p4_label, "Mix");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_PLATE_REVERB:
					lv_label_set_text(objects.effect_p1_label, "Time");
					lv_label_set_text(objects.effect_p2_label, "Damp");
					lv_label_set_text(objects.effect_p3_label, "Tone");
					lv_label_set_text(objects.effect_p4_label, "Mix");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_SPRING_REVERB:
					lv_label_set_text(objects.effect_p1_label, "Tension");
					lv_label_set_text(objects.effect_p2_label, "Decay");
					lv_label_set_text(objects.effect_p3_label, "Tone");
					lv_label_set_text(objects.effect_p4_label, "Mix");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				case EFFECT_SHIMMER_REVERB:
					lv_label_set_text(objects.effect_p1_label, "Time");
					lv_label_set_text(objects.effect_p2_label, "Depth");
					lv_label_set_text(objects.effect_p3_label, "Rate");
					lv_label_set_text(objects.effect_p4_label, "Mix");
					lv_obj_clear_flag(objects.effect_p4_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
				default:
					lv_label_set_text(objects.effect_p1_label, "Param 1");
					lv_label_set_text(objects.effect_p2_label, "Param 2");
					lv_label_set_text(objects.effect_p3_label, "Param 3");
					lv_label_set_text(objects.effect_p4_label, "Param 4");
					lv_obj_clear_flag(objects.effect_p1_label, LV_OBJ_FLAG_HIDDEN);
					lv_obj_clear_flag(objects.effect_p4_slider, LV_OBJ_FLAG_HIDDEN);
					break;
			}
		}
	}
}

static void actions(lv_event_t *e){
	lv_event_code_t event = (lv_event_code_t) lv_event_get_code(e);
	lv_obj_t * target = (lv_obj_t *)lv_event_get_target(e);
	if (target == objects.ret_btn){
		if(event == LV_EVENT_RELEASED){
			lv_screen_load(objects.main);
		}
	}
	else if (target == objects.effects_dropdown){
		if(event == LV_EVENT_VALUE_CHANGED){
			// SHOW THE CONFIRMATION PANEL
			lv_dropdown_get_selected_str(objects.effects_dropdown, buf, sizeof(buf));
			
			// check if the effect is already in the chain
			uint8_t chain_count = MultiEffects_GetChainCount();
			
			for(uint8_t i=0; i<chain_count; i++){
				EffectInstance* instance = MultiEffects_GetInstance(i);
				if(instance){
					const char* name = effect_names[instance->type];
					// Debug: Compare strings
					if(strcmp(name, buf) == 0){
						// Effect already in chain - show message and exit
						message_box("Already Added", "This effect is already in your chain.");
						return;
					}
				}
			}
			
			// Effect not in chain, safe to add
			EffectType effect_type = effect_name_to_enum(buf);
			if(effect_type >= EFFECT_COUNT) {
				message_box("Error", "Unknown effect selected.");
				return;
			}
			
			// Add effect to chain
			MultiEffects_AddEffect(effect_type);
			// Use effect manager's name for button text to ensure compatibility
			lv_obj_t * button = lv_list_add_button(objects.effect_chain_list, NULL, effect_names[effect_type]);
			lv_obj_set_height(button, 50);
			lv_obj_set_style_text_font(button, &lv_font_montserrat_26, 0);
			lv_obj_set_style_bg_color(button, lv_color_hex(0xE85A4F), 0);
			lv_obj_add_event_cb(button, list_button_cb, LV_EVENT_CLICKED, NULL);
		}
	} else if (target == objects.del_btn){
		if(event == LV_EVENT_RELEASED){
			// load the delete confirmation screen
			const char *effect_to_delete = lv_label_get_text(objects.current_effect_label);
			lv_label_set_text_fmt(objects.fx_to_delete, " Delete %s", effect_to_delete);
			lv_label_set_text_fmt(objects.delete_message, " Remove %s from your chain? ", effect_to_delete);
			lv_screen_load(objects.deletepage);
		}
	} else if (target == objects.final_del_btn){
		if(event == LV_EVENT_RELEASED){
			// delete effect from chain
			const char *effect_to_delete = lv_label_get_text(objects.current_effect_label);
			uint32_t idx = 0;
			lv_obj_t* btn = lv_obj_get_child(objects.effect_chain_list, idx);
			while(btn) {
				const char* btn_text = lv_list_get_btn_text(objects.effect_chain_list, btn);
				if(strcmp(btn_text, effect_to_delete) == 0) {
					lv_obj_del(btn);
					break;
				}
				idx++;
				btn = lv_obj_get_child(objects.effect_chain_list, idx);
			}
			// Remove effect from internal chain using RemoveEffect
			uint8_t chain_count = MultiEffects_GetChainCount();
			if (index_of_effect >= 0 && index_of_effect < chain_count) {
				MultiEffects_RemoveEffect(index_of_effect);
			}
			// Reset index_of_effect
			index_of_effect = -1;
			// go back to chain screen
			lv_screen_load(objects.main);
		}
	} else if (target == objects.cancel_del_btn){
		if(event == LV_EVENT_RELEASED){
			// go back to parameters screen
			lv_screen_load(objects.parameters);
		}
	}

}

/* ------------------------------
   1. BUTTON INPUT + DEBOUNCE
------------------------------ */
// Tiny accessor used by main
static inline uint8_t button_get_state(void) {
    return isr_button_debounced; // read single byte atomically on Cortex-M
}

/* ------------------------------
   3. FLASH QUEUE
------------------------------ */
#define MAX_FLASH 4

typedef struct {
    lv_obj_t *icon;
    int remaining;
    int interval_ms;
    bool visible;
    uint32_t last_toggle;
} FlashItem;

static FlashItem flash_items[MAX_FLASH] = {0};

static void flash_icon(lv_obj_t *icon, int times, int interval_ms) {
    for(int i=0;i<MAX_FLASH;i++){
        if(flash_items[i].icon == NULL){
            flash_items[i].icon = icon;
            flash_items[i].remaining = times*2;
            flash_items[i].interval_ms = interval_ms;
            flash_items[i].visible = false;
            flash_items[i].last_toggle = HAL_GetTick();
            lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

static void flash_update(uint32_t now){
    for(int i=0;i<MAX_FLASH;i++){
        FlashItem *f = &flash_items[i];
        if(f->icon && f->remaining > 0){
            if(now - f->last_toggle >= f->interval_ms){
                if(f->visible)
                    lv_obj_add_flag(f->icon, LV_OBJ_FLAG_HIDDEN);
                else
                    lv_obj_clear_flag(f->icon, LV_OBJ_FLAG_HIDDEN);

                f->visible = !f->visible;
                f->last_toggle = now;
                f->remaining--;

                if(f->remaining <= 0){
                    lv_obj_add_flag(f->icon, LV_OBJ_FLAG_HIDDEN);
                    f->icon = NULL;
                }
            }
        }
    }
}

/* ------------------------------
   4. UI ICONS
------------------------------ */
static void ui_set_icons(LooperState state)
{
    // Hide all icons first
    lv_obj_add_flag(objects.record_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.play_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(objects.pause_icon, LV_OBJ_FLAG_HIDDEN);
//    lv_obj_add_flag(objects.undo_icon, LV_OBJ_FLAG_HIDDEN);
//    lv_obj_add_flag(objects.redo_icon, LV_OBJ_FLAG_HIDDEN);

    // Show icon for current state
    switch(state) {
        case LOOPER_STOPPED:
            if(Looper_HasLoop(&looper)) {
                lv_obj_clear_flag(objects.pause_icon, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case LOOPER_RECORDING:
            lv_obj_clear_flag(objects.record_icon, LV_OBJ_FLAG_HIDDEN);
            break;
        case LOOPER_PLAYING:
            lv_obj_clear_flag(objects.play_icon, LV_OBJ_FLAG_HIDDEN);
            break;
        case LOOPER_OVERDUBBING:
            lv_obj_clear_flag(objects.record_icon, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}
/* ------------------------------
   5. RC-1 BUTTON HANDLER
------------------------------ */
static void execute_single_press(void) {
    LooperState state = Looper_GetState(&looper);
    bool has_loop = Looper_HasLoop(&looper);

    switch (state) {
        case LOOPER_STOPPED:
            if (has_loop) {
                // Resume to previous state (playing or overdubbing)
                if (rc1_btn.state_before_stop == LOOPER_OVERDUBBING) {
                    Looper_RequestCommand(&looper, CMD_START_OVERDUB);
                } else {
                    // Default: resume playback
                    Looper_RequestCommand(&looper, CMD_RESUME_PLAYBACK);
                }
            } else {
                // Start recording if no loop exists
                Looper_RequestCommand(&looper, CMD_START_RECORDING);
            }
            break;
        case LOOPER_RECORDING:
            Looper_RequestCommand(&looper, CMD_STOP_RECORDING);
            break;
        case LOOPER_PLAYING:
            Looper_RequestCommand(&looper, CMD_START_OVERDUB);
            break;
        case LOOPER_OVERDUBBING:
            Looper_RequestCommand(&looper, CMD_STOP_OVERDUB);
            break;
    }
}

static void execute_double_press(void) {
    LooperState state = Looper_GetState(&looper);

    if (state == LOOPER_PLAYING || state == LOOPER_OVERDUBBING) {
        // Remember state before stopping
        rc1_btn.state_before_stop = state;
        Looper_RequestCommand(&looper, CMD_STOP_PLAYBACK);
    } else if (state == LOOPER_RECORDING) {
        Looper_RequestCommand(&looper, CMD_STOP_RECORDING);
        Looper_RequestCommand(&looper, CMD_STOP_PLAYBACK);
        rc1_btn.state_before_stop = LOOPER_STOPPED;
    }
}

static void execute_hold(bool was_double) {
    // Icon already shown during hold, just execute the command
    if (was_double) {
        Looper_RequestCommand(&looper, CMD_CLEAR);
    } else {
        if (Looper_CanUndo(&looper)) {
            Looper_RequestCommand(&looper, CMD_UNDO);
        } else {
            Looper_RequestCommand(&looper, CMD_CLEAR);
        }
    }
}

/* ------------------------------
   7. MAIN LOOPER CONTROL - RC-1 STYLE
------------------------------ */
void looper_button_ctrl(void) {
    uint32_t now = HAL_GetTick();
    uint8_t button_state = button_get_state();  // 0 or 1

    static uint8_t last_button_state = 0;

    // Check for hold threshold while button is STILL PRESSED (visual feedback)
    if (button_state == 1 && rc1_btn.is_pressed && !rc1_btn.hold_icon_shown) {
        uint32_t hold_duration = now - rc1_btn.press_start_time;

        if (hold_duration >= HOLD_THRESHOLD_MS) {
            // Hold threshold reached! Show icon as visual feedback
            if (rc1_btn.double_detected) {
                // Double + hold = CLEAR icon
                flash_icon(objects.record_icon, 3, 100);
            } else {
                // Single hold = UNDO or CLEAR icon
                if (Looper_CanUndo(&looper)) {
                    //flash_icon(objects.undo_icon, 2, 50);
                } else {
                    flash_icon(objects.record_icon, 3, 100);
                }
            }

            rc1_btn.hold_icon_shown = true;  // Don't show again until next press
        }
    }

    // Only process on state CHANGE
    if (button_state != last_button_state) {
        if (button_state == 1) {
            // BUTTON PRESSED (0->1 transition)
            rc1_btn.is_pressed = true;
            rc1_btn.press_start_time = now;
            rc1_btn.hold_icon_shown = false;  // Reset for new press

            if (rc1_btn.waiting_for_double &&
                (now - rc1_btn.last_press_time) <= DOUBLE_PRESS_WINDOW_MS) {
                // DOUBLE PRESS!
                rc1_btn.double_detected = true;
                rc1_btn.waiting_for_double = false;
                execute_double_press();
            } else {
                // SINGLE PRESS - execute immediately
                rc1_btn.double_detected = false;
                execute_single_press();
                rc1_btn.waiting_for_double = true;
                rc1_btn.last_press_time = now;
            }

        } else {
            // BUTTON RELEASED (1->0 transition)
            uint32_t hold_duration = now - rc1_btn.press_start_time;

            if (hold_duration >= HOLD_THRESHOLD_MS) {
                execute_hold(rc1_btn.double_detected);
                rc1_btn.waiting_for_double = false;
            }

            rc1_btn.is_pressed = false;
            rc1_btn.hold_icon_shown = false;  // Reset for next press
        }

        last_button_state = button_state;
    }

    // Check double-press timeout when button is not pressed
    if (button_state == 0 && rc1_btn.waiting_for_double &&
        (now - rc1_btn.last_press_time) > DOUBLE_PRESS_WINDOW_MS) {
        rc1_btn.waiting_for_double = false;
    }

    // Update UI
    ui_set_icons(Looper_GetState(&looper));
    flash_update(now);
}

/* ------------------------------
   8. LOOPER PROGRESS BAR
------------------------------ */
static void looper_bar_update_cb(lv_timer_t * timer)
{
    (void)timer;
    LooperState state = Looper_GetState(&looper);
    uint8_t progress = Looper_GetProgressPercent(&looper);

    lv_bar_set_value(objects.looper_bar_progress, progress, LV_ANIM_OFF);
    //lv_arc_set_value(objects.looper_bar_progress, progress);

    // Color based on state
    lv_color_t bar_color;
    switch(state) {
        case LOOPER_RECORDING:
            bar_color = lv_color_hex(0xFF0000);  // Red
            break;
        case LOOPER_PLAYING:
            bar_color = lv_color_hex(0x00FF00);  // Green
            break;
        case LOOPER_OVERDUBBING:
            bar_color = lv_color_hex(0xFF8000);  // Orange
            break;
        default:
            bar_color = lv_color_hex(0x808080);  // Gray
            break;
    }
    lv_obj_set_style_bg_color(objects.looper_bar_progress, bar_color, LV_PART_INDICATOR);
    //lv_obj_set_style_arc_color(objects.looper_bar_progress, bar_color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_FMC_Init();
  MX_SAI1_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM13_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_QUADSPI_Init();
  MX_TIM8_Init();
  MX_USART6_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  GT911_Init(sampleConfig);

  // Initialize SDRAM
  SDRAM_Initialization_Sequence(&hsdram1, &Command);
  HAL_Delay(10);

  st7796_init();
  st7796_invert_colors(1);  // Try this if colors are inverted
  st7796_set_rotation(3);   // Try different rotations (0-3)
  st7796_fill_color(0x0000);
  HAL_Delay(200);
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, SET);

  // lvgl part
  lv_init();
  lv_tick_set_cb(HAL_GetTick);
  display = lv_display_create(HOR_RES, VER_RES);
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, my_flush_cb);

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, read_touch_cb);

  ui_init();

  message_box("Welcome", "Guitar Pedal Ready!");

  lv_screen_load_anim(objects.main, LV_SCR_LOAD_ANIM_FADE_ON, 500, 500, false);

  lv_obj_add_event_cb(objects.parameters, 		actions, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(objects.effects_dropdown, 	actions, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.ret_btn, 			actions, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(objects.del_btn, 			actions, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(objects.final_del_btn, 	actions, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(objects.cancel_del_btn, 	actions, LV_EVENT_RELEASED, NULL);

  lv_obj_add_event_cb(objects.cabs_dropdown, cabs_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.amps_dropdown, amps_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_bar_set_value(objects.looper_bar_progress, 0, LV_ANIM_OFF);
  lv_obj_add_flag(objects.record_icon, 		LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(objects.play_icon, 		LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(objects.pause_icon, 		LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(objects.effect_p1_slider, effects_slider_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)0);
  lv_obj_add_event_cb(objects.effect_p2_slider, effects_slider_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)1);
  lv_obj_add_event_cb(objects.effect_p3_slider, effects_slider_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)2);
  lv_obj_add_event_cb(objects.effect_p4_slider, effects_slider_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)3);

  lv_obj_add_event_cb(objects.effect_p1_slider, slider_value_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)0);
  lv_obj_add_event_cb(objects.effect_p2_slider, slider_value_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)1);
  lv_obj_add_event_cb(objects.effect_p3_slider, slider_value_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)2);
  lv_obj_add_event_cb(objects.effect_p4_slider, slider_value_cb, 	LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)3);

  lv_obj_add_event_cb(objects.gain_slider, 		amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.bass_slider, 		amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.mid_slider, 		amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.treble_slider, 	amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.presence_slider, 	amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(objects.master_slider, 	amp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);


  lv_slider_set_range(objects.effect_p1_slider, 0, 100);
  lv_slider_set_range(objects.effect_p2_slider, 0, 100);
  lv_slider_set_range(objects.effect_p3_slider, 0, 100);
  lv_slider_set_range(objects.effect_p4_slider, 0, 100);

  lv_slider_set_range(objects.gain_slider, 		0, 100);
  lv_slider_set_range(objects.bass_slider, 		0, 100);
  lv_slider_set_range(objects.mid_slider, 		0, 100);
  lv_slider_set_range(objects.treble_slider, 	0, 100);
  lv_slider_set_range(objects.presence_slider, 	0, 100);
  lv_slider_set_range(objects.master_slider, 	0, 100);

  // Initialize encoder
  HAL_TIM_Base_Start_IT(&htim13);

  // Initialize guitar pedal
  Looper_Init(&looper, 48000*60);
  GuitarPedal_Init();
  HAL_Delay(10);
  GuitarPedal_Start();
  MultiEffects_Init();
  GuitarPedal_EnableNoiseGate(1);
  GuitarPedal_EnableAmp(1);      // Turn on amp simulation
  GuitarPedal_EnableCabinet(1);  // Turn on cabinet simulation
  HAL_Delay(10);
  init_amp_sliders();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_VAL, 4);
  
  uint32_t last_ui_update = 0;
  
  while (1)
  {
	/* Rate-limit UI updates to reduce CPU load and audio interference */
	uint32_t now = HAL_GetTick();
	
	/* Update UI at most every 20ms (50 Hz) - smooth enough for human eye */
	if (now - last_ui_update >= 20) {
		last_ui_update = now;
		
		/* CRITICAL: Keep this short to avoid blocking audio */
		lv_timer_handler();  // Process LVGL tasks
		
		looper_button_ctrl();
		looper_bar_update_cb(NULL);

		//
		lv_bar_set_value(objects.output_level_bar, (int32_t)GuitarPedal_GetOutputLevelDb(), LV_ANIM_OFF);

		ui_tick();
	} else {
		/* Yield CPU when UI doesn't need updating */
		HAL_Delay(1);
	}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 78;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_SPI1;
  PeriphClkInitStruct.PLL3.PLL3M = 5;
  PeriphClkInitStruct.PLL3.PLL3N = 192;
  PeriphClkInitStruct.PLL3.PLL3P = 5;
  PeriphClkInitStruct.PLL3.PLL3Q = 20;
  PeriphClkInitStruct.PLL3.PLL3R = 80;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL3;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  // Process the data
    if (hadc == &hadc1)
    {
//        char msg[128];
//        snprintf(msg, sizeof(msg),
//                 "ADC[0]=%u | ADC[1]=%u | ADC[2]=%u | ADC[3]=%u\r\n",
//                 ADC_VAL[3], ADC_VAL[2], ADC_VAL[0], ADC_VAL[1]);
//
//        CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
    }
}

static inline void Button_Timer_Tick_FromISR(void)
{
	//debug_print("[TIMER] Button Timer Tick\r\n");
	uint8_t raw = (HAL_GPIO_ReadPin(I_O_1_GPIO_Port, I_O_1_Pin) == GPIO_PIN_RESET) ? 1 : 0;

    if(raw == isr_button_raw) {
        if(isr_button_stable_count < 250) isr_button_stable_count++; // saturate to avoid wrap
    } else {
        isr_button_raw = raw;
        isr_button_stable_count = 1;
    }

    if(isr_button_stable_count >= DEBOUNCE_MS) {
        if(isr_button_debounced != isr_button_raw) {
            isr_button_debounced = isr_button_raw;        // publish new stable state
            isr_button_event_time_ms = HAL_GetTick();     // timestamp (non-blocking read)
            // push to ring buffer for main to consume
            button_evt_push_isr(isr_button_debounced, isr_button_event_time_ms);
            isr_button_event = 1;                         // compatibility flag
        }
        // keep stable_count saturated
        isr_button_stable_count = DEBOUNCE_MS;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM13) {
	  Button_Timer_Tick_FromISR();

  }
}

//void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
//{
//    if (hspi == &hspi1) {
//        ST7796S_DMA_TxCpltCallback(hspi);
//        // Don't call lv_display_flush_ready() here anymore!
//        // The driver handles it internally after ALL chunks complete
//    }
//}


/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
