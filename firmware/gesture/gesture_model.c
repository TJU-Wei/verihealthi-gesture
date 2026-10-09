/* SPDX-License-Identifier: MIT */
#include <math.h>
#include "gesture_model.h"
#include "gesture_params.h"

static float relu(float x)
{
    return x > 0.0f ? x : 0.0f;
}

static float sigmoid(float x)
{
    if (x >= 0.0f) {
        float z = expf(-x);
        return 1.0f / (1.0f + z);
    }
    float z = expf(x);
    return z / (1.0f + z);
}

static void main_predict_raw(const float *features, GestureLabel *label, float *confidence)
{
    float scaled[GESTURE_FEATURE_COUNT];
    float hidden[GESTURE_MAIN_HIDDEN_COUNT];
    float logits[GESTURE_MAIN_CLASS_COUNT];
    float max_logit;
    float sum_exp = 0.0f;
    int best = 0;

    for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
        scaled[i] = (features[i] - gesture_main_scaler_mean[i]) / gesture_main_scaler_scale[i];
    }

    for (int h = 0; h < GESTURE_MAIN_HIDDEN_COUNT; h++) {
        float acc = gesture_main_b0[h];
        for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
            acc += scaled[i] * gesture_main_w0[i][h];
        }
        hidden[h] = relu(acc);
    }

    for (int c = 0; c < GESTURE_MAIN_CLASS_COUNT; c++) {
        float acc = gesture_main_b1[c];
        for (int h = 0; h < GESTURE_MAIN_HIDDEN_COUNT; h++) {
            acc += hidden[h] * gesture_main_w1[h][c];
        }
        logits[c] = acc;
    }

    max_logit = logits[0];
    for (int c = 1; c < GESTURE_MAIN_CLASS_COUNT; c++) {
        if (logits[c] > max_logit) {
            max_logit = logits[c];
        }
    }

    for (int c = 0; c < GESTURE_MAIN_CLASS_COUNT; c++) {
        float prob = expf(logits[c] - max_logit);
        logits[c] = prob;
        sum_exp += prob;
    }

    for (int c = 1; c < GESTURE_MAIN_CLASS_COUNT; c++) {
        if (logits[c] > logits[best]) {
            best = c;
        }
    }

    *label = (GestureLabel)best;
    *confidence = logits[best] / sum_exp;
}

static GestureLabel pc_predict(const float *features, float *confidence)
{
    float scaled[GESTURE_FEATURE_COUNT];
    float hidden[GESTURE_PC_HIDDEN_COUNT];
    float logit = gesture_pc_b1;
    float p_clench;

    for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
        scaled[i] = (features[i] - gesture_pc_scaler_mean[i]) / gesture_pc_scaler_scale[i];
    }

    for (int h = 0; h < GESTURE_PC_HIDDEN_COUNT; h++) {
        float acc = gesture_pc_b0[h];
        for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
            acc += scaled[i] * gesture_pc_w0[i][h];
        }
        hidden[h] = relu(acc);
    }

    for (int h = 0; h < GESTURE_PC_HIDDEN_COUNT; h++) {
        logit += hidden[h] * gesture_pc_w1[h];
    }

    p_clench = sigmoid(logit);
    if (p_clench >= 0.5f) {
        *confidence = p_clench;
        return GESTURE_CLENCH;
    }
    *confidence = 1.0f - p_clench;
    return GESTURE_PINCH;
}

GesturePrediction gesture_model_predict(const float *features)
{
    GesturePrediction pred;

    pred.label = GESTURE_NONE;
    pred.confidence = 0.0f;
    if (features == 0) {
        return pred;
    }

    main_predict_raw(features, &pred.label, &pred.confidence);
    if (pred.label == GESTURE_PINCH || pred.label == GESTURE_CLENCH) {
        float pc_confidence = 0.0f;
        GestureLabel pc_label = pc_predict(features, &pc_confidence);
        pred.label = pc_label;
        if (pc_confidence < pred.confidence) {
            pred.confidence = pc_confidence;
        }
    }

    return pred;
}
