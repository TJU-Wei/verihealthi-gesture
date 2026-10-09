/* SPDX-License-Identifier: MIT */
#ifndef GESTURE_MODEL_H
#define GESTURE_MODEL_H

#include "gesture_types.h"

typedef struct GesturePrediction {
    GestureLabel label;
    float confidence;
} GesturePrediction;

GesturePrediction gesture_model_predict(const float *features);

#endif
