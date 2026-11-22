#pragma once
#include <lvgl.h>

static constexpr int32_t SCREEN_WIDTH  = 360;
static constexpr int32_t SCREEN_HEIGHT = 360;

bool display_driver_init();
void display_set_brightness(uint8_t pwm);
