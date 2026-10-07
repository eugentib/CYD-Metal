#include "ui_cyd35.h"

#include <stdio.h>
#include <string.h>

#include "display_st7796.h"

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_CYAN    0x07FF
#define C_GREEN   0x07E0
#define C_YELLOW  0xFFE0
#define C_RED     0xF800
#define C_BLUE    0x001F
#define C_DARK    0x18E3
#define C_GRAY    0x7BEF

static uint16_t clamp_bar(uint16_t v)
{
    return v > 100 ? 100 : v;
}

static esp_err_t draw_button(int x, int y, int w, int h,
                             uint16_t fill, const char *label)
{
    ESP_ERROR_CHECK(display_st7796_fill_rect(x, y, w, h, fill));
    ESP_ERROR_CHECK(display_st7796_fill_rect(x + 2, y + 2, w - 4, h - 4, C_BLACK));
    return display_st7796_draw_text(x + 12, y + 13, 2, C_WHITE, label);
}

esp_err_t ui_cyd35_init(const char *touch_name)
{
    ESP_ERROR_CHECK(display_st7796_fill(C_BLACK));

    ESP_ERROR_CHECK(display_st7796_fill_rect(0, 0, 480, 38, C_CYAN));
    ESP_ERROR_CHECK(display_st7796_draw_text(12, 10, 2, C_BLACK, "CYD-METAL"));
    ESP_ERROR_CHECK(display_st7796_draw_text(322, 12, 1, C_BLACK,
                                             touch_name ? touch_name : "TOUCH"));

    ESP_ERROR_CHECK(display_st7796_draw_text(16, 52, 2, C_WHITE, "SCORE"));
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 122, 1, C_GRAY, "EARLY"));
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 154, 1, C_GRAY, "MID"));
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 186, 1, C_GRAY, "LATE"));

    ESP_ERROR_CHECK(draw_button(10, 264, 145, 48, C_CYAN, "ZERO"));
    ESP_ERROR_CHECK(draw_button(167, 264, 145, 48, C_YELLOW, "MUTE"));
    ESP_ERROR_CHECK(draw_button(324, 264, 145, 48, C_BLUE, "RAW"));

    return ESP_OK;
}

static esp_err_t draw_bar(int y, uint16_t value, uint16_t color)
{
    const int x = 78;
    const int w = 380;
    const int h = 16;
    ESP_ERROR_CHECK(display_st7796_fill_rect(x, y, w, h, C_DARK));

    int fill = (int)((uint32_t)w * clamp_bar(value) / 100U);
    if (fill > 0) {
        ESP_ERROR_CHECK(display_st7796_fill_rect(x, y, fill, h, color));
    }

    char s[12];
    snprintf(s, sizeof(s), "%u", value);
    ESP_ERROR_CHECK(display_st7796_fill_rect(408, y - 1, 50, 18, C_BLACK));
    return display_st7796_draw_text(412, y + 3, 1, C_WHITE, s);
}

esp_err_t ui_cyd35_update(const detector_frame_t *frame,
                          bool muted,
                          bool raw_page,
                          const touch_cyd35_point_t *touch)
{
    if (!frame) {
        return ESP_ERR_INVALID_ARG;
    }

    char s[64];

    ESP_ERROR_CHECK(display_st7796_fill_rect(125, 48, 335, 58, C_BLACK));
    snprintf(s, sizeof(s), "%u", frame->score);
    ESP_ERROR_CHECK(display_st7796_draw_text(130, 54, 5,
                                             frame->score >= 25 ? C_RED :
                                             frame->score >= 8 ? C_YELLOW :
                                             C_GREEN,
                                             s));

    ESP_ERROR_CHECK(draw_bar(120, frame->early, C_CYAN));
    ESP_ERROR_CHECK(draw_bar(152, frame->mid, C_YELLOW));
    ESP_ERROR_CHECK(draw_bar(184, frame->late, C_GREEN));

    ESP_ERROR_CHECK(display_st7796_fill_rect(12, 213, 456, 38, C_BLACK));

    if (raw_page) {
        snprintf(s, sizeof(s), "RAW %u %u %u %u %u",
                 frame->raw[0], frame->raw[1], frame->raw[2],
                 frame->raw[3], frame->raw[4]);
        ESP_ERROR_CHECK(display_st7796_draw_text(16, 218, 1, C_WHITE, s));
    } else {
        snprintf(s, sizeof(s), "PERSIST %u%%  LATE %u",
                 frame->persistence_pct, frame->late_samples);
        ESP_ERROR_CHECK(display_st7796_draw_text(16, 218, 1, C_WHITE, s));
    }

    /* Mute state indicator inside the middle button. */
    ESP_ERROR_CHECK(display_st7796_fill_rect(260, 278, 38, 18, C_BLACK));
    ESP_ERROR_CHECK(display_st7796_draw_text(262, 281, 1,
                                             muted ? C_RED : C_GREEN,
                                             muted ? "OFF" : "ON"));

    return ESP_OK;
}
