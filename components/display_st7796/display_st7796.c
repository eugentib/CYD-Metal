#include "display_st7796.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_rom_sys.h"

#define TAG "st7796"

#define LCD_HOST        SPI2_HOST
#define LCD_PIN_MISO    GPIO_NUM_12
#define LCD_PIN_MOSI    GPIO_NUM_13
#define LCD_PIN_SCLK    GPIO_NUM_14
#define LCD_PIN_CS      GPIO_NUM_15
#define LCD_PIN_DC      GPIO_NUM_2
#define LCD_PIN_BL      GPIO_NUM_27

#define LCD_SPI_HZ      (40 * 1000 * 1000)

static spi_device_handle_t s_lcd;

static esp_err_t lcd_tx(const void *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_polling_transmit(s_lcd, &t);
}

static esp_err_t lcd_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_PIN_DC, 0);
    return lcd_tx(&cmd, 1);
}

static esp_err_t lcd_data(const void *data, size_t len)
{
    gpio_set_level(LCD_PIN_DC, 1);
    return lcd_tx(data, len);
}

static esp_err_t lcd_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_ERROR(lcd_cmd(cmd), TAG, "command 0x%02x failed", cmd);
    if (len) {
        ESP_RETURN_ON_ERROR(lcd_data(data, len), TAG, "data for 0x%02x failed", cmd);
    }
    return ESP_OK;
}

static esp_err_t lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t col[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    uint8_t row[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };

    ESP_RETURN_ON_ERROR(lcd_cmd_data(0x2A, col, sizeof(col)), TAG, "CASET failed");
    ESP_RETURN_ON_ERROR(lcd_cmd_data(0x2B, row, sizeof(row)), TAG, "RASET failed");
    ESP_RETURN_ON_ERROR(lcd_cmd(0x2C), TAG, "RAMWR failed");
    return ESP_OK;
}

esp_err_t display_st7796_init(void)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << LCD_PIN_DC) | (1ULL << LCD_PIN_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_cfg), TAG, "LCD GPIO config failed");

    /* Backlight is active-high on the 3.5-inch ESP32-3248S035 board. */
    gpio_set_level(LCD_PIN_BL, 0);

    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CYD_LCD_WIDTH * 20 * 2,
    };

    esp_err_t err = spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = LCD_SPI_HZ,
        .mode = 0,
        .spics_io_num = LCD_PIN_CS,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(LCD_HOST, &dev_cfg, &s_lcd),
        TAG, "SPI device add failed");

    /* ST7796 initialization sequence used by known-good CYD 3.5 drivers. */
    ESP_RETURN_ON_ERROR(lcd_cmd(0x01), TAG, "SWRESET failed");
    esp_rom_delay_us(120000);

    ESP_RETURN_ON_ERROR(lcd_cmd(0x11), TAG, "SLPOUT failed");
    esp_rom_delay_us(120000);

    {
        const uint8_t d[] = {0x55}; /* RGB565 */
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0x3A, d, sizeof(d)), TAG, "COLMOD failed");
    }
    {
        const uint8_t d[] = {0xC3};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xF0, d, sizeof(d)), TAG, "F0 unlock 1 failed");
    }
    {
        const uint8_t d[] = {0x96};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xF0, d, sizeof(d)), TAG, "F0 unlock 2 failed");
    }
    {
        const uint8_t d[] = {0x01};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xB4, d, sizeof(d)), TAG, "INVCTR failed");
    }
    {
        const uint8_t d[] = {0x80, 0x22, 0x3B};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xB6, d, sizeof(d)), TAG, "DISPCTRL failed");
    }
    {
        const uint8_t d[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xE8, d, sizeof(d)), TAG, "E8 failed");
    }
    {
        const uint8_t d[] = {0x06};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xC1, d, sizeof(d)), TAG, "PWCTRL2 failed");
    }
    {
        const uint8_t d[] = {0xA7};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xC2, d, sizeof(d)), TAG, "PWCTRL3 failed");
    }
    {
        const uint8_t d[] = {0x18};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xC5, d, sizeof(d)), TAG, "VCOM failed");
    }
    {
        const uint8_t d[] = {
            0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
            0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B
        };
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xE0, d, sizeof(d)), TAG, "PGAMCTRL failed");
    }
    {
        const uint8_t d[] = {
            0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
            0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B
        };
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xE1, d, sizeof(d)), TAG, "NGAMCTRL failed");
    }
    {
        const uint8_t d[] = {0x3C};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xF0, d, sizeof(d)), TAG, "F0 relock 1 failed");
    }
    {
        const uint8_t d[] = {0x69};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0xF0, d, sizeof(d)), TAG, "F0 relock 2 failed");
    }

    /* Landscape rotation: MX + MV + BGR. */
    {
        const uint8_t d[] = {0x68};
        ESP_RETURN_ON_ERROR(lcd_cmd_data(0x36, d, sizeof(d)), TAG, "MADCTL failed");
    }

    ESP_RETURN_ON_ERROR(lcd_cmd(0x38), TAG, "IDMOFF failed");
    ESP_RETURN_ON_ERROR(lcd_cmd(0x21), TAG, "INVON failed");
    ESP_RETURN_ON_ERROR(lcd_cmd(0x29), TAG, "DISPON failed");
    esp_rom_delay_us(120000);

    gpio_set_level(LCD_PIN_BL, 1);
    return ESP_OK;
}

esp_err_t display_st7796_fill_rect(int x, int y, int w, int h, uint16_t rgb565)
{
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > CYD_LCD_WIDTH)  w = CYD_LCD_WIDTH - x;
    if (y + h > CYD_LCD_HEIGHT) h = CYD_LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(
        lcd_set_window(x, y, x + w - 1, y + h - 1),
        TAG, "set window failed");

    /*
     * LCD wants RGB565 MSB first. Store a byte-swapped 16-bit word so the
     * ESP32's little-endian memory appears on the wire in the correct order.
     */
    const uint16_t be = (uint16_t)((rgb565 << 8) | (rgb565 >> 8));
    static uint16_t line[CYD_LCD_WIDTH];

    for (int i = 0; i < w; ++i) {
        line[i] = be;
    }

    gpio_set_level(LCD_PIN_DC, 1);
    for (int row = 0; row < h; ++row) {
        ESP_RETURN_ON_ERROR(lcd_tx(line, (size_t)w * 2U), TAG, "pixel transfer failed");
    }
    return ESP_OK;
}

esp_err_t display_st7796_fill(uint16_t rgb565)
{
    return display_st7796_fill_rect(0, 0, CYD_LCD_WIDTH, CYD_LCD_HEIGHT, rgb565);
}

static void glyph5x7(char c, uint8_t g[5])
{
    memset(g, 0, 5);

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

#define GLYPH(a,b,c_,d,e) do { g[0]=(a); g[1]=(b); g[2]=(c_); g[3]=(d); g[4]=(e); } while (0)
    switch (c) {
    case '0': GLYPH(0x3E,0x51,0x49,0x45,0x3E); break;
    case '1': GLYPH(0x00,0x42,0x7F,0x40,0x00); break;
    case '2': GLYPH(0x42,0x61,0x51,0x49,0x46); break;
    case '3': GLYPH(0x21,0x41,0x45,0x4B,0x31); break;
    case '4': GLYPH(0x18,0x14,0x12,0x7F,0x10); break;
    case '5': GLYPH(0x27,0x45,0x45,0x45,0x39); break;
    case '6': GLYPH(0x3C,0x4A,0x49,0x49,0x30); break;
    case '7': GLYPH(0x01,0x71,0x09,0x05,0x03); break;
    case '8': GLYPH(0x36,0x49,0x49,0x49,0x36); break;
    case '9': GLYPH(0x06,0x49,0x49,0x29,0x1E); break;

    case 'A': GLYPH(0x7E,0x11,0x11,0x11,0x7E); break;
    case 'B': GLYPH(0x7F,0x49,0x49,0x49,0x36); break;
    case 'C': GLYPH(0x3E,0x41,0x41,0x41,0x22); break;
    case 'D': GLYPH(0x7F,0x41,0x41,0x22,0x1C); break;
    case 'E': GLYPH(0x7F,0x49,0x49,0x49,0x41); break;
    case 'F': GLYPH(0x7F,0x09,0x09,0x09,0x01); break;
    case 'G': GLYPH(0x3E,0x41,0x49,0x49,0x7A); break;
    case 'H': GLYPH(0x7F,0x08,0x08,0x08,0x7F); break;
    case 'I': GLYPH(0x00,0x41,0x7F,0x41,0x00); break;
    case 'J': GLYPH(0x20,0x40,0x41,0x3F,0x01); break;
    case 'K': GLYPH(0x7F,0x08,0x14,0x22,0x41); break;
    case 'L': GLYPH(0x7F,0x40,0x40,0x40,0x40); break;
    case 'M': GLYPH(0x7F,0x02,0x0C,0x02,0x7F); break;
    case 'N': GLYPH(0x7F,0x04,0x08,0x10,0x7F); break;
    case 'O': GLYPH(0x3E,0x41,0x41,0x41,0x3E); break;
    case 'P': GLYPH(0x7F,0x09,0x09,0x09,0x06); break;
    case 'Q': GLYPH(0x3E,0x41,0x51,0x21,0x5E); break;
    case 'R': GLYPH(0x7F,0x09,0x19,0x29,0x46); break;
    case 'S': GLYPH(0x46,0x49,0x49,0x49,0x31); break;
    case 'T': GLYPH(0x01,0x01,0x7F,0x01,0x01); break;
    case 'U': GLYPH(0x3F,0x40,0x40,0x40,0x3F); break;
    case 'V': GLYPH(0x1F,0x20,0x40,0x20,0x1F); break;
    case 'W': GLYPH(0x3F,0x40,0x38,0x40,0x3F); break;
    case 'X': GLYPH(0x63,0x14,0x08,0x14,0x63); break;
    case 'Y': GLYPH(0x07,0x08,0x70,0x08,0x07); break;
    case 'Z': GLYPH(0x61,0x51,0x49,0x45,0x43); break;

    case '-': GLYPH(0x08,0x08,0x08,0x08,0x08); break;
    case ':': GLYPH(0x00,0x36,0x36,0x00,0x00); break;
    case '%': GLYPH(0x62,0x64,0x08,0x13,0x23); break;
    case '/': GLYPH(0x20,0x10,0x08,0x04,0x02); break;
    case '.': GLYPH(0x00,0x60,0x60,0x00,0x00); break;
    case '_': GLYPH(0x40,0x40,0x40,0x40,0x40); break;
    case '?': GLYPH(0x02,0x01,0x51,0x09,0x06); break;
    case ' ': default: break;
    }
#undef GLYPH
}

esp_err_t display_st7796_draw_text(int x, int y, int scale,
                                   uint16_t rgb565, const char *text)
{
    if (!text || scale <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int cursor = x;
    while (*text) {
        uint8_t g[5];
        glyph5x7(*text++, g);

        for (int row = 0; row < 7; ++row) {
            int run_start = -1;
            for (int col = 0; col <= 5; ++col) {
                bool on = col < 5 && ((g[col] >> row) & 1U);
                if (on && run_start < 0) {
                    run_start = col;
                } else if (!on && run_start >= 0) {
                    ESP_RETURN_ON_ERROR(
                        display_st7796_fill_rect(
                            cursor + run_start * scale,
                            y + row * scale,
                            (col - run_start) * scale,
                            scale,
                            rgb565),
                        TAG, "text draw failed");
                    run_start = -1;
                }
            }
        }

        cursor += 6 * scale;
        if (cursor >= CYD_LCD_WIDTH) {
            break;
        }
    }
    return ESP_OK;
}

esp_err_t display_st7796_draw_bringup_screen(void)
{
    /* RGB565 colors */
    const uint16_t black = 0x0000;
    const uint16_t cyan  = 0x07FF;
    const uint16_t green = 0x07E0;
    const uint16_t red   = 0xF800;
    const uint16_t blue  = 0x001F;
    const uint16_t white = 0xFFFF;

    ESP_RETURN_ON_ERROR(display_st7796_fill(black), TAG, "clear failed");

    /* Simple static M0 test pattern. No font/UI dependency yet. */
    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(0, 0, 480, 36, cyan), TAG, "header failed");
    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(20, 70, 440, 70, white), TAG, "panel failed");
    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(24, 74, 432, 62, black), TAG, "panel inset failed");

    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(40, 174, 120, 56, red), TAG, "red bar failed");
    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(180, 174, 120, 56, green), TAG, "green bar failed");
    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(320, 174, 120, 56, blue), TAG, "blue bar failed");

    ESP_RETURN_ON_ERROR(display_st7796_fill_rect(20, 270, 440, 16, cyan), TAG, "footer failed");
    return ESP_OK;
}
