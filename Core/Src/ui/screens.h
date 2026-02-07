#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *splash;
    lv_obj_t *main;
    lv_obj_t *parameters;
    lv_obj_t *deletepage;
    lv_obj_t *main_page_header;
    lv_obj_t *amps_dropdown;
    lv_obj_t *cabs_dropdown;
    lv_obj_t *main_page_mid_section;
    lv_obj_t *effect_chain_list;
    lv_obj_t *effects_dropdown;
    lv_obj_t *obj0;
    lv_obj_t *gain_value;
    lv_obj_t *bass_value;
    lv_obj_t *mid_value;
    lv_obj_t *treble_value;
    lv_obj_t *presence_value;
    lv_obj_t *master_value;
    lv_obj_t *gain_slider;
    lv_obj_t *bass_slider;
    lv_obj_t *mid_slider;
    lv_obj_t *treble_slider;
    lv_obj_t *presence_slider;
    lv_obj_t *master_slider;
    lv_obj_t *output_level_bar;
    lv_obj_t *main_page_footer;
    lv_obj_t *record_icon;
    lv_obj_t *pause_icon;
    lv_obj_t *play_icon;
    lv_obj_t *looper_bar_progress;
    lv_obj_t *obj1;
    lv_obj_t *ret_btn;
    lv_obj_t *current_effect_label;
    lv_obj_t *del_btn;
    lv_obj_t *power_btn;
    lv_obj_t *effects_parameter_panel;
    lv_obj_t *effect_p1_label;
    lv_obj_t *effect_p2_label;
    lv_obj_t *effect_p3_label;
    lv_obj_t *effect_p4_label;
    lv_obj_t *effect_p5_label;
    lv_obj_t *effect_p6_label;
    lv_obj_t *effect_p7_label;
    lv_obj_t *effect_p1_value;
    lv_obj_t *effect_p2_value;
    lv_obj_t *effect_p3_value;
    lv_obj_t *effect_p4_value;
    lv_obj_t *effect_p5_value;
    lv_obj_t *effect_p6_value;
    lv_obj_t *effect_p7_value;
    lv_obj_t *effect_p1_slider;
    lv_obj_t *effect_p2_slider;
    lv_obj_t *effect_p3_slider;
    lv_obj_t *effect_p4_slider;
    lv_obj_t *effect_p5_slider;
    lv_obj_t *effect_p6_slider;
    lv_obj_t *effect_p7_slider;
    lv_obj_t *obj2;
    lv_obj_t *fx_to_delete;
    lv_obj_t *delete_message;
    lv_obj_t *obj3;
    lv_obj_t *cancel_del_btn;
    lv_obj_t *final_del_btn;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_SPLASH = 1,
    SCREEN_ID_MAIN = 2,
    SCREEN_ID_PARAMETERS = 3,
    SCREEN_ID_DELETEPAGE = 4,
};

void create_screen_splash();
void tick_screen_splash();

void create_screen_main();
void tick_screen_main();

void create_screen_parameters();
void tick_screen_parameters();

void create_screen_deletepage();
void tick_screen_deletepage();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/