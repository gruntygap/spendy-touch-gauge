#include <Arduino.h>
#include <lvgl.h>
#include <ST77916_QSPI.h>
#include <CST816S.h>

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// Display QSPI pins
#define TFT_SCLK 42
#define TFT_IO0 38
#define TFT_IO1 39
#define TFT_IO2 40
#define TFT_IO3 41
#define TFT_RST 12
#define TFT_CS 13
#define TFT_BL 11

// Touch I2C pins
#define TP_SDA 5
#define TP_SCL 6
#define TP_RST 1
#define TP_IRQ 2

static constexpr int32_t SCREEN_WIDTH = 360;
static constexpr int32_t SCREEN_HEIGHT = 360;

ST77916_QSPI_GFX tft(TFT_SCLK, TFT_IO0, TFT_IO1, TFT_IO2, TFT_IO3, TFT_RST, TFT_CS, TFT_BL);
CST816S touch(TP_SDA, TP_SCL, TP_RST, TP_IRQ);

// static constexpr uint32_t DRAW_BUF_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color16_t);

// make drawbuf 1/8 the size
static constexpr uint32_t DRAW_BUF_SIZE = SCREEN_WIDTH * (SCREEN_HEIGHT / 8) * sizeof(lv_color16_t);
static uint8_t *draw_buf = nullptr;

static lv_obj_t *settings_drawer = nullptr;
static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *bottom_pill = nullptr;
static lv_timer_t *pill_hide_timer = nullptr;
static bool drawer_open = false;

// Drag tracking state
static int32_t drag_start_finger_y = 0;
static int32_t drag_start_drawer_y = SCREEN_HEIGHT;
static bool is_dragging_drawer = false;

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
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touch.data.x;
    data->point.y = touch.data.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// -------------------------------------------------------------------------
// Bottom Pill Fade-In / Auto-Hide Logic
// -------------------------------------------------------------------------
static void anim_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(var), static_cast<lv_opa_t>(v), 0);
}

static void fade_bottom_pill(bool show) {
    if (!bottom_pill) return;

    lv_opa_t target_opa  = show ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_opa_t current_opa = lv_obj_get_style_bg_opa(bottom_pill, LV_PART_MAIN);
    if (current_opa == target_opa) return;

    lv_anim_delete(bottom_pill, anim_opa_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bottom_pill);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, current_opa, target_opa);
    lv_anim_set_duration(&a, show ? 150 : 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}


static void pill_hide_timer_cb(lv_timer_t *timer) {
  // Keep hidden if drawer is open or actively being dragged
  if (!drawer_open && !is_dragging_drawer) {
    fade_bottom_pill(false);
  }
  lv_timer_pause(timer);
}

static void reveal_bottom_pill_temporarily() {
  if (drawer_open) return;

  fade_bottom_pill(true);

  if (!pill_hide_timer) {
    pill_hide_timer = lv_timer_create(pill_hide_timer_cb, 2500, nullptr);
  } else {
    lv_timer_reset(pill_hide_timer);
    lv_timer_resume(pill_hide_timer);
  }
}

static void screen_touch_wake_cb(lv_event_t *e) {
  reveal_bottom_pill_temporarily();
}

// -------------------------------------------------------------------------
// Smooth Snap Animation for Settings Drawer
// -------------------------------------------------------------------------
static void anim_y_cb(void *var, int32_t v) {
  lv_obj_set_y(static_cast<lv_obj_t *>(var), v);
}

void animate_drawer_to(bool open) {
  drawer_open = open;
  int32_t target_y = open ? 0 : SCREEN_HEIGHT;
  int32_t current_y = lv_obj_get_y(settings_drawer);

  if (open) {
    fade_bottom_pill(false);
  } else {
    reveal_bottom_pill_temporarily();
  }

  if (current_y == target_y) return;

  uint32_t dist = static_cast<uint32_t>(abs(target_y - current_y));
  uint32_t duration = LV_CLAMP(120, (dist * 240) / SCREEN_HEIGHT, 240);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, settings_drawer);
  lv_anim_set_exec_cb(&a, anim_y_cb);
  lv_anim_set_values(&a, current_y, target_y);
  lv_anim_set_duration(&a, duration);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

// -------------------------------------------------------------------------
// Interactive Finger-Tracking Drag Handler
// -------------------------------------------------------------------------
static void drawer_drag_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  if (!indev || !settings_drawer) return;

  lv_point_t p;
  lv_indev_get_point(indev, &p);

  if (code == LV_EVENT_PRESSED) {
    lv_anim_delete(settings_drawer, anim_y_cb);
    drag_start_finger_y = p.y;
    drag_start_drawer_y = lv_obj_get_y(settings_drawer);
    is_dragging_drawer = true;
    reveal_bottom_pill_temporarily();
  } else if (code == LV_EVENT_PRESSING && is_dragging_drawer) {
    int32_t delta_y = p.y - drag_start_finger_y;
    int32_t new_y = LV_CLAMP(0, drag_start_drawer_y + delta_y, SCREEN_HEIGHT);
    lv_obj_set_y(settings_drawer, new_y);
  } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && is_dragging_drawer) {
    is_dragging_drawer = false;
    int32_t total_delta = p.y - drag_start_finger_y;
    int32_t current_y = lv_obj_get_y(settings_drawer);

    if (total_delta < -25) {
      animate_drawer_to(true);
    } else if (total_delta > 25) {
      animate_drawer_to(false);
    } else {
      animate_drawer_to(current_y < (SCREEN_HEIGHT / 2));
    }
  }
}

// -------------------------------------------------------------------------
// Main Screen & Settings Controls Callbacks
// -------------------------------------------------------------------------
static void btn_event_cb(lv_event_t *e) {
  static uint32_t count = 0;
  count++;
  lv_obj_t *btn = lv_event_get_target_obj(e);
  lv_obj_t *label = lv_obj_get_child(btn, 0);
  lv_label_set_text_fmt(label, "Clicks: %lu", count);
}

static void brightness_slider_cb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target_obj(e);
  int32_t pct = lv_slider_get_value(slider);

  uint8_t pwm = static_cast<uint8_t>((pct * 255) / 100);
  tft.setBrightness(pwm);

  if (brightness_label) {
    lv_label_set_text_fmt(brightness_label, "Brightness: %ld%%", pct);
  }
}

// -------------------------------------------------------------------------
// Build Main Screen + Finger-Tracked Settings Drawer
// -------------------------------------------------------------------------
void build_ui() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Reveal the bottom pill whenever any child or background on the screen is touched
  lv_obj_add_event_cb(scr, screen_touch_wake_cb, LV_EVENT_PRESSED, nullptr);

  // Main screen center button (enable event bubbling so pressing it also reveals the pill)
  lv_obj_t *btn = lv_button_create(scr);
  lv_obj_set_size(btn, 180, 60);
  lv_obj_center(btn);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "Tap Me");
  lv_obj_center(btn_label);

  // Large invisible bottom-edge touch grab zone to pull the drawer up
  lv_obj_t *bottom_grab_zone = lv_obj_create(scr);
  lv_obj_set_size(bottom_grab_zone, SCREEN_WIDTH, 75);
  lv_obj_align(bottom_grab_zone, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(bottom_grab_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom_grab_zone, 0, 0);
  lv_obj_remove_flag(bottom_grab_zone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(bottom_grab_zone, drawer_drag_event_cb, LV_EVENT_ALL, nullptr);

  // Visible bottom handle pill (starts completely hidden until screen is touched)
  bottom_pill = lv_obj_create(bottom_grab_zone);
  lv_obj_set_size(bottom_pill, 56, 7);
  lv_obj_align(bottom_pill, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(bottom_pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bottom_pill, lv_color_hex(0x64788E), 0);
  lv_obj_set_style_bg_opa(bottom_pill, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom_pill, 0, 0);
  lv_obj_remove_flag(bottom_pill, LV_OBJ_FLAG_CLICKABLE);

  // ---------------------------------------------------------------------
  // Settings Drawer (starts hidden off-screen at y = SCREEN_HEIGHT)
  // ---------------------------------------------------------------------
  settings_drawer = lv_obj_create(scr);
  lv_obj_set_size(settings_drawer, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(settings_drawer, 0, SCREEN_HEIGHT);
  lv_obj_set_style_radius(settings_drawer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(settings_drawer, lv_color_hex(0x1C2836), 0);
  lv_obj_set_style_bg_opa(settings_drawer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(settings_drawer, lv_color_hex(0x3A5068), 0);
  lv_obj_set_style_border_width(settings_drawer, 2, 0);
  lv_obj_set_style_pad_all(settings_drawer, 0, 0);
  lv_obj_remove_flag(settings_drawer, LV_OBJ_FLAG_SCROLLABLE);

  // Generous top tab/handle grab zone (360 x 85 px) for easy finger grab to close
  lv_obj_t *top_grab_zone = lv_obj_create(settings_drawer);
  lv_obj_set_size(top_grab_zone, SCREEN_WIDTH, 85);
  lv_obj_align(top_grab_zone, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(top_grab_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(top_grab_zone, 0, 0);
  lv_obj_remove_flag(top_grab_zone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(top_grab_zone, drawer_drag_event_cb, LV_EVENT_ALL, nullptr);
  // Visible top handle pill inside the top grab zone
  lv_obj_t *top_pill = lv_obj_create(top_grab_zone);
  lv_obj_set_size(top_pill, 60, 8);
  lv_obj_align(top_pill, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_radius(top_pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(top_pill, lv_color_hex(0x8FA5BD), 0);
  lv_obj_set_style_border_width(top_pill, 0, 0);
  lv_obj_remove_flag(top_pill, LV_OBJ_FLAG_CLICKABLE);
  // Drawer Title inside the top grab area
  lv_obj_t *title = lv_label_create(top_grab_zone);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_remove_flag(title, LV_OBJ_FLAG_CLICKABLE);
  // Brightness label + slider
  brightness_label = lv_label_create(settings_drawer);
  lv_label_set_text(brightness_label, "Brightness: 80%");
  lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xD0DCE8), 0);
  lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, -18);
  lv_obj_t *slider = lv_slider_create(settings_drawer);
  lv_obj_set_size(slider, 230, 22);
  lv_obj_align(slider, LV_ALIGN_CENTER, 0, 20);
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
  tft.setBrightness(204);
  lv_init();
  lv_tick_set_cb(millis);
  draw_buf = static_cast<uint8_t *>(
    // heap_caps_aligned_alloc(64, DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    heap_caps_aligned_alloc(64, DRAW_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
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
  delay(1);
}