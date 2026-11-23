#include "ui_gauges.h"
#include "ui_bitmaps.h"
#include "display_driver.h"
#include <stdlib.h>
#include <math.h>

#if LV_FONT_MONTSERRAT_48
LV_FONT_DECLARE(lv_font_montserrat_48);
#endif
#if LV_FONT_MONTSERRAT_18
LV_FONT_DECLARE(lv_font_montserrat_18);
#endif

static lv_obj_t *tileview = nullptr;
static lv_obj_t *dots_container = nullptr;
static lv_obj_t *dot_1 = nullptr;
static lv_obj_t *dot_2 = nullptr;
static lv_obj_t *dot_3 = nullptr;
static lv_obj_t *dot_4 = nullptr;
static lv_obj_t *dot_5 = nullptr;

// Tile 0: Boost Gauge (-10.0 to 20.0 PSI)
static lv_obj_t *boost_arc = nullptr;
static lv_obj_t *boost_peak_chip = nullptr;
static lv_obj_t *boost_title_lbl = nullptr;
static lv_obj_t *psi_val_lbl = nullptr;
static lv_obj_t *boost_unit_lbl = nullptr;
static int32_t last_boost = -9999;
static int32_t max_recorded_boost = -100;
static lv_point_precise_t boost_peak_pts[2];

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

static lv_obj_t *fan_temp_bar = nullptr;
static lv_obj_t *fan_temp_lbl = nullptr;

// Tile 3: Table Switching (0 = Fuel VE Table, 1 = Ignition Table)
static lv_obj_t *table_sw[2] = { nullptr, nullptr };
static lv_obj_t *table_badge_1[2] = { nullptr, nullptr };
static lv_obj_t *table_badge_2[2] = { nullptr, nullptr };
static table_toggle_cb_t user_table_cb = nullptr;

// -------------------------------------------------------------------------
// Tile 4 Statics: Voltage (Lightning Bolt) + Ethanol (Corn Cob)
// -------------------------------------------------------------------------
static lv_draw_buf_t *volt_icon_snap = nullptr;
static lv_draw_buf_t *ethanol_icon_snap = nullptr;
static lv_obj_t *volt_val_lbl = nullptr;
static lv_obj_t *ethanol_val_lbl = nullptr;
static int32_t last_volts = -9999;
static int32_t last_ethanol = -9999;

void ui_gauges_set_table_callback(table_toggle_cb_t cb) {
  user_table_cb = cb;
}

// -------------------------------------------------------------------------
// Update tileview_changed_cb for 5 pages
// -------------------------------------------------------------------------
static void tileview_changed_cb(lv_event_t *e) {
  lv_obj_t *tv = lv_event_get_target_obj(e);
  lv_obj_t *active = lv_tileview_get_tile_active(tv);
  uint32_t idx = lv_obj_get_index(active);

  lv_obj_t *dots[] = { dot_1, dot_2, dot_3, dot_4, dot_5 };
  for (uint32_t i = 0; i < 5; i++) {
    if (dots[i]) {
      lv_obj_set_style_bg_color(dots[i], lv_color_hex(idx == i ? 0x38BDF8 : 0x334155), 0);
    }
  }

  extern void ui_drawer_notify_touch();
  ui_drawer_notify_touch();
}

// -------------------------------------------------------------------------
// Build Tile 4: Battery Voltage Display
// -------------------------------------------------------------------------
static void build_voltage_tile(lv_obj_t *parent) {
  lv_obj_add_flag(parent, LV_OBJ_FLAG_EVENT_BUBBLE);

  if (!volt_icon_snap) {
    volt_icon_snap = create_lightning_bitmap(parent);
  }
  if (!ethanol_icon_snap) {
    ethanol_icon_snap = create_corn_bitmap(parent);
  }

  // Outer circular bezel matching the rest of the gauges
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

  // Header label
  lv_obj_t *header = lv_label_create(bezel);
  lv_label_set_text(header, "POWER & FUEL");
  lv_obj_set_style_text_color(header, lv_color_hex(0x64748B), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 38);

  // ---------------------------------------------------------------------
  // Top Row Card: Yellow Lightning Bolt + Live Voltage Readout
  // ---------------------------------------------------------------------
  lv_obj_t *volt_card = lv_obj_create(bezel);
  lv_obj_set_size(volt_card, 272, 68);
  lv_obj_align(volt_card, LV_ALIGN_CENTER, 0, -40);
  lv_obj_set_style_radius(volt_card, 16, 0);
  lv_obj_set_style_bg_color(volt_card, lv_color_hex(0x182230), 0);
  lv_obj_set_style_border_color(volt_card, lv_color_hex(0x2B3B4E), 0);
  lv_obj_set_style_border_width(volt_card, 1, 0);
  lv_obj_set_style_pad_hor(volt_card, 16, 0);
  lv_obj_remove_flag(volt_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(volt_card, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *bolt_img = lv_image_create(volt_card);
  if (volt_icon_snap) {
    lv_image_set_src(bolt_img, volt_icon_snap);
  }
  lv_obj_align(bolt_img, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *volt_title = lv_label_create(volt_card);
  lv_label_set_text(volt_title, "VOLTAGE");
  lv_obj_set_style_text_color(volt_title, lv_color_hex(0x64748B), 0);
  lv_obj_align(volt_title, LV_ALIGN_TOP_RIGHT, 0, 8);

  volt_val_lbl = lv_label_create(volt_card);
  lv_label_set_text(volt_val_lbl, "14.2 V");
  lv_obj_set_style_text_color(volt_val_lbl, lv_color_hex(0xFACC15), 0);
  lv_obj_align(volt_val_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, -8);

  // ---------------------------------------------------------------------
  // Bottom Row Card: Corn Bitmap + Live Ethanol Readout
  // ---------------------------------------------------------------------
  lv_obj_t *eth_card = lv_obj_create(bezel);
  lv_obj_set_size(eth_card, 272, 68);
  lv_obj_align(eth_card, LV_ALIGN_CENTER, 0, 44);
  lv_obj_set_style_radius(eth_card, 16, 0);
  lv_obj_set_style_bg_color(eth_card, lv_color_hex(0x182230), 0);
  lv_obj_set_style_border_color(eth_card, lv_color_hex(0x2B3B4E), 0);
  lv_obj_set_style_border_width(eth_card, 1, 0);
  lv_obj_set_style_pad_hor(eth_card, 16, 0);
  lv_obj_remove_flag(eth_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(eth_card, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *corn_img = lv_image_create(eth_card);
  if (ethanol_icon_snap) {
    lv_image_set_src(corn_img, ethanol_icon_snap);
  }
  lv_obj_align(corn_img, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *eth_title = lv_label_create(eth_card);
  lv_label_set_text(eth_title, "ETHANOL");
  lv_obj_set_style_text_color(eth_title, lv_color_hex(0x64748B), 0);
  lv_obj_align(eth_title, LV_ALIGN_TOP_RIGHT, 0, 8);

  ethanol_val_lbl = lv_label_create(eth_card);
  lv_label_set_text(ethanol_val_lbl, "E85 (85%)");
  lv_obj_set_style_text_color(ethanol_val_lbl, lv_color_hex(0x4ADE80), 0);
  lv_obj_align(ethanol_val_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, -8);
  // Initialize default values
  ui_gauges_set_voltage_tenths(142);
  ui_gauges_set_ethanol_pct(85);
}

// -------------------------------------------------------------------------
// Voltage Data Updater (Called from loop on Core 1)
// -------------------------------------------------------------------------
void ui_gauges_set_voltage_tenths(int32_t volts_tenths) {
  if (volts_tenths == last_volts) return;
  last_volts = volts_tenths;

  int32_t whole = volts_tenths / 10;
  int32_t frac = abs(volts_tenths % 10);

  lv_color_t v_color = lv_color_hex(0xFACC15);  // Healthy yellow
  if (volts_tenths < 122) {
    v_color = lv_color_hex(0xEF4444);  // Low voltage red
  } else if (volts_tenths > 152) {
    v_color = lv_color_hex(0xF97316);  // Over-voltage orange
  } else if (volts_tenths < 132) {
    v_color = lv_color_hex(0x38BDF8);  // Key-on / idle blue
  }

  if (volt_val_lbl) {
    lv_label_set_text_fmt(volt_val_lbl, "%ld.%ld V", whole, frac);
    lv_obj_set_style_text_color(volt_val_lbl, v_color, 0);
  }
}

void ui_gauges_set_ethanol_pct(int32_t ethanol_pct) {
  ethanol_pct = LV_CLAMP(0, ethanol_pct, 100);
  if (ethanol_pct == last_ethanol) return;
  last_ethanol = ethanol_pct;

  if (ethanol_val_lbl) {
    lv_label_set_text_fmt(ethanol_val_lbl, "E%ld (%ld%%)", ethanol_pct, ethanol_pct);

    // High ethanol (E60+) glows bright green/gold; low blend stays clean silver/blue
    if (ethanol_pct >= 60) {
      lv_obj_set_style_text_color(ethanol_val_lbl, lv_color_hex(0x4ADE80), 0);
    } else if (ethanol_pct >= 25) {
      lv_obj_set_style_text_color(ethanol_val_lbl, lv_color_hex(0xFACC15), 0);
    } else {
      lv_obj_set_style_text_color(ethanol_val_lbl, lv_color_hex(0xCBD5E1), 0);
    }
  }
}

// -------------------------------------------------------------------------
// Table State & Highlight Logic (OFF = Table 1 active, ON = Table 2 active)
// -------------------------------------------------------------------------
void ui_gauges_set_table_state(uint8_t idx, bool table2_active) {
  if (idx > 1) return;

  if (table_sw[idx]) {
    bool currently_checked = lv_obj_has_state(table_sw[idx], LV_STATE_CHECKED);
    if (table2_active && !currently_checked) {
      lv_obj_add_state(table_sw[idx], LV_STATE_CHECKED);
    } else if (!table2_active && currently_checked) {
      lv_obj_remove_state(table_sw[idx], LV_STATE_CHECKED);
    }
  }

  // Highlight "1" when switch is OFF, highlight "2" when switch is ON
  if (table_badge_1[idx] && table_badge_2[idx]) {
    if (!table2_active) {
      // Table 1 Active (cyan glow), Table 2 Dimmed
      lv_obj_set_style_bg_color(table_badge_1[idx], lv_color_hex(0x0EA5E9), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(table_badge_1[idx], 0), lv_color_hex(0xFFFFFF), 0);

      lv_obj_set_style_bg_color(table_badge_2[idx], lv_color_hex(0x1E293B), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(table_badge_2[idx], 0), lv_color_hex(0x475569), 0);
    } else {
      // Table 1 Dimmed, Table 2 Active (amber/orange performance highlight)
      lv_obj_set_style_bg_color(table_badge_1[idx], lv_color_hex(0x1E293B), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(table_badge_1[idx], 0), lv_color_hex(0x475569), 0);

      lv_obj_set_style_bg_color(table_badge_2[idx], lv_color_hex(0xF59E0B), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(table_badge_2[idx], 0), lv_color_hex(0x0F172A), 0);
    }
  }
}

static void table_switch_event_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target_obj(e);
  uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  bool table2_active = lv_obj_has_state(sw, LV_STATE_CHECKED);

  ui_gauges_set_table_state(idx, table2_active);

  if (user_table_cb) {
    user_table_cb(idx, table2_active);
  }
}

// -------------------------------------------------------------------------
// Build Tile 3: Table Switching (Fuel VE Table & Ignition Table)
// -------------------------------------------------------------------------
static void build_tables_tile(lv_obj_t *parent) {
  lv_obj_add_flag(parent, LV_OBJ_FLAG_EVENT_BUBBLE);

  // Outer circular bezel matching the rest of the gauges
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

  // Title at the top
  lv_obj_t *header = lv_label_create(bezel);
  lv_label_set_text(header, "Table switching");
  lv_obj_set_style_text_color(header, lv_color_hex(0xCBD5E1), 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 38);

  const char *names[] = { "Fuel VE Table", "Ignition Table" };
  const int32_t y_offsets[] = { -38, 46 };

  for (uint8_t i = 0; i < 2; i++) {
    // Card container inside the 360px circular safe zone
    lv_obj_t *card = lv_obj_create(bezel);
    lv_obj_set_size(card, 278, 68);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, y_offsets[i]);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x182230), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2B3B4E), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_hor(card, 14, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Table name label on the left
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, names[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF1F5F9), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 8);

    // "1" Indicator Badge (highlighted when toggle is OFF)
    table_badge_1[i] = lv_obj_create(card);
    lv_obj_set_size(table_badge_1[i], 26, 22);
    lv_obj_align(table_badge_1[i], LV_ALIGN_BOTTOM_LEFT, 0, -6);
    lv_obj_set_style_radius(table_badge_1[i], 6, 0);
    lv_obj_set_style_border_width(table_badge_1[i], 0, 0);
    lv_obj_set_style_pad_all(table_badge_1[i], 0, 0);
    lv_obj_remove_flag(table_badge_1[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(table_badge_1[i], LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *b1_lbl = lv_label_create(table_badge_1[i]);
    lv_label_set_text(b1_lbl, "1");
    lv_obj_center(b1_lbl);

    // "2" Indicator Badge (highlighted when toggle is ON)
    table_badge_2[i] = lv_obj_create(card);
    lv_obj_set_size(table_badge_2[i], 26, 22);
    lv_obj_align(table_badge_2[i], LV_ALIGN_BOTTOM_LEFT, 32, -6);
    lv_obj_set_style_radius(table_badge_2[i], 6, 0);
    lv_obj_set_style_border_width(table_badge_2[i], 0, 0);
    lv_obj_set_style_pad_all(table_badge_2[i], 0, 0);
    lv_obj_remove_flag(table_badge_2[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(table_badge_2[i], LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *b2_lbl = lv_label_create(table_badge_2[i]);
    lv_label_set_text(b2_lbl, "2");
    lv_obj_center(b2_lbl);

    // Toggle switch on the right
    table_sw[i] = lv_switch_create(card);
    lv_obj_set_size(table_sw[i], 52, 28);
    lv_obj_align(table_sw[i], LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(table_sw[i], lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(table_sw[i], lv_color_hex(0xF59E0B), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(table_sw[i], table_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Default to OFF -> Table 1 highlighted
    ui_gauges_set_table_state(i, false);
  }
}

void ui_gauges_set_fan_callback(fan_toggle_cb_t cb) {
  user_fan_cb = cb;
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

  // Outer circular bezel
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
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 36);

  const char *names[] = { "Primary Fan", "Secondary Fan" };
  const int32_t y_offsets[] = { -52, 16 };

  for (uint8_t i = 0; i < 2; i++) {
    lv_obj_t *card = lv_obj_create(bezel);
    lv_obj_set_size(card, 272, 58);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, y_offsets[i]);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x182230), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2B3B4E), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Spinning fan bitmap on the left
    fan_img[i] = lv_image_create(card);
    if (fan_icon_snap) {
      lv_image_set_src(fan_img[i], fan_icon_snap);
      lv_image_set_pivot(fan_img[i],
                         fan_icon_snap->header.w / 2,
                         fan_icon_snap->header.h / 2);
    }
    lv_obj_align(fan_img[i], LV_ALIGN_LEFT_MID, 0, 0);

    // Primary / Secondary label
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, names[i]);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF1F5F9), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 52, 0);

    // Toggle switch on the right
    fan_sw[i] = lv_switch_create(card);
    lv_obj_set_size(fan_sw[i], 50, 26);
    lv_obj_align(fan_sw[i], LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(fan_sw[i], lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(fan_sw[i], lv_color_hex(0x0EA5E9), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(fan_sw[i], fan_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    ui_gauges_set_fan_state(i, false);
  }

  // ---------------------------------------------------------------------
  // Compact Horizontal Coolant Temperature Bar Beneath the Switches
  // ---------------------------------------------------------------------
  fan_temp_lbl = lv_label_create(bezel);
  lv_label_set_text(fan_temp_lbl, "Coolant: 185 °F");
  lv_obj_set_style_text_color(fan_temp_lbl, lv_color_hex(0xCBD5E1), 0);
  lv_obj_align(fan_temp_lbl, LV_ALIGN_CENTER, 0, 68);

  fan_temp_bar = lv_bar_create(bezel);
  lv_obj_set_size(fan_temp_bar, 196, 12);
  lv_obj_align(fan_temp_bar, LV_ALIGN_CENTER, 0, 90);
  lv_bar_set_range(fan_temp_bar, 1000, 2600);  // Matches 100.0°F - 260.0°F range
  lv_bar_set_value(fan_temp_bar, 1850, LV_ANIM_OFF);
  lv_obj_remove_flag(fan_temp_bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(fan_temp_bar, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_set_style_radius(fan_temp_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(fan_temp_bar, lv_color_hex(0x1E293B), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(fan_temp_bar, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_radius(fan_temp_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(fan_temp_bar, lv_color_hex(0x10B981), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(fan_temp_bar, LV_OPA_COVER, LV_PART_INDICATOR);
}


static void update_boost_peak_chip(int32_t peak_val) {
  if (!boost_peak_chip) return;
  if (peak_val < -100) peak_val = -100;
  if (peak_val > 200) peak_val = 200;

  float frac = static_cast<float>(peak_val + 100) / 300.0f;
  float angle_deg = 135.0f + frac * 270.0f;
  float angle_rad = angle_deg * (3.1415926535f / 180.0f);

  const float cx = 176.0f;
  const float cy = 176.0f;
  const float r_in = 113.0f;
  const float r_out = 127.0f;

  float cos_a = cosf(angle_rad);
  float sin_a = sinf(angle_rad);

  boost_peak_pts[0].x = static_cast<int32_t>(cx + r_in * cos_a);
  boost_peak_pts[0].y = static_cast<int32_t>(cy + r_in * sin_a);
  boost_peak_pts[1].x = static_cast<int32_t>(cx + r_out * cos_a);
  boost_peak_pts[1].y = static_cast<int32_t>(cy + r_out * sin_a);

  lv_line_set_points(boost_peak_chip, boost_peak_pts, 2);
}

void ui_gauges_reset_peak_boost() {
  max_recorded_boost = (last_boost != -9999) ? last_boost : -100;
  update_boost_peak_chip(max_recorded_boost);
}

static void boost_readout_click_cb(lv_event_t *e) {
  LV_UNUSED(e);
  ui_gauges_reset_peak_boost();
  extern void ui_drawer_notify_touch();
  ui_drawer_notify_touch();
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

  // Background track arc inside the scale (baked into static dial snapshot)
  lv_obj_t *bg_track = lv_arc_create(static_dial);
  lv_obj_set_size(bg_track, 240, 240);
  lv_obj_center(bg_track);
  lv_arc_set_rotation(bg_track, 135);
  lv_arc_set_bg_angles(bg_track, 0, 270);
  lv_arc_set_range(bg_track, -100, 200);
  lv_arc_set_value(bg_track, -100);
  lv_obj_remove_style(bg_track, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(bg_track, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_color(bg_track, lv_color_hex(0x1E293B), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(bg_track, LV_OPA_TRANSP, LV_PART_INDICATOR);

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

  // Dynamic boost value arc inside the scale with #53E2D0 stroke
  boost_arc = lv_arc_create(dial_parent);
  lv_obj_set_size(boost_arc, 240, 240);
  lv_obj_center(boost_arc);
  lv_arc_set_rotation(boost_arc, 135);
  lv_arc_set_bg_angles(boost_arc, 0, 270);
  lv_arc_set_range(boost_arc, -100, 200);
  lv_arc_set_value(boost_arc, -100);
  lv_obj_remove_style(boost_arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(boost_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(boost_arc, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_style_arc_opa(boost_arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(boost_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(boost_arc, lv_color_hex(0x53E2D0), LV_PART_INDICATOR);

  // Peak-hold chip line indicating highest recorded value
  boost_peak_chip = lv_line_create(dial_parent);
  lv_obj_set_style_line_width(boost_peak_chip, 4, 0);
  lv_obj_set_style_line_rounded(boost_peak_chip, true, 0);
  lv_obj_set_style_line_color(boost_peak_chip, lv_color_hex(0x53E2D0), 0);
  lv_obj_remove_flag(boost_peak_chip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(boost_peak_chip, LV_OBJ_FLAG_EVENT_BUBBLE);

  // Top title: "boost" in 18px font above readout
  boost_title_lbl = lv_label_create(dial_parent);
  lv_label_set_text(boost_title_lbl, "boost");
  lv_obj_set_style_text_color(boost_title_lbl, lv_color_hex(0x94A3B8), 0);
#if LV_FONT_MONTSERRAT_18
  lv_obj_set_style_text_font(boost_title_lbl, &lv_font_montserrat_18, 0);
#endif
  lv_obj_align(boost_title_lbl, LV_ALIGN_CENTER, 0, -40);

  // Center 48px digital value readout
  psi_val_lbl = lv_label_create(dial_parent);
  lv_label_set_text(psi_val_lbl, "0.0");
  lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xF1F5F9), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(psi_val_lbl, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(psi_val_lbl, LV_ALIGN_CENTER, 0, 2);
  lv_obj_add_flag(psi_val_lbl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(psi_val_lbl, boost_readout_click_cb, LV_EVENT_CLICKED, nullptr);

  // Bottom unit label: "psi" beneath readout
  boost_unit_lbl = lv_label_create(dial_parent);
  lv_label_set_text(boost_unit_lbl, "psi");
  lv_obj_set_style_text_color(boost_unit_lbl, lv_color_hex(0x64748B), 0);
  lv_obj_align(boost_unit_lbl, LV_ALIGN_CENTER, 0, 38);

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
  lv_obj_t *tile2 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);
  build_fans_tile(tile2);

  // Tile 3 (Table Switching): swipe left -> Tile 2
  lv_obj_t *tile3 = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_HOR);
  build_tables_tile(tile3);

  // Tile 4 (System Voltage): swipe left -> Tile 3
  lv_obj_t *tile4 = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT);
  build_voltage_tile(tile4);

  /// 5 Top Page Indicator Dots inside dots_container (82px wide)
  dots_container = lv_obj_create(parent);
  lv_obj_set_size(dots_container, 82, 16);
  lv_obj_align(dots_container, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_set_style_bg_opa(dots_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dots_container, 0, 0);
  lv_obj_set_style_pad_all(dots_container, 0, 0);
  lv_obj_set_style_opa(dots_container, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(dots_container, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(dots_container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t **dot_ptrs[] = { &dot_1, &dot_2, &dot_3, &dot_4, &dot_5 };
  const int32_t dot_x[] = { -28, -14, 0, 14, 28 };
  for (int i = 0; i < 5; i++) {
    *dot_ptrs[i] = lv_obj_create(dots_container);
    lv_obj_set_size(*dot_ptrs[i], 7, 7);
    lv_obj_align(*dot_ptrs[i], LV_ALIGN_CENTER, dot_x[i], 0);
    lv_obj_set_style_radius(*dot_ptrs[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(*dot_ptrs[i], lv_color_hex(i == 0 ? 0x38BDF8 : 0x334155), 0);
    lv_obj_set_style_border_width(*dot_ptrs[i], 0, 0);
    lv_obj_remove_flag(*dot_ptrs[i], LV_OBJ_FLAG_CLICKABLE);
  }
}

void ui_gauges_set_boost_tenths(int32_t psi_tenths) {
  if (psi_tenths == last_boost) return;
  last_boost = psi_tenths;

  if (boost_arc) {
    lv_arc_set_value(boost_arc, psi_tenths);
  }

  if (psi_tenths > max_recorded_boost) {
    max_recorded_boost = psi_tenths;
    update_boost_peak_chip(max_recorded_boost);
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
    } else {
      lv_obj_set_style_text_color(psi_val_lbl, lv_color_hex(0xF1F5F9), 0);
    }
  }
}
void ui_gauges_set_coolant_tenths(int32_t temp_tenths) {
  if (temp_tenths == last_temp) return;
  last_temp = temp_tenths;

  lv_color_t status_color = lv_color_hex(0x10B981);  // Normal green
  if (temp_tenths >= 2250) {
    status_color = lv_color_hex(0xEF4444);  // High-temp red
  } else if (temp_tenths >= 2050) {
    status_color = lv_color_hex(0xF59E0B);  // Warm amber
  }

  // Update Tile 1 full-screen arc gauge
  if (temp_arc) {
    lv_arc_set_value(temp_arc, temp_tenths);
    lv_obj_set_style_arc_color(temp_arc, status_color, LV_PART_INDICATOR);
  }
  if (temp_lbl) {
    lv_label_set_text_fmt(temp_lbl, "%ld °F", temp_tenths / 10);
  }

  // Update Tile 2 compact horizontal coolant bar beneath fan toggles
  if (fan_temp_bar) {
    lv_bar_set_value(fan_temp_bar, temp_tenths, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(fan_temp_bar, status_color, LV_PART_INDICATOR);
  }
  if (fan_temp_lbl) {
    lv_label_set_text_fmt(fan_temp_lbl, "Coolant: %ld °F", temp_tenths / 10);
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
