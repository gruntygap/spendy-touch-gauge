#pragma once
#include <lvgl.h>

/**
 * @brief Pre-bake a 45x45 ARGB8888 yellow lightning bolt icon bitmap.
 * @param temp_parent Temporary parent object used for snapshot rendering.
 * @return Allocated lv_draw_buf_t* snapshot (ARGB8888 format).
 */
lv_draw_buf_t *create_lightning_bitmap(lv_obj_t *temp_parent);

/**
 * @brief Pre-bake a 45x45 ARGB8888 golden corn cob with green leaves icon bitmap.
 * @param temp_parent Temporary parent object used for snapshot rendering.
 * @return Allocated lv_draw_buf_t* snapshot (ARGB8888 format).
 */
lv_draw_buf_t *create_corn_bitmap(lv_obj_t *temp_parent);

/**
 * @brief Pre-bake a 45x45 ARGB8888 4-blade cooling fan icon bitmap.
 * @param temp_parent Temporary parent object used for snapshot rendering.
 * @return Allocated lv_draw_buf_t* snapshot (ARGB8888 format).
 */
lv_draw_buf_t *create_fan_bitmap(lv_obj_t *temp_parent);
