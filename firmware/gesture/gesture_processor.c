/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "gesture_features.h"
#include "gesture_model.h"
#include "gesture_processor.h"

static void copy_window_chronological(const GestureProcessor *processor, GestureFrame *out)
{
    int start = processor->write_index;

    for (int i = 0; i < GESTURE_WINDOW_FRAMES; i++) {
        int idx = (start + i) % GESTURE_WINDOW_FRAMES;
        out[i] = processor->window[idx];
    }
}

static bool is_output_label(GestureLabel label)
{
    return label == GESTURE_PINCH || label == GESTURE_CLENCH || label == GESTURE_UP ||
           label == GESTURE_DOWN;
}

static float threshold_for_label(GestureLabel label)
{
    if (label == GESTURE_PINCH) {
        return GESTURE_PINCH_THRESHOLD;
    }
    if (label == GESTURE_UP) {
        return GESTURE_UP_THRESHOLD;
    }
    if (label == GESTURE_DOWN) {
        return GESTURE_DOWN_THRESHOLD;
    }
    return GESTURE_MAIN_THRESHOLD;
}

static int confirm_windows_for_label(GestureLabel label)
{
    if (label == GESTURE_PINCH) {
        return GESTURE_PINCH_CONFIRM_WINDOWS;
    }
    if (label == GESTURE_DOWN) {
        return GESTURE_DOWN_CONFIRM_WINDOWS;
    }
    return GESTURE_CONFIRM_WINDOWS;
}

void gesture_processor_init(GestureProcessor *processor)
{
    if (processor == 0) {
        return;
    }
    memset(processor, 0, sizeof(*processor));
    processor->pending_label = GESTURE_NONE;
}

bool gesture_processor_push_frame(
    GestureProcessor *processor,
    const GestureFrame *frame,
    GestureResult *result)
{
    GestureFrame ordered[GESTURE_WINDOW_FRAMES];
    float features[GESTURE_FEATURE_COUNT];
    GesturePrediction pred;
    uint32_t current_frame;

    if (processor == 0 || frame == 0 || result == 0) {
        return false;
    }

    result->label = GESTURE_NONE;
    result->confidence = 0.0f;
    result->frame_index = processor->total_frames;

    processor->window[processor->write_index] = *frame;
    processor->write_index = (processor->write_index + 1) % GESTURE_WINDOW_FRAMES;
    processor->total_frames++;
    current_frame = processor->total_frames;
    if (processor->buffered_frames < GESTURE_WINDOW_FRAMES) {
        processor->buffered_frames++;
    }

    if (processor->buffered_frames < GESTURE_WINDOW_FRAMES ||
        (current_frame % GESTURE_STEP_FRAMES) != 0) {
        return false;
    }

    if (current_frame < processor->cooldown_until_frame) {
        return false;
    }

    copy_window_chronological(processor, ordered);
    gesture_extract_features(ordered, GESTURE_WINDOW_FRAMES, features);
    pred = gesture_model_predict(features);

    if (!is_output_label(pred.label) || pred.confidence < threshold_for_label(pred.label)) {
        processor->pending_label = GESTURE_NONE;
        processor->pending_count = 0;
        return false;
    }

    if (processor->pending_label == pred.label) {
        processor->pending_count++;
    } else {
        processor->pending_label = pred.label;
        processor->pending_count = 1;
    }

    if (processor->pending_count < confirm_windows_for_label(pred.label)) {
        return false;
    }

    result->label = pred.label;
    result->confidence = pred.confidence;
    result->frame_index = current_frame;

    processor->cooldown_until_frame = current_frame + GESTURE_COOLDOWN_FRAMES;
    processor->pending_label = GESTURE_NONE;
    processor->pending_count = 0;
    return true;
}
