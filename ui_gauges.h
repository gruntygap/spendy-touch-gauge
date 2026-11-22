#pragma once
#include <lvgl.h>
#include <stdint.h>

void ui_gauges_create(lv_obj_t *parent);
void ui_gauges_set_boost_tenths(int32_t psi_tenths);
void ui_gauges_set_coolant_f(int32_t temp_f);
void ui_gauges_set_dots_visible(bool show);
