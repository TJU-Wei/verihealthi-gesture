/* SPDX-License-Identifier: MIT */
#ifndef GESTURE_PARAMS_H
#define GESTURE_PARAMS_H

#include "gesture_types.h"

#define GESTURE_MAIN_HIDDEN_COUNT 32
#define GESTURE_MAIN_CLASS_COUNT 5
#define GESTURE_PC_HIDDEN_COUNT 16

extern const float gesture_main_scaler_mean[GESTURE_FEATURE_COUNT];
extern const float gesture_main_scaler_scale[GESTURE_FEATURE_COUNT];
extern const float gesture_main_w0[GESTURE_FEATURE_COUNT][GESTURE_MAIN_HIDDEN_COUNT];
extern const float gesture_main_b0[GESTURE_MAIN_HIDDEN_COUNT];
extern const float gesture_main_w1[GESTURE_MAIN_HIDDEN_COUNT][GESTURE_MAIN_CLASS_COUNT];
extern const float gesture_main_b1[GESTURE_MAIN_CLASS_COUNT];

extern const float gesture_pc_scaler_mean[GESTURE_FEATURE_COUNT];
extern const float gesture_pc_scaler_scale[GESTURE_FEATURE_COUNT];
extern const float gesture_pc_w0[GESTURE_FEATURE_COUNT][GESTURE_PC_HIDDEN_COUNT];
extern const float gesture_pc_b0[GESTURE_PC_HIDDEN_COUNT];
extern const float gesture_pc_w1[GESTURE_PC_HIDDEN_COUNT];
extern const float gesture_pc_b1;

#endif
