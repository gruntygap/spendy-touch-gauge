#include "ui_gauges.h"
#include "display_driver.h"
#include <stdlib.h>

static lv_obj_t *tileview = nullptr;
static lv_obj_t *dots_container = nullptr;
static lv_obj_t *dot_1 = nullptr;
static lv_obj_t *dot_2 = nullptr;
static lv_obj_t *dot_3 = nullptr;

// Tile 0: Boost Gauge (-10.0 to 20.0 PSI)
static lv_obj_t *boost_scale = nullptr;
static lv_obj_t *needle_line = nullptr;
static lv_obj_t *psi_val_lbl = nullptr;
static int32_t last_boost = -9999;

// Tile 1: Coolant Temp Gauge (100 to 260 °F)
static lv_obj_t *temp_arc = nullptr;
static lv_obj_t *temp_lbl = nullptr;
static int32_t last_temp = -9999;

// Tile 2: Fan Control Switches & Spinning Bitmaps
static lv_draw_buf_t *fan_icon_snap = nullptr;
static lv_obj_t *fan_img[2] = { nullptr, nullptr };
static lv_obj_t *fan_sw[2] = { nullptr, nullptr };
static bool fan_running[2] = { false, false };
static fan_toggle_cb_t user_fan_cb = nullptr;

void ui_gauges_set_fan_callback(fan_toggle_cb_t cb) {
  user_fan_cb = cb;
}

// Update page indicator dots when user swipes left/right
static void tileview_changed_cb(lv_event_t *e) {
  lv_obj_t *tv = lv_event_get_target_obj(e);
  lv_obj_t *active = lv_tileview_get_tile_active(tv);
  uint32_t idx = lv_obj_get_index(active);

  if (dot_1 && dot_2 && dot_3) {
    lv_obj_set_style_bg_color(dot_1, lv_color_hex(idx == 0 ? 0x38BDF8 : 0x334155), 0);
    lv_obj_set_style_bg_color(dot_2, lv_color_hex(idx == 1 ? 0x38BDF8 : 0x334155), 0);
    lv_obj_set_style_bg_color(dot_3, lv_color_hex(idx == 2 ? 0x38BDF8 : 0x334155), 0);
  }

  extern void ui_drawer_notify_touch();
  ui_drawer_notify_touch();
}

static lv_draw_buf_t *create_fan_bitmap(lv_obj_t *temp_parent) {
  constexpr int32_t SIZE = 45;  // Odd size gives exact single center pixel at (22, 22)

  lv_obj_t *holder = lv_obj_create(temp_parent);
  lv_obj_remove_style_all(holder);
  lv_obj_set_size(holder, SIZE, SIZE);
  lv_obj_set_style_bg_opa(holder, LV_OPA_TRANSP, 0);

  // Subtle circular shroud ring (odd 41x41 centered on 45x45)
  lv_obj_t *shroud = lv_obj_create(holder);
  lv_obj_remove_style_all(shroud);
  lv_obj_set_size(shroud, 41, 41);
  lv_obj_center(shroud);
  lv_obj_set_style_radius(shroud, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(shroud, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(shroud, 2, 0);

  // 4 symmetrical odd-dimensioned blades arranged at 0°, 90°, 180°, 270°
  const int32_t bx[] = { 0, 10, 0, -10 };
  const int32_t by[] = { -10, 0, 10, 0 };
  const int32_t bw[] = { 9, 15, 9, 15 };
  const int32_t bh[] = { 15, 9, 15, 9 };

  for (int i = 0; i < 4; i++) {
    lv_obj_t *blade = lv_obj_create(holder);
    lv_obj_remove_style_all(blade);
    lv_obj_set_size(blade, bw[i], bh[i]);
    lv_obj_align(blade, LV_ALIGN_CENTER, bx[i], by[i]);
    lv_obj_set_style_radius(blade, 4, 0);
    lv_obj_set_style_bg_color(blade, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_bg_opa(blade, LV_OPA_COVER, 0);
  }

  // Center motor hub (odd 13x13 so center pixel is exact)
  lv_obj_t *hub = lv_obj_create(holder);
  lv_obj_remove_style_all(hub);
  lv_obj_set_size(hub, 13, 13);
  lv_obj_center(hub);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(0xE2E8F0), 0);
  lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(0x0F172A), 0);
  lv_obj_set_style_border_width(hub, 2, 0);

  lv_obj_update_layout(holder);
  lv_draw_buf_t *snap = lv_snapshot_take(holder, LV_COLOR_FORMAT_ARGB8888);
  lv_obj_delete(holder);
  return snap;
}



// -------------------------------------------------------------------------
// Fan Spin Animation & Switch Event Handlers
// -------------------------------------------------------------------------

static lv_timer_t *fan_spin_timer = nullptr;
static int32_t fan_angle[2] = { 0, 0 };

// Single lightweight timer rotates whichever fan(s) are currently ON
static void fan_spin_timer_cb(lv_timer_t *timer) {
  bool any_running = false;
  for (uint8_t i = 0; i < 2; i++) {
    if (fan_running[i] && fan_img[i]) {
      any_running = true;
      fan_angle[i] = (fan_angle[i] + 180) % 3600;  // +18° per 30ms (~100 RPM)
      lv_image_set_rotation(fan_img[i], fan_angle[i]);
    }
  }
  if (!any_running && fan_spin_timer) {
    lv_timer_pause(fan_spin_timer);
  }
}

void ui_gauges_set_fan_state(uint8_t idx, bool on) {
  if (idx > 1) return;
  fan_running[idx] = on;

  if (fan_sw[idx]) {
    bool currently_checked = lv_obj_has_state(fan_sw[idx], LV_STATE_CHECKED);
    if (on && !currently_checked) {
      lv_obj_add_state(fan_sw[idx], LV_STATE_CHECKED);
    } else if (!on && currently_checked) {
      lv_obj_remove_state(fan_sw[idx], LV_STATE_CHECKED);
    }
  }

  if (!fan_img[idx]) return;

  if (on) {
    // Full brightness when running
    lv_obj_set_style_opa(fan_img[idx], LV_OPA_COVER, 0);

    if (!fan_spin_timer) {
      fan_spin_timer = lv_timer_create(fan_spin_timer_cb, 30, nullptr);
    } else {
      lv_timer_resume(fan_spin_timer);
    }
  } else {
    // Snap cleanly back to 0° and dim to 40% opacity when stopped
    fan_angle[idx] = 0;
    lv_image_set_rotation(fan_img[idx], 0);
    lv_obj_set_style_opa(fan_img[idx], LV_OPA_40, 0);
  }
}

static void fan_switch_event_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target_obj(e);
  uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

  ui_gauges_set_fan_state(idx, is_on);

  if (user_fan_cb) {
    user_fan_cb(idx, is_on);
  }
}


// -------------------------------------------------------------------------
// Build Tile 2: Primary & Secondary Fan Control Page
// -------------------------------------------------------------------------
static void build_fans_tile(lv_obj_t *parent) {
  lv_obj_add_flag(parent, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (!fan_icon_snap) {
    fan_icon_snap = create_fan_bitmap(parent);
  }

  // Outer circular bezel matching the other gauges
  lv_obj_t *bezel = lv_obj_create(parent);
  lv_obj_set_size(bezel, 352, 352);
  lv_obj_center(bezel);
  lv_obj_set_style_radius(bezel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bezel, lv_color_hex(0x111822), 0);
  lv_obj_set_style_border_color(bezel, lv_color_hex(0x263547), 0);
  lv_obj_set_style_border_width(bezel, 3, 0);
  lv_obj_set_style_pad_all(bezel, 0, 0);
  lv_obj_remove_flag(bezel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bezel, LV_OBJ_FLAG_EVENT_BUBBLE);

  // Page title
  lv_obj_t *header = lv_label_create(bezel);
  lv_label_set_text(header, "COOLING FANS");
  lv_obj_set_style_text_color(header, lv_color_hex(0x64748B), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 42);

  const char *names[2] = { "Primary Fan", "Secondary Fan" };
  const int32_t y_offsets[2] = { -38, 46 };

  for (uint8_t i = 0; i < 2; i++) {
    // Row card sized to fit comfortably inside the 360px circular bezel
    lv_obj_t *card = lv_obj_create(bezel);
    lv_obj_set_size(card, 276, 68);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, y_offsets[i]);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x182230), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2B3B4E), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_hor(card, 14, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Small animated bitmap fan on the left
    fan_img[i] = lv_image_create(card);
    if (fan_icon_snap) {
      lv_image_set_src(fan_img[i], fan_icon_snap);
      lv_image_set_pivot(fan_img[i], fan_icon_snap->header.w / 2, fan_icon_snap->header.h / 2);
    }
    lv_obj_align(fan_img[i], LV_ALIGN_LEFT_MID, 0, 0);

    // Label specifying Primary or Secondary
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, names[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF1F5F9), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 54, 0);

    // Input switch on the right
    fan_sw[i] = lv_switch_create(card);
    lv_obj_set_size(fan_sw[i], 52, 28);
    lv_obj_align(fan_sw[i], LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(fan_sw[i], lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(fan_sw[i], lv_color_hex(0x0EA5E9), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(fan_sw[i], fan_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Initialize stopped/dimmed state
    ui_gauges_set_fan_state(i, false);
  }
}

static void build_boost_tile(lv_obj_t *parent) {
  lv_obj_add_flag(parent, LV_OBJ_FLAG_EVENT_BUBBLE);

  // Pre-bake static dial face into a pre-swapped PSRAM snapshot
  lv_obj_t *static_dial = lv_obj_create(parent);
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

  lv_obj_update_layout(static_dial);
  lv_draw_buf_t *snap = lv_snapshot_take(static_dial, LV_COLOR_FORMAT_RGB565);

  lv_obj_t *dial_parent = static_dial;
  if (snap) {
    lv_draw_sw_rgb565_swap(snap->data, (snap->header.stride / 2) * snap->header.h);
    snap->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    lv_obj_delete(static_dial);

    lv_obj_t *dial_img = lv_image_create(parent);
    lv_image_set_src(dial_img, snap);
    lv_obj_center(dial_img);
    lv_obj_add_flag(dial_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    dial_parent = dial_img;
  }

  boost_scale = lv_scale_create(dial_parent);
  lv_obj_set_size(boost_scale, 326, 326);
  lv_obj_center(boost_scale);
  lv_scale_set_mode(boost_scale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(boost_scale, -100, 200);
  lv_scale_set_total_tick_count(boost_scale, 0);
  lv_scale_set_angle_range(boost_scale, 270);
  lv_scale_set_rotation(boost_scale, 135);
  lv_scale_set_label_show(boost_scale, false);
  lv_obj_set_style_arc_opa(boost_scale, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_add_flag(boost_scale, LV_OBJ_FLAG_EVENT_BUBBLE);

  needle_line = lv_line_create(boost_scale);
  lv_obj_set_style_line_width(needle_line, 4, 0);
  lv_obj_set_style_line_color(needle_line, lv_color_hex(0xFF5A36), 0);

  lv_obj_t *hub = lv_obj_create(boost_scale);
  lv_obj_set_size(hub, 30, 30);
  lv_obj_center(hub);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(0x64748B), 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);

  psi_val_lbl = lv_label_create(dial_parent);
  lv_label_set_text(psi_val_lbl, "0.0");
  lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xF1F5F9), 0);
  lv_obj_align(psi_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -68);

  ui_gauges_set_boost_tenths(0);
}

static void build_coolant_tile(lv_obj_t *parent) {
  lv_obj_add_flag(parent, LV_OBJ_FLAG_EVENT_BUBBLE);

  // 1. Temporary container for static bezel, track arc, and unit label
  lv_obj_t *static_bezel = lv_obj_create(parent);
  lv_obj_set_size(static_bezel, 352, 352);
  lv_obj_center(static_bezel);
  lv_obj_set_style_radius(static_bezel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(static_bezel, lv_color_hex(0x111822), 0);
  lv_obj_set_style_border_color(static_bezel, lv_color_hex(0x263547), 0);
  lv_obj_set_style_border_width(static_bezel, 3, 0);
  lv_obj_set_style_pad_all(static_bezel, 0, 0);
  lv_obj_remove_flag(static_bezel, LV_OBJ_FLAG_SCROLLABLE);

  // Background track arc (baked into snapshot so LVGL never re-draws it)
  lv_obj_t *bg_track = lv_arc_create(static_bezel);
  lv_obj_set_size(bg_track, 306, 306);
  lv_obj_center(bg_track);
  lv_arc_set_rotation(bg_track, 135);
  lv_arc_set_bg_angles(bg_track, 0, 270);
  lv_arc_set_range(bg_track, 1000, 2600);
  lv_arc_set_value(bg_track, 1000);
  lv_obj_remove_style(bg_track, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(bg_track, 18, LV_PART_MAIN);
  lv_obj_set_style_arc_color(bg_track, lv_color_hex(0x1E293B), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(bg_track, LV_OPA_TRANSP, LV_PART_INDICATOR);

  lv_obj_t *unit_lbl = lv_label_create(static_bezel);
  lv_label_set_text(unit_lbl, "COOLANT TEMP");
  lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x64748B), 0);
  lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, 24);

  // 2. Snapshot static_bezel into PSRAM and pre-swap bytes for zero-copy blitting
  lv_obj_update_layout(static_bezel);
  lv_draw_buf_t *snap = lv_snapshot_take(static_bezel, LV_COLOR_FORMAT_RGB565);

  lv_obj_t *dial_parent = static_bezel;
  if (snap) {
    lv_draw_sw_rgb565_swap(snap->data, (snap->header.stride / 2) * snap->header.h);
    snap->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    lv_obj_delete(static_bezel);

    lv_obj_t *dial_img = lv_image_create(parent);
    lv_image_set_src(dial_img, snap);
    lv_obj_center(dial_img);
    lv_obj_add_flag(dial_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    dial_parent = dial_img;
  }

  // 3. Dynamic indicator arc only (LV_PART_MAIN is transparent since track is baked)
  temp_arc = lv_arc_create(dial_parent);
  lv_obj_set_size(temp_arc, 306, 306);
  lv_obj_center(temp_arc);
  lv_arc_set_rotation(temp_arc, 135);
  lv_arc_set_bg_angles(temp_arc, 0, 270);
  lv_arc_set_range(temp_arc, 1000, 2600);
  lv_arc_set_value(temp_arc, 1850);
  lv_obj_remove_style(temp_arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(temp_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(temp_arc, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_set_style_arc_opa(temp_arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(temp_arc, 18, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(temp_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR);

  // Dynamic digital temperature readout
  temp_lbl = lv_label_create(dial_parent);
  lv_label_set_text(temp_lbl, "185 °F");
  lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xF1F5F9), 0);
  lv_obj_align(temp_lbl, LV_ALIGN_CENTER, 0, -10);

  ui_gauges_set_coolant_tenths(1850);
}

void ui_gauges_create(lv_obj_t *parent) {
  tileview = lv_tileview_create(parent);
  lv_obj_set_size(tileview, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(tileview, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Tile 0 (Boost): swipe right -> Tile 1
  lv_obj_t *tile0 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
  build_boost_tile(tile0);

  // Tile 1 (Coolant): swipe left -> Tile 0 OR swipe right -> Tile 2
  lv_obj_t *tile1 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
  build_coolant_tile(tile1);

  // Tile 2 (Cooling Fans): swipe left -> Tile 1
  lv_obj_t *tile2 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT);
  build_fans_tile(tile2);


  // Subtle top page indicator dots (inside circular safe zone)
  dots_container = lv_obj_create(parent);
  lv_obj_set_size(dots_container, 54, 16);
  lv_obj_align(dots_container, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_set_style_bg_opa(dots_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dots_container, 0, 0);
  lv_obj_set_style_pad_all(dots_container, 0, 0);
  lv_obj_set_style_opa(dots_container, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(dots_container, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(dots_container, LV_OBJ_FLAG_SCROLLABLE);

  dot_1 = lv_obj_create(dots_container);
  lv_obj_set_size(dot_1, 7, 7);
  lv_obj_align(dot_1, LV_ALIGN_CENTER, -14, 0);
  lv_obj_set_style_radius(dot_1, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot_1, lv_color_hex(0x38BDF8), 0);
  lv_obj_set_style_border_width(dot_1, 0, 0);
  lv_obj_remove_flag(dot_1, LV_OBJ_FLAG_CLICKABLE);

  dot_2 = lv_obj_create(dots_container);
  lv_obj_set_size(dot_2, 7, 7);
  lv_obj_align(dot_2, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(dot_2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot_2, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(dot_2, 0, 0);
  lv_obj_remove_flag(dot_2, LV_OBJ_FLAG_CLICKABLE);

  dot_3 = lv_obj_create(dots_container);
  lv_obj_set_size(dot_3, 7, 7);
  lv_obj_align(dot_3, LV_ALIGN_CENTER, 14, 0);
  lv_obj_set_style_radius(dot_3, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot_3, lv_color_hex(0x334155), 0);
  lv_obj_set_style_border_width(dot_3, 0, 0);
  lv_obj_remove_flag(dot_3, LV_OBJ_FLAG_CLICKABLE);
}

void ui_gauges_set_boost_tenths(int32_t psi_tenths) {
  if (psi_tenths == last_boost) return;
  last_boost = psi_tenths;
  if (boost_scale && needle_line) {
    lv_scale_set_line_needle_value(boost_scale, needle_line, 102, psi_tenths);
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
void ui_gauges_set_coolant_tenths(int32_t temp_tenths) {
  if (temp_tenths == last_temp) return;
  last_temp = temp_tenths;

  if (temp_arc) {
    lv_arc_set_value(temp_arc, temp_tenths);
    if (temp_tenths >= 2250) {
      lv_obj_set_style_arc_color(temp_arc, lv_color_hex(0xEF4444), LV_PART_INDICATOR);
    } else if (temp_tenths >= 2050) {
      lv_obj_set_style_arc_color(temp_arc, lv_color_hex(0xF59E0B), LV_PART_INDICATOR);
    } else {
      lv_obj_set_style_arc_color(temp_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR);
    }
  }

  if (temp_lbl) {
    lv_label_set_text_fmt(temp_lbl, "%ld °F", temp_tenths / 10);
  }
}

static void dots_opa_anim_cb(void *var, int32_t v) {
  lv_obj_set_style_opa(static_cast<lv_obj_t *>(var), static_cast<lv_opa_t>(v), 0);
}

void ui_gauges_set_dots_visible(bool show) {
  if (!dots_container) return;

  lv_opa_t target_opa = show ? LV_OPA_COVER : LV_OPA_TRANSP;
  lv_opa_t current_opa = lv_obj_get_style_opa(dots_container, LV_PART_MAIN);
  if (current_opa == target_opa) return;

  lv_anim_delete(dots_container, dots_opa_anim_cb);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, dots_container);
  lv_anim_set_exec_cb(&a, dots_opa_anim_cb);
  lv_anim_set_values(&a, current_opa, target_opa);
  lv_anim_set_duration(&a, show ? 150 : 300);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}
