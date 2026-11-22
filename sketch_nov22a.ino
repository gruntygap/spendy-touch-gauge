#include <Arduino.h>
#include <lvgl.h>
#include <ST77916_QSPI.h>
#include <CST816S.h>

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// Display QSPI pins
#define TFT_SCLK 42
#define TFT_IO0  38
#define TFT_IO1  39
#define TFT_IO2  40
#define TFT_IO3  41
#define TFT_RST  12
#define TFT_CS   13
#define TFT_BL   11

// Touch I2C pins
#define TP_SDA   5
#define TP_SCL   6
#define TP_RST   1
#define TP_IRQ   2

static constexpr uint32_t SCREEN_WIDTH  = 360;
static constexpr uint32_t SCREEN_HEIGHT = 360;

ST77916_QSPI_GFX tft(TFT_SCLK, TFT_IO0, TFT_IO1, TFT_IO2, TFT_IO3, TFT_RST, TFT_CS, TFT_BL);
CST816S touch(TP_SDA, TP_SCL, TP_RST, TP_IRQ);

static constexpr uint32_t DRAW_BUF_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color16_t);
static uint8_t *draw_buf = nullptr;
static lv_obj_t *brightness_label = nullptr;

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.pushImage(area->x1, area->y1, w, h,
                  reinterpret_cast<const uint16_t *>(px_map),
                  ST77916_PIXELS_NATIVE);

    if (lv_display_flush_is_last(disp)) {
        tft.display();
    }

    lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touch.available() && touch.data.points > 0) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void btn_event_cb(lv_event_t *e) {
    static uint32_t count = 0;
    count++;
    lv_obj_t *btn   = lv_event_get_target_obj(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text_fmt(label, "Clicks: %lu", count);
}

static void brightness_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e);
    int32_t pct = lv_slider_get_value(slider);

    // Map 5%–100% slider range to 13–255 PWM so the screen never goes completely dark
    uint8_t pwm = static_cast<uint8_t>((pct * 255) / 100);
    tft.setBrightness(pwm);

    if (brightness_label) {
        lv_label_set_text_fmt(brightness_label, "Brightness: %ld%%", pct);
    }
}

void build_ui() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Tap counter button positioned slightly above center
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 180, 56);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -45);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap Me");
    lv_obj_center(btn_label);

    // Brightness percentage readout
    brightness_label = lv_label_create(scr);
    lv_label_set_text(brightness_label, "Brightness: 80%");
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xD0DCE8), 0);
    lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, 22);

    // Brightness slider (sized comfortably inside the 360px circular bezel)
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_size(slider, 220, 18);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 55);
    lv_slider_set_range(slider, 5, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

void setup() {
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setTimeOut(50);
    touch.begin();

    if (!tft.begin()) {
        while (true) delay(100);
    }
    tft.setBrightness(204); // Match initial 80% slider value (0.80 * 255)

    lv_init();
    lv_tick_set_cb(millis);

    draw_buf = static_cast<uint8_t *>(
        heap_caps_aligned_alloc(64, DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, nullptr, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    build_ui();
    lv_refr_now(disp);
}

void loop() {
    lv_timer_handler();
    delay(5);
}
