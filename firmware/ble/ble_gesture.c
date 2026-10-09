/* SPDX-License-Identifier: MIT */
#include "ble_gesture.h"
#include "drivers/ble/gatt.h"
#include <string.h>

/* UUID: 00000001-0004-1000-8000-00805F9B05B5 (Primary Service) */
#define GESTURE_SVC_UUID_BYTES \
    0x01, 0x00, 0x00, 0x00, \
    0x04, 0x00, \
    0x00, 0x10, \
    0x80, 0x00, \
    0x00, 0x80, 0x5F, 0x9B, 0x05, 0xB5

/* UUID: 00000002-0004-1000-8000-00805F9B05B5 (Characteristic) */
#define GESTURE_CHRC_UUID_BYTES \
    0x02, 0x00, 0x00, 0x00, \
    0x04, 0x00, \
    0x00, 0x10, \
    0x80, 0x00, \
    0x00, 0x80, 0x5F, 0x9B, 0x05, 0xB5

static struct bt_uuid_128 gesture_svc_uuid = BT_UUID_INIT_128(
    GESTURE_SVC_UUID_BYTES);

static struct bt_uuid_128 gesture_chrc_uuid = BT_UUID_INIT_128(
    GESTURE_CHRC_UUID_BYTES);

#define GESTURE_SVC_UUID  BT_UUID_DECLARE_128(gesture_svc_uuid)
#define GESTURE_CHRC_UUID BT_UUID_DECLARE_128(gesture_chrc_uuid)

static uint8_t gesture_data[GESTURE_RESULT_LEN];

static ssize_t gesture_read_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len,
                               uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             gesture_data, sizeof(gesture_data));
}

static void gesture_ccc_cb(const struct bt_gatt_attr *attr, uint16_t value)
{
    (void)attr;
    (void)value;
}

BT_GATT_SERVICE_DEFINE(gesture_svc,
    BT_GATT_PRIMARY_SERVICE(GESTURE_SVC_UUID),
    BT_GATT_CHARACTERISTIC(GESTURE_CHRC_UUID,
                           BT_GATT_CHRC_READ |
                           BT_GATT_CHRC_NOTIFY |
                           BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_READ,
                           gesture_read_cb, NULL,
                           gesture_data),
    BT_GATT_CCC(gesture_ccc_cb,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

void ble_gesture_service_init(void)
{
    (void)memset(gesture_data, 0, sizeof(gesture_data));
}

void ble_send_gesture_result(uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }
    if (len > GESTURE_RESULT_LEN) {
        len = GESTURE_RESULT_LEN;
    }
    (void)memcpy(gesture_data, buf, len);
    (void)bt_gatt_notify(NULL, &gesture_svc.attrs[1],
                         gesture_data, sizeof(gesture_data));
}
