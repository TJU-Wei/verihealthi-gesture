/* SPDX-License-Identifier: MIT */
#include "gesture_types.h"

const char *gesture_label_name(GestureLabel label)
{
    switch (label) {
    case GESTURE_PINCH:
        return "pinch";
    case GESTURE_CLENCH:
        return "clench";
    case GESTURE_UP:
        return "up";
    case GESTURE_DOWN:
        return "down";
    case GESTURE_OTHERS:
        return "others";
    default:
        return "none";
    }
}
