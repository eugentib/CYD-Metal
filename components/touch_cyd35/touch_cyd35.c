#include "touch_cyd35.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"

#define TAG "touch"

#define TOUCH_SPI_HOST      SPI2_HOST
#define XPT_CS_GPIO         GPIO_NUM_33
#define XPT_IRQ_GPIO        GPIO_NUM_36
#define XPT_SPI_HZ          (2 * 1000 * 1000)

#define GT_I2C_PORT         I2C_NUM_0
#define GT_SDA_GPIO         GPIO_NUM_33
#define GT_SCL_GPIO         GPIO_NUM_32
#define GT_RST_GPIO         GPIO_NUM_25
#define GT_INT_GPIO         GPIO_NUM_21
#define GT_I2C_HZ           400000

#define GT_REG_PRODUCT_ID   0x8140
#define GT_REG_STATUS       0x814E
#define GT_REG_POINT1       0x8150

static touch_cyd35_type_t s_type = TOUCH_CYD35_NONE;
static spi_device_handle_t s_xpt;
static uint8_t s_gt_addr;

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t map_u16(uint16_t v, uint16_t in_lo, uint16_t in_hi,
                        uint16_t out_lo, uint16_t out_hi)
{
    int32_t x = clamp_i(v, in_lo, in_hi);
    int32_t num = (x - in_lo) * ((int32_t)out_hi - out_lo);
    int32_t den = (int32_t)in_hi - in_lo;
    return (uint16_t)(out_lo + num / den);
}

static esp_err_t gt_read(uint16_t reg, void *data, size_t len)
{
    uint8_t a[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_write_read_device(
        GT_I2C_PORT, s_gt_addr, a, sizeof(a), data, len, pdMS_TO_TICKS(20));
}

static esp_err_t gt_write_u8(uint16_t reg, uint8_t value)
{
    uint8_t b[3] = {(uint8_t)(reg >> 8), (uint8_t)reg, value};
    return i2c_master_write_to_device(
        GT_I2C_PORT, s_gt_addr, b, sizeof(b), pdMS_TO_TICKS(20));
}

static bool gt_try_address(uint8_t addr)
{
    s_gt_addr = addr;
    uint8_t id[4] = {0};
    if (gt_read(GT_REG_PRODUCT_ID, id, sizeof(id)) != ESP_OK) {
        return false;
    }

    ESP_LOGI(TAG, "GT911 detected at 0x%02X, product id='%c%c%c%c'",
             addr,
             id[0] ? id[0] : '?', id[1] ? id[1] : '?',
             id[2] ? id[2] : '?', id[3] ? id[3] : '?');
    return true;
}

static esp_err_t try_gt911(void)
{
    const i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GT_SDA_GPIO,
        .scl_io_num = GT_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = GT_I2C_HZ,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(GT_I2C_PORT, &cfg), TAG,
                        "GT911 I2C config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(
                            GT_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0),
                        TAG, "GT911 I2C driver install failed");

    /*
     * GT911 address-selection/reset sequence. On the resistive board these
     * pins are not connected to a GT911, so this is harmless.
     */
    const gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << GT_RST_GPIO) | (1ULL << GT_INT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);
    gpio_set_level(GT_RST_GPIO, 0);
    gpio_set_level(GT_INT_GPIO, 0);
    esp_rom_delay_us(10000);
    gpio_set_level(GT_RST_GPIO, 1);
    esp_rom_delay_us(50000);

    gpio_set_direction(GT_INT_GPIO, GPIO_MODE_INPUT);

    if (gt_try_address(0x5D) || gt_try_address(0x14)) {
        s_type = TOUCH_CYD35_GT911;
        return ESP_OK;
    }

    i2c_driver_delete(GT_I2C_PORT);
    s_gt_addr = 0;
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t xpt_read12(uint8_t command, uint16_t *value)
{
    uint8_t tx[3] = {command, 0x00, 0x00};
    uint8_t rx[3] = {0};

    spi_transaction_t t = {
        .length = 24,
        .rxlength = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_xpt, &t),
                        TAG, "XPT2046 SPI transaction failed");

    *value = (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3) & 0x0FFF;
    return ESP_OK;
}

static void sort5(uint16_t a[5])
{
    for (int i = 1; i < 5; ++i) {
        uint16_t v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = v;
    }
}

static esp_err_t init_xpt2046(void)
{
    const gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << XPT_IRQ_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&irq_cfg), TAG, "XPT IRQ config failed");

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = XPT_SPI_HZ,
        .mode = 0,
        .spics_io_num = XPT_CS_GPIO,
        .queue_size = 1,
    };

    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(TOUCH_SPI_HOST, &dev_cfg, &s_xpt),
        TAG, "XPT2046 add SPI device failed");

    s_type = TOUCH_CYD35_XPT2046;
    ESP_LOGI(TAG,
             "Using XPT2046 resistive touch: CS=33 IRQ=36, shared SPI 12/13/14");
    return ESP_OK;
}

esp_err_t touch_cyd35_init(void)
{
    s_type = TOUCH_CYD35_NONE;

    esp_err_t err = try_gt911();
    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "Using GT911 capacitive touch: SDA=33 SCL=32 RST=25 INT=21");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "GT911 not found; trying XPT2046 resistive touch");
    return init_xpt2046();
}

static esp_err_t read_xpt(touch_cyd35_point_t *p)
{
    /*
     * IRQ is active-low. Some board revisions are noisy, so when asserted we
     * take five samples and use the median.
     */
    if (gpio_get_level(XPT_IRQ_GPIO) != 0) {
        p->touched = false;
        return ESP_OK;
    }

    uint16_t xs[5], ys[5];
    for (int i = 0; i < 5; ++i) {
        ESP_RETURN_ON_ERROR(xpt_read12(0xD0, &xs[i]), TAG, "X read failed");
        ESP_RETURN_ON_ERROR(xpt_read12(0x90, &ys[i]), TAG, "Y read failed");
    }
    sort5(xs);
    sort5(ys);

    p->raw_x = xs[2];
    p->raw_y = ys[2];

    /*
     * First-pass calibration for ESP32-3248S035R.
     * The display is in landscape. ESPHome configurations for this board use
     * swap_xy + mirror_x, which corresponds to the transform below.
     * Serial logging exposes raw values so calibration can be refined.
     */
    uint16_t mx = map_u16(p->raw_y, 200, 3900, 0, 479);
    uint16_t my = map_u16(p->raw_x, 200, 3900, 0, 319);
    p->x = 479 - mx;
    p->y = my;
    p->touched = true;
    return ESP_OK;
}

static esp_err_t read_gt(touch_cyd35_point_t *p)
{
    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(gt_read(GT_REG_STATUS, &status, 1), TAG,
                        "GT911 status read failed");

    const uint8_t count = status & 0x0F;
    if ((status & 0x80) == 0 || count == 0) {
        p->touched = false;
        return ESP_OK;
    }

    uint8_t d[8] = {0};
    ESP_RETURN_ON_ERROR(gt_read(GT_REG_POINT1, d, sizeof(d)), TAG,
                        "GT911 point read failed");

    p->raw_x = (uint16_t)d[1] | ((uint16_t)d[2] << 8);
    p->raw_y = (uint16_t)d[3] | ((uint16_t)d[4] << 8);

    /*
     * GT911 sensor coordinates are normally portrait (320x480), while our
     * ST7796 is rotated to 480x320 landscape.
     */
    if (p->raw_x <= 320 && p->raw_y <= 480) {
        p->x = (p->raw_y > 479) ? 479 : p->raw_y;
        p->y = (p->raw_x > 319) ? 0 : (uint16_t)(319 - p->raw_x);
    } else {
        p->x = (p->raw_x > 479) ? 479 : p->raw_x;
        p->y = (p->raw_y > 319) ? 319 : p->raw_y;
    }

    p->touched = true;

    /* Acknowledge/clear the data-ready status. */
    (void)gt_write_u8(GT_REG_STATUS, 0);
    return ESP_OK;
}

esp_err_t touch_cyd35_read(touch_cyd35_point_t *point)
{
    if (!point) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(point, 0, sizeof(*point));

    switch (s_type) {
    case TOUCH_CYD35_XPT2046:
        return read_xpt(point);
    case TOUCH_CYD35_GT911:
        return read_gt(point);
    default:
        point->touched = false;
        return ESP_OK;
    }
}

touch_cyd35_type_t touch_cyd35_type(void)
{
    return s_type;
}

const char *touch_cyd35_type_name(void)
{
    switch (s_type) {
    case TOUCH_CYD35_XPT2046: return "XPT2046";
    case TOUCH_CYD35_GT911:   return "GT911";
    default:                  return "NONE";
    }
}
