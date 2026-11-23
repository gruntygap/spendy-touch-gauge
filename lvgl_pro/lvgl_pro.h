/**
 * @file lvgl_pro.h
 */

#ifndef LVGL_PRO_LVGL_PRO_H
#define LVGL_PRO_LVGL_PRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/* Target(s) select at startup: one or more LVGL_PRO_TARGET_* values OR-ed together. */
#ifndef LVGL_PRO_COMPILE_TARGET
#define LVGL_PRO_COMPILE_TARGET LVGL_PRO_TARGET_ALL
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lvgl_pro_gen.h"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the component library
 */
void lvgl_pro_init(const char * asset_path);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_PRO_LVGL_PRO_H*/