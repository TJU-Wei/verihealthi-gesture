/* SPDX-License-Identifier: MIT */
#ifndef GESTURE_FEATURES_H
#define GESTURE_FEATURES_H

#include "gesture_types.h"

void gesture_extract_features(const GestureFrame *window, int window_frames, float *features);

#endif
