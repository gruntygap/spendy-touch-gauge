#include "display_driver.h"
#include <Arduino.h>
#include <Wire.h>
#include <ST77916_QSPI.h>
#include <CST816S.h>

#define TFT_SCLK 42
#define TFT_IO0  38
#define TFT_IO1  39
#define TFT_IO2  40
#define TFT_IO3  41
#define TFT_RST  12
#define TFT_CS   13
#define TFT_BL   11

#define TP_SDA   5
#define TP_SCL   6
#define TP_RST   1
#define TP_IRQ   2

static ST77916_QSPI_GFX tft(TFT_SCLK, TFT_IO0, TFT_IO1, TFT_IO2, TFT_IO3, TFT_RST, TFT_CS, TFT_BL);
static CST816S touch(TP_SDA, TP_SCL, TP_RST, TP_IRQ);

// 1/4 screen scratchpad in internal DMA SRAM (64.8 KB)
static constexpr uint32_t DRAW_BUF_SIZE = SCREEN_WIDTH * (SCREEN_HEIGHT / 4) * sizeof(lv_color16_t);
static uint8_t *draw_buf = nullptr;

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.pushImage(area->x1, area->y1, w, h,
                  reinterpret_cast<const uint16_t *>(px_map),
                  ST77916_PIXELS_BE);

    if (lv_display_flush_is_last(disp)) {
        tft.display();
    }

    lv_display_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touch.available() && touch.data.points > 0) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool display_driver_init() {
    // Initialize CST816S first so its reset pulse occurs before ST77916 init
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setTimeOut(50);
    touch.begin();

    if (!tft.begin()) {
        return false;
    }
    tft.setBrightness(204);

    lv_init();
    lv_tick_set_cb(millis);

    draw_buf = static_cast<uint8_t *>(
        heap_caps_aligned_alloc(64, DRAW_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    );
    if (!draw_buf) return false;

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, nullptr, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    return true;
}

void display_set_brightness(uint8_t pwm) {
    tft.setBrightness(pwm);
}
