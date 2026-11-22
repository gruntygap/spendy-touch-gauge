#include <Arduino.h>
#include <atomic>
#include <lvgl.h>
#include "display_driver.h"
#include "ui_gauges.h"
#include "ui_drawer.h"

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static std::atomic<int32_t> g_boost_psi_tenths{0};
static std::atomic<int32_t> g_coolant_temp_f{185};
static std::atomic<bool>    g_demo_mode{true};

// Core 0: High-Speed Telemetry / CAN Bus Sampling Task
void data_sampling_task(void *pvParameters) {
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(5);
    float phase = 0.0f;

    for (;;) {
        if (g_demo_mode.load(std::memory_order_relaxed)) {
            phase += 0.015f;
            int32_t sim_psi  = static_cast<int32_t>(50.0f + 135.0f * sinf(phase));
            int32_t sim_temp = static_cast<int32_t>(190.0f + 35.0f * cosf(phase * 0.4f));
            g_boost_psi_tenths.store(sim_psi, std::memory_order_relaxed);
            g_coolant_temp_f.store(sim_temp, std::memory_order_relaxed);
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

static void screen_touch_wake_cb(lv_event_t *e) {
    ui_drawer_notify_touch();
}

void setup() {
    if (!display_driver_init()) {
        while (true) delay(100);
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B1017), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, screen_touch_wake_cb, LV_EVENT_PRESSED, nullptr);

    ui_gauges_create(scr);
    ui_drawer_create(scr);

    lv_refr_now(nullptr);

    xTaskCreatePinnedToCore(
        data_sampling_task,
        "DataSampler",
        4096,
        nullptr,
        2,
        nullptr,
        0
    );
}

void loop() {
    ui_gauges_set_boost_tenths(g_boost_psi_tenths.load(std::memory_order_relaxed));
    ui_gauges_set_coolant_f(g_coolant_temp_f.load(std::memory_order_relaxed));

    lv_timer_handler();
    delay(1);
}

