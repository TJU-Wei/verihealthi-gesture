/* SPDX-License-Identifier: MIT */
#ifndef GESTURE_PROCESSOR_H
#define GESTURE_PROCESSOR_H

#include <stdbool.h>
#include "gesture_types.h"

#define GESTURE_MAIN_THRESHOLD 0.80f
#define GESTURE_PINCH_THRESHOLD 0.99f
#define GESTURE_UP_THRESHOLD 0.80f
#define GESTURE_DOWN_THRESHOLD 0.70f
#define GESTURE_CONFIRM_WINDOWS 2
#define GESTURE_PINCH_CONFIRM_WINDOWS 3
#define GESTURE_DOWN_CONFIRM_WINDOWS 1
#define GESTURE_COOLDOWN_FRAMES 60

typedef struct GestureProcessor {
    GestureFrame window[GESTURE_WINDOW_FRAMES];
    int write_index;
    int buffered_frames;
    uint32_t total_frames;
    GestureLabel pending_label;
    int pending_count;
    uint32_t cooldown_until_frame;
} GestureProcessor;

void gesture_processor_init(GestureProcessor *processor);
bool gesture_processor_push_frame(
    GestureProcessor *processor,
    const GestureFrame *frame,
    GestureResult *result);

#endif
