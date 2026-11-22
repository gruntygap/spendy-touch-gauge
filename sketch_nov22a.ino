#include <Arduino.h>
#include <atomic>
#include <lvgl.h>
#include <ST77916_QSPI.h>
#include <CST816S.h>

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// Thread-safe bridge between Core 0 (CAN/sensors) and Core 1 (LVGL UI)
static std::atomic<int32_t> g_boost_psi_tenths{ 0 };
static std::atomic<bool> g_demo_mode{ true };  // If no can-bus demo

// -------------------------------------------------------------------------
// Core 0 Task: High-Speed Sensor / CAN Bus Sampling
// -------------------------------------------------------------------------
void data_sampling_task(void *pvParameters) {
  // Initialize TWAI (CAN bus) or ADC peripherals here on Core 0
  TickType_t last_wake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(5);  // 200 Hz sampling rate

  float phase = 0.0f;

  for (;;) {
    if (g_demo_mode.load(std::memory_order_relaxed)) {
      // Simulated sweep (-8.5 to +18.5 PSI) until real CAN/sensor is wired
      phase += 0.015f;
      int32_t sim_psi = static_cast<int32_t>(50.0f + 135.0f * sinf(phase));
      g_boost_psi_tenths.store(sim_psi, std::memory_order_relaxed);
    } else {
      // Example placeholder for TWAI / CAN frame read:
      // twai_message_t msg;
      // if (twai_receive(&msg, 0) == ESP_OK && msg.identifier == 0x123) {
      //     int32_t psi_tenths = parse_boost_from_can(msg.data);
      //     g_boost_psi_tenths.store(psi_tenths, std::memory_order_relaxed);
      // }
    }

    // Exact 5 ms periodic sleep—never starves Core 0 system watchdog
    vTaskDelayUntil(&last_wake, period);
  }
}

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

// 1/6 screen scratchpad in fast internal DMA SRAM (360 * 60 * 2 = 43.2 KB)
static constexpr uint32_t DRAW_BUF_SIZE = SCREEN_WIDTH * (SCREEN_HEIGHT / 4) * sizeof(lv_color16_t);
static uint8_t *draw_buf = nullptr;

// Gauge UI elements
static lv_obj_t *dial_canvas = nullptr;
static uint8_t *canvas_buf = nullptr;
static lv_obj_t *boost_scale = nullptr;
static lv_obj_t *needle_line = nullptr;
static lv_obj_t *psi_val_lbl = nullptr;
static int32_t last_tenths = -9999;

// Drawer UI elements
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

  // Zero-copy memcpy into ST77916_QSPI's big-endian framebuffer
  tft.pushImage(area->x1, area->y1, w, h,
                reinterpret_cast<const uint16_t *>(px_map),
                ST77916_PIXELS_BE);

  if (lv_display_flush_is_last(disp)) {
    // Push ONLY the changed dirty box instead of the whole 360x360 screen
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
// Boost Gauge Value Setter & Demo Sweep Animation (0.1 PSI units)
// -------------------------------------------------------------------------
void set_boost_tenths(int32_t psi_tenths) {
  // Skip redundant redraws when value hasn't changed
  if (psi_tenths == last_tenths) return;
  last_tenths = psi_tenths;

  if (boost_scale && needle_line) {
    lv_scale_set_line_needle_value(boost_scale, needle_line, 116, psi_tenths);
  }

  if (psi_val_lbl) {
    int32_t whole = psi_tenths / 10;
    int32_t frac = abs(psi_tenths % 10);
    if (psi_tenths < 0 && whole == 0) {
      lv_label_set_text_fmt(psi_val_lbl, "-0.%ld", frac);
    } else {
      lv_label_set_text_fmt(psi_val_lbl, "%ld.%ld", whole, frac);
    }

    if (psi_tenths >= 140) {
      lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xFF5A36), 0);
    } else if (psi_tenths > 0) {
      lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0x38BDF8), 0);
    } else {
      lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xF1F5F9), 0);
    }
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

  lv_opa_t target_opa = show ? LV_OPA_COVER : LV_OPA_TRANSP;
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
// Smooth Snap Animation & Drag Tracking for Settings Drawer
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
// Pre-Bake Static Dial Face Into a PSRAM Snapshot Image for 60 FPS Needle
// -------------------------------------------------------------------------
void build_ui() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B1017), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, screen_touch_wake_cb, LV_EVENT_PRESSED, nullptr);

  // 1. Temporary container for static bezel + scale ticks + labels
  lv_obj_t *static_dial = lv_obj_create(scr);
  lv_obj_set_size(static_dial, 352, 352);
  lv_obj_center(static_dial);
  lv_obj_set_style_radius(static_dial, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(static_dial, lv_color_hex(0x111822), 0);
  lv_obj_set_style_border_color(static_dial, lv_color_hex(0x263547), 0);
  lv_obj_set_style_border_width(static_dial, 3, 0);
  lv_obj_set_style_pad_all(static_dial, 0, 0);
  lv_obj_remove_flag(static_dial, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *temp_scale = lv_scale_create(static_dial);
  lv_obj_set_size(temp_scale, 326, 326);
  lv_obj_center(temp_scale);
  lv_scale_set_mode(temp_scale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(temp_scale, -100, 200);
  lv_scale_set_total_tick_count(temp_scale, 31);
  lv_scale_set_major_tick_every(temp_scale, 5);
  lv_scale_set_angle_range(temp_scale, 270);
  lv_scale_set_rotation(temp_scale, 135);
  lv_scale_set_label_show(temp_scale, true);
  static const char *custom_labels[] = { "-10", "-5", "0", "5", "10", "15", "20", nullptr };
  lv_scale_set_text_src(temp_scale, custom_labels);
  lv_obj_set_style_arc_width(temp_scale, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(temp_scale, lv_color_hex(0x334155), LV_PART_MAIN);
  lv_obj_set_style_line_width(temp_scale, 3, LV_PART_INDICATOR);
  lv_obj_set_style_length(temp_scale, 16, LV_PART_INDICATOR);
  lv_obj_set_style_line_color(temp_scale, lv_color_hex(0x94A3B8), LV_PART_INDICATOR);
  lv_obj_set_style_text_color(temp_scale, lv_color_hex(0xCBD5E1), LV_PART_INDICATOR);
  lv_obj_set_style_line_width(temp_scale, 2, LV_PART_ITEMS);
  lv_obj_set_style_length(temp_scale, 8, LV_PART_ITEMS);
  lv_obj_set_style_line_color(temp_scale, lv_color_hex(0x475569), LV_PART_ITEMS);
  static lv_style_t boost_arc_style, boost_major_style, boost_minor_style;
  lv_style_init(&boost_arc_style);
  lv_style_set_arc_color(&boost_arc_style, lv_color_hex(0x0EA5E9));
  lv_style_set_arc_width(&boost_arc_style, 5);
  lv_style_init(&boost_major_style);
  lv_style_set_line_color(&boost_major_style, lv_color_hex(0x38BDF8));
  lv_style_set_text_color(&boost_major_style, lv_color_hex(0x38BDF8));
  lv_style_init(&boost_minor_style);
  lv_style_set_line_color(&boost_minor_style, lv_color_hex(0x0284C7));
  lv_scale_section_t *boost_sec = lv_scale_add_section(temp_scale);
  lv_scale_section_set_range(boost_sec, 0, 140);
  lv_scale_section_set_style(boost_sec, LV_PART_MAIN, &boost_arc_style);
  lv_scale_section_set_style(boost_sec, LV_PART_INDICATOR, &boost_major_style);
  lv_scale_section_set_style(boost_sec, LV_PART_ITEMS, &boost_minor_style);
  static lv_style_t warn_arc_style, warn_major_style, warn_minor_style;
  lv_style_init(&warn_arc_style);
  lv_style_set_arc_color(&warn_arc_style, lv_color_hex(0xEF4444));
  lv_style_set_arc_width(&warn_arc_style, 6);
  lv_style_init(&warn_major_style);
  lv_style_set_line_color(&warn_major_style, lv_color_hex(0xF87171));
  lv_style_set_text_color(&warn_major_style, lv_color_hex(0xF87171));
  lv_style_init(&warn_minor_style);
  lv_style_set_line_color(&warn_minor_style, lv_color_hex(0xDC2626));
  lv_scale_section_t *warn_sec = lv_scale_add_section(temp_scale);
  lv_scale_section_set_range(warn_sec, 140, 200);
  lv_scale_section_set_style(warn_sec, LV_PART_MAIN, &warn_arc_style);
  lv_scale_section_set_style(warn_sec, LV_PART_INDICATOR, &warn_major_style);
  lv_scale_section_set_style(warn_sec, LV_PART_ITEMS, &warn_minor_style);
  lv_obj_t *unit_lbl = lv_label_create(static_dial);
  lv_label_set_text(unit_lbl, "BOOST PSI");
  lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x64748B), 0);
  lv_obj_align(unit_lbl, LV_ALIGN_BOTTOM_MID, 0, -46);
  // 2. Snapshot static_dial once into PSRAM (352x352 RGB565 = 247 KB)
  lv_obj_update_layout(static_dial);
  lv_draw_buf_t *snap = lv_snapshot_take(static_dial, LV_COLOR_FORMAT_RGB565);
  if (snap) {
    lv_draw_sw_rgb565_swap(snap->data, (snap->header.stride / 2) * snap->header.h);
    snap->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  }
  lv_obj_delete(static_dial);  // Free all 31 tick/label objects!
  // Display the pre-baked snapshot as a fast zero-overhead bitmap
  lv_obj_t *dial_img = lv_image_create(scr);
  lv_image_set_src(dial_img, snap);
  lv_obj_center(dial_img);
  lv_obj_add_flag(dial_img, LV_OBJ_FLAG_EVENT_BUBBLE);
  // 3. Invisible lightweight scale solely to compute needle coordinates
  boost_scale = lv_scale_create(dial_img);
  lv_obj_set_size(boost_scale, 326, 326);
  lv_obj_center(boost_scale);
  lv_scale_set_mode(boost_scale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(boost_scale, -100, 200);
  lv_scale_set_total_tick_count(boost_scale, 0);  // 0 ticks drawn = instant!
  lv_scale_set_angle_range(boost_scale, 270);
  lv_scale_set_rotation(boost_scale, 135);
  lv_scale_set_label_show(boost_scale, false);
  lv_obj_set_style_arc_opa(boost_scale, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_add_flag(boost_scale, LV_OBJ_FLAG_EVENT_BUBBLE);
  // Needle Line (4px crisp line—no rounded endcaps cuts AA overhead in half)
  needle_line = lv_line_create(boost_scale);
  lv_obj_set_style_line_width(needle_line, 4, 0);
  lv_obj_set_style_line_color(needle_line, lv_color_hex(0xFF5A36), 0);
  // Center hub cap
  lv_obj_t *hub = lv_obj_create(boost_scale);
  lv_obj_set_size(hub, 30, 30);
  lv_obj_center(hub);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(0x64748B), 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);
  // Digital readout
  psi_val_lbl = lv_label_create(dial_img);
  lv_label_set_text(psi_val_lbl, "0.0");
  lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xF1F5F9), 0);
  lv_obj_align(psi_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -68);
  // ---------------------------------------------------------------------
  // Bottom Touch-Wake Grab Zone & Auto-Hiding Pill
  // ---------------------------------------------------------------------
  lv_obj_t *bottom_grab_zone = lv_obj_create(scr);
  lv_obj_set_size(bottom_grab_zone, SCREEN_WIDTH, 75);
  lv_obj_align(bottom_grab_zone, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(bottom_grab_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom_grab_zone, 0, 0);
  lv_obj_remove_flag(bottom_grab_zone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(bottom_grab_zone, drawer_drag_event_cb, LV_EVENT_ALL, nullptr);
  bottom_pill = lv_obj_create(bottom_grab_zone);
  lv_obj_set_size(bottom_pill, 56, 7);
  lv_obj_align(bottom_pill, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(bottom_pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bottom_pill, lv_color_hex(0x64788E), 0);
  lv_obj_set_style_bg_opa(bottom_pill, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bottom_pill, 0, 0);
  lv_obj_remove_flag(bottom_pill, LV_OBJ_FLAG_CLICKABLE);
  // ---------------------------------------------------------------------
  // Finger-Tracked Settings Drawer
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
  lv_obj_t *top_grab_zone = lv_obj_create(settings_drawer);
  lv_obj_set_size(top_grab_zone, SCREEN_WIDTH, 85);
  lv_obj_align(top_grab_zone, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(top_grab_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(top_grab_zone, 0, 0);
  lv_obj_remove_flag(top_grab_zone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(top_grab_zone, drawer_drag_event_cb, LV_EVENT_ALL, nullptr);
  lv_obj_t *top_pill = lv_obj_create(top_grab_zone);
  lv_obj_set_size(top_pill, 60, 8);
  lv_obj_align(top_pill, LV_ALIGN_TOP_MID, 0, 16);
  lv_obj_set_style_radius(top_pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(top_pill, lv_color_hex(0x8FA5BD), 0);
  lv_obj_set_style_border_width(top_pill, 0, 0);
  lv_obj_remove_flag(top_pill, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *title = lv_label_create(top_grab_zone);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_remove_flag(title, LV_OBJ_FLAG_CLICKABLE);
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
  set_boost_tenths(0);
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
    heap_caps_aligned_alloc(64, DRAW_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, draw_buf, nullptr, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  build_ui();
  lv_refr_now(disp);

  // Pin data/CAN sampling task strictly to Core 0
  xTaskCreatePinnedToCore(
    data_sampling_task,  // Task function
    "DataSampler",       // Name
    4096,                // 4 KB stack (plenty for CAN/ADC math)
    nullptr,             // Parameters
    2,                   // Priority (above idle, safe for Core 0)
    nullptr,             // Task handle
    0                    // Core 0 (PRO_CPU)
  );
}
void loop() {
// Core 1 safely reads latest atomic telemetry and updates LVGL widgets
  int32_t latest_psi = g_boost_psi_tenths.load(std::memory_order_relaxed);
  set_boost_tenths(latest_psi);
  lv_timer_handler();
  delay(1);
}
