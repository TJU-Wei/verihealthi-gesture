/* SPDX-License-Identifier: MIT */
#ifndef BLE_GESTURE_H
#define BLE_GESTURE_H

#include <stdint.h>

#define GESTURE_RESULT_LEN 8

void ble_gesture_service_init(void);
void ble_send_gesture_result(uint8_t *buf, uint16_t len);

#endif /* BLE_GESTURE_H */
