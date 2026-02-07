#include "st7796.h"

/* ===== Low-level with memory barriers for -O3 ===== */

static void lcd_cmd(uint8_t cmd)
{
    LCD_CS_LOW();
    __DSB();  // Data Synchronization Barrier - prevents reordering
    LCD_DC_LOW();
    __DSB();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    __DSB();
    LCD_CS_HIGH();
}

static void lcd_data(const uint8_t *data, uint16_t len)
{
    LCD_CS_LOW();
    __DSB();
    LCD_DC_HIGH();
    __DSB();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, len, HAL_MAX_DELAY);
    __DSB();
    LCD_CS_HIGH();
}

static void lcd_reset(void)
{
    LCD_RST_LOW();
    __DSB();
    HAL_Delay(20);
    LCD_RST_HIGH();
    __DSB();
    HAL_Delay(120);
}

/* ===== Init ===== */

void st7796_init(void)
{
    lcd_reset();

    lcd_cmd(0x01);          // Software reset
    HAL_Delay(120);

    lcd_cmd(0x11);          // Sleep out
    HAL_Delay(120);

    /* Enable extended command set */
    lcd_cmd(0xF0);
    uint8_t f0a = 0xC3;
    lcd_data(&f0a, 1);

    lcd_cmd(0xF0);
    uint8_t f0b = 0x96;
    lcd_data(&f0b, 1);

    /* MADCTL */
    uint8_t madctl = 0x48;  // MX + BGR
    lcd_cmd(0x36);
    lcd_data(&madctl, 1);

    /* RGB565 */
    uint8_t pixfmt = 0x55;
    lcd_cmd(0x3A);
    lcd_data(&pixfmt, 1);

    /* Column inversion */
    uint8_t inv = 0x01;
    lcd_cmd(0xB4);
    lcd_data(&inv, 1);

    /* Display function control */
    uint8_t dfc[] = {0x80, 0x02, 0x3B};
    lcd_cmd(0xB6);
    lcd_data(dfc, 3);

    /* Display output ctrl adjust (per Bodmer TFT_eSPI) */
    uint8_t e8[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    lcd_cmd(0xE8);
    lcd_data(e8, sizeof(e8));

    /* Power control */
    uint8_t c1 = 0x06;
    lcd_cmd(0xC1);
    lcd_data(&c1, 1);

    uint8_t c2 = 0xA7;
    lcd_cmd(0xC2);
    lcd_data(&c2, 1);

    uint8_t vcom = 0x18;
    lcd_cmd(0xC5);
    lcd_data(&vcom, 1);

    /* Gamma positive */
    uint8_t gp[] = {
        0xF0,0x09,0x0B,0x06,0x04,0x15,0x2F,
        0x54,0x42,0x3C,0x17,0x14,0x18,0x1B
    };
    lcd_cmd(0xE0);
    lcd_data(gp, sizeof(gp));

    /* Gamma negative */
    uint8_t gn[] = {
        0xE0,0x09,0x0B,0x06,0x04,0x03,0x2B,
        0x43,0x42,0x3B,0x16,0x14,0x17,0x1B
    };
    lcd_cmd(0xE1);
    lcd_data(gn, sizeof(gn));

    HAL_Delay(120);

    /* Disable extended command set */
    lcd_cmd(0xF0);
    uint8_t f0c = 0x3C;
    lcd_data(&f0c, 1);

    lcd_cmd(0xF0);
    uint8_t f0d = 0x69;
    lcd_data(&f0d, 1);

    HAL_Delay(120);

    lcd_cmd(0x29);          // Display ON
}

/* ===== Drawing ===== */

void st7796_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t d[4];

    lcd_cmd(0x2A);
    d[0]=x0>>8; d[1]=x0;
    d[2]=x1>>8; d[3]=x1;
    lcd_data(d,4);

    lcd_cmd(0x2B);
    d[0]=y0>>8; d[1]=y0;
    d[2]=y1>>8; d[3]=y1;
    lcd_data(d,4);

    lcd_cmd(0x2C);
}

void st7796_write_pixels(uint16_t *pixels, uint32_t count)
{
    // Static buffer at function scope - prevents stack overflow with -O3
    static uint16_t spi_buf[2048] __attribute__((aligned(4)));

    LCD_CS_LOW();
    __DSB();  // Ensure CS is low before DC
    LCD_DC_HIGH();
    __DSB();  // Ensure DC is high before SPI

    while (count)
    {
        uint32_t px = count;
        if (px > 2048)
            px = 2048;

        // Byte swap for big-endian SPI
        for (uint32_t i = 0; i < px; i++)
        {
            spi_buf[i] = __REV16(pixels[i]);
        }

        // Critical: ensure buffer is written before SPI reads it
        __DSB();

        HAL_SPI_Transmit(&hspi1, (uint8_t *)spi_buf, px * 2, HAL_MAX_DELAY);

        pixels += px;
        count  -= px;
    }

    __DSB();  // Wait for SPI to complete
    LCD_CS_HIGH();
}


void st7796_fill_color(uint16_t color)
{
    st7796_set_window(0, 0, ST7796_WIDTH - 1, ST7796_HEIGHT - 1);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    LCD_CS_LOW();
    __DSB();
    LCD_DC_HIGH();
    __DSB();

    for (uint32_t i = 0; i < ST7796_WIDTH * ST7796_HEIGHT; i++)
    {
        uint8_t px[2] = {hi, lo};
        HAL_SPI_Transmit(&hspi1, px, 2, HAL_MAX_DELAY);
    }

    __DSB();
    LCD_CS_HIGH();
}

/* ===== Display Control ===== */

void st7796_invert_colors(uint8_t invert)
{
    lcd_cmd(invert ? 0x21 : 0x20);  // INVON : INVOFF
}

void st7796_set_rotation(uint8_t rotation)
{
    uint8_t madctl;

    switch (rotation % 4)
    {
    case 0:
        madctl = 0x40 | 0x08;  // MX | BGR (portrait)
        break;
    case 1:
        madctl = 0x20 | 0x08;  // MV | BGR (landscape)
        break;
    case 2:
        madctl = 0x80 | 0x08;  // MY | BGR (portrait inverted)
        break;
    case 3:
        madctl = 0x40 | 0x80 | 0x20 | 0x08;  // MX | MY | MV | BGR (landscape inverted)
        break;
    default:
        madctl = 0x48;
        break;
    }

    lcd_cmd(0x36);
    lcd_data(&madctl, 1);
}
