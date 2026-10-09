/* SPDX-License-Identifier: MIT */
#ifndef GESTURE_TYPES_H
#define GESTURE_TYPES_H

#include <stdint.h>

#define GESTURE_SAMPLE_RATE_HZ 50
#define GESTURE_WINDOW_FRAMES 50
#define GESTURE_STEP_FRAMES 5
#define GESTURE_FEATURE_COUNT 59

typedef enum GestureLabel {
    GESTURE_PINCH = 0,
    GESTURE_CLENCH = 1,
    GESTURE_UP = 2,
    GESTURE_DOWN = 3,
    GESTURE_OTHERS = 4,
    GESTURE_NONE = 5,
} GestureLabel;

typedef struct GestureFrame {
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t ax;
    int16_t ay;
    int16_t az;
} GestureFrame;

typedef struct GestureResult {
    GestureLabel label;
    float confidence;
    uint32_t frame_index;
} GestureResult;

const char *gesture_label_name(GestureLabel label);

#endif
