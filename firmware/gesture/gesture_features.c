/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stddef.h>
#include "gesture_features.h"

static void append_axis_features(const float *values, int count, float *features, int *idx)
{
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float min_v = values[0];
    float max_v = values[0];

    for (int i = 0; i < count; i++) {
        float v = values[i];
        sum += v;
        sum_sq += v * v;
        if (v < min_v) {
            min_v = v;
        }
        if (v > max_v) {
            max_v = v;
        }
    }

    float mean = sum / (float)count;
    float var = (sum_sq / (float)count) - (mean * mean);
    if (var < 0.0f) {
        var = 0.0f;
    }

    features[(*idx)++] = mean;
    features[(*idx)++] = sqrtf(var);
    features[(*idx)++] = min_v;
    features[(*idx)++] = max_v;
    features[(*idx)++] = max_v - min_v;
    features[(*idx)++] = sqrtf(sum_sq / (float)count);
}

static void append_summary_features(const float *values, int count, float *features, int *idx)
{
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float max_v = values[0];

    for (int i = 0; i < count; i++) {
        float v = values[i];
        sum += v;
        sum_sq += v * v;
        if (v > max_v) {
            max_v = v;
        }
    }

    float mean = sum / (float)count;
    float var = (sum_sq / (float)count) - (mean * mean);
    if (var < 0.0f) {
        var = 0.0f;
    }

    features[(*idx)++] = mean;
    features[(*idx)++] = sqrtf(var);
    features[(*idx)++] = max_v;
    features[(*idx)++] = sqrtf(sum_sq / (float)count);
}

void gesture_extract_features(const GestureFrame *window, int window_frames, float *features)
{
    float axes[6][GESTURE_WINDOW_FRAMES];
    float acc_mag[GESTURE_WINDOW_FRAMES];
    float gyro_mag[GESTURE_WINDOW_FRAMES];
    float jerk_mag[GESTURE_WINDOW_FRAMES - 1] = {0.0f};
    int idx = 0;

    if (window == NULL || features == NULL || window_frames <= 0 ||
        window_frames > GESTURE_WINDOW_FRAMES) {
        return;
    }

    for (int i = 0; i < window_frames; i++) {
        axes[0][i] = (float)window[i].gx;
        axes[1][i] = (float)window[i].gy;
        axes[2][i] = (float)window[i].gz;
        axes[3][i] = (float)window[i].ax;
        axes[4][i] = (float)window[i].ay;
        axes[5][i] = (float)window[i].az;
        gyro_mag[i] = sqrtf(axes[0][i] * axes[0][i] + axes[1][i] * axes[1][i] +
                            axes[2][i] * axes[2][i]);
        acc_mag[i] = sqrtf(axes[3][i] * axes[3][i] + axes[4][i] * axes[4][i] +
                           axes[5][i] * axes[5][i]);
    }

    for (int axis = 0; axis < 6; axis++) {
        append_axis_features(axes[axis], window_frames, features, &idx);
    }

    append_summary_features(acc_mag, window_frames, features, &idx);
    append_summary_features(gyro_mag, window_frames, features, &idx);

    for (int i = 0; i < window_frames - 1; i++) {
        float d0 = axes[0][i + 1] - axes[0][i];
        float d1 = axes[1][i + 1] - axes[1][i];
        float d2 = axes[2][i + 1] - axes[2][i];
        float d3 = axes[3][i + 1] - axes[3][i];
        float d4 = axes[4][i + 1] - axes[4][i];
        float d5 = axes[5][i + 1] - axes[5][i];
        jerk_mag[i] = sqrtf(d0 * d0 + d1 * d1 + d2 * d2 + d3 * d3 + d4 * d4 + d5 * d5);
    }
    append_summary_features(jerk_mag, window_frames > 1 ? window_frames - 1 : 1, features, &idx);

    int half = window_frames / 2;
    if (half < 1) {
        half = 1;
    }
    for (int axis = 0; axis < 6; axis++) {
        float first = 0.0f;
        float second = 0.0f;
        for (int i = 0; i < half; i++) {
            first += axes[axis][i];
        }
        for (int i = half; i < window_frames; i++) {
            second += axes[axis][i];
        }
        first /= (float)half;
        second /= (float)(window_frames - half);
        features[idx++] = second - first;
    }

    for (int axis = 0; axis < 3; axis++) {
        float sum = 0.0f;
        for (int i = 0; i < window_frames; i++) {
            sum += axes[axis][i];
        }
        features[idx++] = sum / (float)GESTURE_SAMPLE_RATE_HZ;
    }

    float ax0 = axes[3][0];
    float ay0 = axes[4][0];
    float az0 = axes[5][0];
    float ax1 = axes[3][window_frames - 1];
    float ay1 = axes[4][window_frames - 1];
    float az1 = axes[5][window_frames - 1];
    float pitch0 = atan2f(-ax0, sqrtf(ay0 * ay0 + az0 * az0));
    float pitch1 = atan2f(-ax1, sqrtf(ay1 * ay1 + az1 * az1));
    float roll0 = atan2f(ay0, az0);
    float roll1 = atan2f(ay1, az1);

    features[idx++] = pitch1 - pitch0;
    features[idx++] = roll1 - roll0;
}
