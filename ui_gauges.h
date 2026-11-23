#pragma once
#include <lvgl.h>
#include <stdint.h>

typedef void (*fan_toggle_cb_t)(uint8_t fan_index, bool on);
typedef void (*table_toggle_cb_t)(uint8_t table_index, bool table2_active);

void ui_gauges_create(lv_obj_t *parent);
void ui_gauges_set_boost_tenths(int32_t psi_tenths);
void ui_gauges_reset_peak_boost();
void ui_gauges_set_coolant_tenths(int32_t temp_tenths);
void ui_gauges_set_voltage_tenths(int32_t volts_tenths);
void ui_gauges_set_ethanol_pct(int32_t ethanol_pct);
void ui_gauges_set_dots_visible(bool show);

void ui_gauges_set_fan_callback(fan_toggle_cb_t cb);
void ui_gauges_set_fan_state(uint8_t fan_index, bool on);

void ui_gauges_set_table_callback(table_toggle_cb_t cb);
void ui_gauges_set_table_state(uint8_t table_index, bool table2_active);
