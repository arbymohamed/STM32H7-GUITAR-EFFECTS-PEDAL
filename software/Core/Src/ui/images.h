#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_play;
extern const lv_img_dsc_t img_record;
extern const lv_img_dsc_t img_pause;
extern const lv_img_dsc_t img_undo;
extern const lv_img_dsc_t img_loop_button_icon;
extern const lv_img_dsc_t img_logo;
extern const lv_img_dsc_t img_redo;
extern const lv_img_dsc_t img_settings;
extern const lv_img_dsc_t img_amps;
extern const lv_img_dsc_t img_usb;
extern const lv_img_dsc_t img_looper;
extern const lv_img_dsc_t img_fx_chain;
extern const lv_img_dsc_t img_cabinet;
extern const lv_img_dsc_t img_return;
extern const lv_img_dsc_t img_delete;
extern const lv_img_dsc_t img_power;
extern const lv_img_dsc_t img_plus;
extern const lv_img_dsc_t img_delete_2;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[18];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/