#include "ui_bitmaps.h"

lv_draw_buf_t *create_lightning_bitmap(lv_obj_t *temp_parent) {
  constexpr int32_t SIZE = 45;

  lv_obj_t *holder = lv_obj_create(temp_parent);
  lv_obj_remove_style_all(holder);
  lv_obj_set_size(holder, SIZE, SIZE);
  lv_obj_set_style_bg_opa(holder, LV_OPA_TRANSP, 0);

  // Bold zig-zag lightning bolt centered on 45x45
  static lv_point_precise_t bolt_pts[] = {
    { 26, 4 },
    { 12, 23 },
    { 23, 23 },
    { 18, 41 },
    { 33, 21 },
    { 22, 21 },
    { 26, 4 }
  };

  lv_obj_t *bolt = lv_line_create(holder);
  lv_line_set_points(bolt, bolt_pts, 7);
  lv_obj_set_style_line_width(bolt, 4, 0);
  lv_obj_set_style_line_color(bolt, lv_color_hex(0xFACC15), 0);
  lv_obj_set_style_line_rounded(bolt, true, 0);

  lv_obj_update_layout(holder);
  lv_draw_buf_t *snap = lv_snapshot_take(holder, LV_COLOR_FORMAT_ARGB8888);
  lv_obj_delete(holder);
  return snap;
}

lv_draw_buf_t *create_corn_bitmap(lv_obj_t *temp_parent) {
  constexpr int32_t SIZE = 45;

  lv_obj_t *holder = lv_obj_create(temp_parent);
  lv_obj_remove_style_all(holder);
  lv_obj_set_size(holder, SIZE, SIZE);
  lv_obj_set_style_bg_opa(holder, LV_OPA_TRANSP, 0);

  // Golden corn ear body (oval pill in center)
  lv_obj_t *cob = lv_obj_create(holder);
  lv_obj_remove_style_all(cob);
  lv_obj_set_size(cob, 17, 31);
  lv_obj_align(cob, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_set_style_radius(cob, 9, 0);
  lv_obj_set_style_bg_color(cob, lv_color_hex(0xFACC15), 0);
  lv_obj_set_style_bg_opa(cob, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(cob, lv_color_hex(0xCA8A04), 0);
  lv_obj_set_style_border_width(cob, 1, 0);

  // Subtle kernel grid dots across the cob
  const int32_t kx[] = { -3, 3, -3, 3, -3, 3 };
  const int32_t ky[] = { -7, -7, -1, -1, 5, 5 };
  for (int i = 0; i < 6; i++) {
    lv_obj_t *kernel = lv_obj_create(cob);
    lv_obj_remove_style_all(kernel);
    lv_obj_set_size(kernel, 3, 3);
    lv_obj_align(kernel, LV_ALIGN_CENTER, kx[i], ky[i]);
    lv_obj_set_style_radius(kernel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(kernel, lv_color_hex(0xFEF08A), 0);
    lv_obj_set_style_bg_opa(kernel, LV_OPA_COVER, 0);
  }

  // Left green husk leaf wrapping the lower cob
  lv_obj_t *leaf_l = lv_obj_create(holder);
  lv_obj_remove_style_all(leaf_l);
  lv_obj_set_size(leaf_l, 12, 20);
  lv_obj_align(leaf_l, LV_ALIGN_BOTTOM_LEFT, 9, -4);
  lv_obj_set_style_radius(leaf_l, 6, 0);
  lv_obj_set_style_bg_color(leaf_l, lv_color_hex(0x22C55E), 0);
  lv_obj_set_style_bg_opa(leaf_l, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_rotation(leaf_l, -220, 0);  // -22° tilt

  // Right green husk leaf wrapping the lower cob
  lv_obj_t *leaf_r = lv_obj_create(holder);
  lv_obj_remove_style_all(leaf_r);
  lv_obj_set_size(leaf_r, 12, 20);
  lv_obj_align(leaf_r, LV_ALIGN_BOTTOM_RIGHT, -9, -4);
  lv_obj_set_style_radius(leaf_r, 6, 0);
  lv_obj_set_style_bg_color(leaf_r, lv_color_hex(0x16A34A), 0);
  lv_obj_set_style_bg_opa(leaf_r, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_rotation(leaf_r, 220, 0);  // +22° tilt

  lv_obj_update_layout(holder);
  lv_draw_buf_t *snap = lv_snapshot_take(holder, LV_COLOR_FORMAT_ARGB8888);
  lv_obj_delete(holder);
  return snap;
}

lv_draw_buf_t *create_fan_bitmap(lv_obj_t *temp_parent) {
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
