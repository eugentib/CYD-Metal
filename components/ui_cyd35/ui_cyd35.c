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

static char s_touch_name[16] = "TOUCH";
static bool s_was_settings;
static uint32_t s_width_shown;
static uint16_t s_first_shown;
static uint8_t s_avg_shown;

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
    if (touch_name) {
        snprintf(s_touch_name, sizeof(s_touch_name), "%s", touch_name);
    }
    s_was_settings = false;
    ESP_ERROR_CHECK(display_st7796_fill(C_BLACK));

    ESP_ERROR_CHECK(display_st7796_fill_rect(0, 0, 480, 38, C_CYAN));
    ESP_ERROR_CHECK(display_st7796_draw_text(12, 10, 2, C_BLACK, "CYD-METAL"));
    ESP_ERROR_CHECK(display_st7796_draw_text(322, 12, 1, C_BLACK, s_touch_name));
    ESP_ERROR_CHECK(display_st7796_fill_rect(402, 4, 74, 30, C_BLUE));
    ESP_ERROR_CHECK(display_st7796_draw_text(419, 12, 2, C_WHITE, "SET"));

    ESP_ERROR_CHECK(display_st7796_draw_text(16, 52, 2, C_WHITE, "SCORE"));
    /* Timing labels are redrawn on each live update. */

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

static esp_err_t draw_config_button(int x, int y, int w, int h,
                                     uint16_t value, bool selected)
{
    char label[16];
    snprintf(label, sizeof(label), "%uUS", value);
    ESP_ERROR_CHECK(display_st7796_fill_rect(
        x, y, w, h, selected ? C_GREEN : C_DARK));
    return display_st7796_draw_text(
        x + 13, y + 12, 2, selected ? C_BLACK : C_WHITE, label);
}

static esp_err_t draw_settings(const detector_config_t *cfg)
{
    static const uint16_t widths[4] = {25, 50, 75, 100};
    static const uint16_t starts[8] = {10, 15, 20, 25, 30, 40, 50, 60};
    ESP_ERROR_CHECK(display_st7796_fill(C_BLACK));

    ESP_ERROR_CHECK(display_st7796_fill_rect(0, 0, 480, 38, C_CYAN));
    ESP_ERROR_CHECK(display_st7796_draw_text(12, 10, 2, C_BLACK, "PI SETTINGS"));
    ESP_ERROR_CHECK(display_st7796_fill_rect(402, 4, 74, 30, C_BLUE));
    ESP_ERROR_CHECK(display_st7796_draw_text(413, 12, 2, C_WHITE, "BACK"));

    ESP_ERROR_CHECK(display_st7796_draw_text(
        12, 53, 2, C_WHITE, "TX WIDTH (US)"));
    for (int i = 0; i < 4; ++i) {
        ESP_ERROR_CHECK(draw_config_button(
            8 + 117 * i, 79, 108, 45, widths[i],
            cfg->pulse_width_us == widths[i]));
    }

    ESP_ERROR_CHECK(display_st7796_draw_text(
        12, 140, 2, C_WHITE, "FIRST ADC TAP (US)"));
    for (int i = 0; i < 8; ++i) {
        ESP_ERROR_CHECK(draw_config_button(
            8 + (i % 4) * 117, 163 + (i / 4) * 45, 108, 40,
            starts[i], cfg->tap_us[0] == starts[i]));
    }

    ESP_ERROR_CHECK(draw_button(
        10, 264, 145, 48, cfg->moving_avg_frames == 5 ? C_GREEN : C_BLUE,
        "AVG 5"));
    ESP_ERROR_CHECK(draw_button(
        167, 264, 145, 48, cfg->moving_avg_frames == 10 ? C_GREEN : C_BLUE,
        "AVG 10"));
    ESP_ERROR_CHECK(draw_button(324, 264, 145, 48, C_CYAN, "BACK"));
    return ESP_OK;
}

esp_err_t ui_cyd35_update(const detector_frame_t *frame,
                          bool muted,
                          bool raw_page,
                          bool settings_page,
                          const detector_config_t *config,
                          const touch_cyd35_point_t *touch)
{
    (void)touch;
    if (!frame || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (settings_page) {
        if (!s_was_settings ||
            s_width_shown != config->pulse_width_us ||
            s_first_shown != config->tap_us[0] ||
            s_avg_shown != config->moving_avg_frames) {
            ESP_ERROR_CHECK(draw_settings(config));
        }
        s_was_settings = true;
        s_width_shown = config->pulse_width_us;
        s_first_shown = config->tap_us[0];
        s_avg_shown = config->moving_avg_frames;
        return ESP_OK;
    }
    if (s_was_settings) {
        ESP_ERROR_CHECK(ui_cyd35_init(s_touch_name));
    }

    char s[80];

    ESP_ERROR_CHECK(display_st7796_fill_rect(125, 48, 335, 58, C_BLACK));
    snprintf(s, sizeof(s), "%u", frame->score);
    ESP_ERROR_CHECK(display_st7796_draw_text(130, 54, 5,
                                             frame->score >= 25 ? C_RED :
                                             frame->score >= 8 ? C_YELLOW :
                                             C_GREEN,
                                             s));

    ESP_ERROR_CHECK(display_st7796_fill_rect(12, 117, 65, 94, C_BLACK));
    snprintf(s, sizeof(s), "E %uUS", config->tap_us[0]);
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 122, 1, C_GRAY, s));
    snprintf(s, sizeof(s), "M %uUS", config->tap_us[1]);
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 154, 1, C_GRAY, s));
    snprintf(s, sizeof(s), "L %uUS", config->tap_us[2]);
    ESP_ERROR_CHECK(display_st7796_draw_text(16, 186, 1, C_GRAY, s));

    ESP_ERROR_CHECK(draw_bar(120, frame->early, C_CYAN));
    ESP_ERROR_CHECK(draw_bar(152, frame->mid, C_YELLOW));
    ESP_ERROR_CHECK(draw_bar(184, frame->late, C_GREEN));

    ESP_ERROR_CHECK(display_st7796_fill_rect(12, 213, 456, 38, C_BLACK));

    if (raw_page) {
        snprintf(s, sizeof(s), "RAW %u:%u %u:%u %u:%u",
                 config->tap_us[0], frame->raw[0],
                 config->tap_us[1], frame->raw[1],
                 config->tap_us[2], frame->raw[2]);
        ESP_ERROR_CHECK(display_st7796_draw_text(16, 218, 1, C_WHITE, s));
    } else {
        snprintf(s, sizeof(s), "AVG %u/%u  LATE %u MAX %uUS",
                 frame->frames_averaged, config->moving_avg_frames,
                 frame->late_samples, frame->max_late_us);
        ESP_ERROR_CHECK(display_st7796_draw_text(16, 218, 1, C_WHITE, s));
    }

    /* Mute state indicator inside the middle button. */
    ESP_ERROR_CHECK(display_st7796_fill_rect(260, 278, 38, 18, C_BLACK));
    ESP_ERROR_CHECK(display_st7796_draw_text(262, 281, 1,
                                             muted ? C_RED : C_GREEN,
                                             muted ? "OFF" : "ON"));

    return ESP_OK;
}
