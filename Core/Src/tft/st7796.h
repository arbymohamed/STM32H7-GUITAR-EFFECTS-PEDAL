#pragma once
#include "stm32h7xx_hal.h"
#include "main.h"
#include <stdint.h>

extern SPI_HandleTypeDef hspi1;

/* Resolution */
#define ST7796_WIDTH   480
#define ST7796_HEIGHT  320

/* GPIO macros (from CubeMX) */
#define LCD_CS_LOW()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port,  LCD_CS_Pin,  GPIO_PIN_SET)

#define LCD_DC_LOW()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_RESET)
#define LCD_DC_HIGH()  HAL_GPIO_WritePin(LCD_DC_GPIO_Port,  LCD_DC_Pin,  GPIO_PIN_SET)

#define LCD_RST_LOW()  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET)

/* API */
void st7796_init(void);
void st7796_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void st7796_write_pixels(uint16_t *pixels, uint32_t count);
void st7796_fill_color(uint16_t color);
void st7796_invert_colors(uint8_t invert);
void st7796_set_rotation(uint8_t rotation);

/* Color macros (RGB565 format) */
#define ST7796_BLACK       0x0000
#define ST7796_WHITE       0xFFFF
#define ST7796_RED         0xF800
#define ST7796_GREEN       0x07E0
#define ST7796_BLUE        0x001F
#define ST7796_CYAN        0x07FF
#define ST7796_MAGENTA     0xF81F
#define ST7796_YELLOW      0xFFE0
