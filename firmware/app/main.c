/* SPDX-License-Identifier: MIT */
/*
 * IMU acquisition and gesture recognition application.
 * Gesture algorithm implementation is split into gesture_*.c/.h modules.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vs_conf.h"
#include "soc_init.h"
#include "bsp.h"
#include "uart_printf.h"
#include "board.h"
#include "osal.h"
#include "vpi_error.h"
#include "hal_imu.h"
#include "gesture_processor.h"
#include "main.h"

#define GESTURE_CRC_LIMIT_BYTES 320000u
#define CRC8_SMBUS_INIT 0x00u

static uint8_t crc8_smbus_update(uint8_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

#define ALGO_EVENT_IMU_BLOCK 2u
#define IMU_READ_FRAMES 5u
#define IMU_BLOCK_COUNT 4u
#define IMU_ALGO_BYTES_PER_FRAME (2u * 7u)

#ifndef OSAL_SUCCESS
#define OSAL_SUCCESS 0
#endif

typedef struct ImuDataBlock {
    volatile bool in_use;
    uint16_t count;
    ImuGyroAccelData frames[IMU_READ_FRAMES];
} ImuDataBlock;

static void *g_algo_event_queue;
static ImuDevice *g_imu_dev;
static ImuDataBlock g_imu_blocks[IMU_BLOCK_COUNT];
static uint32_t g_crc_bytes;
static uint8_t g_crc8;
static bool g_crc_printed;
static volatile bool g_imu_irq_pending;

static void imu_data_ready_isr(void)
{
    /* ISR 中只置位，读取与事件发送都放到 imu_task 中执行。 */
    g_imu_irq_pending = true;
}

static bool app_should_print_label(GestureLabel label)
{
    return label == GESTURE_PINCH || label == GESTURE_CLENCH || label == GESTURE_UP ||
           label == GESTURE_DOWN;
}

static GestureFrame convert_imu_frame(const ImuGyroAccelData *src)
{
    GestureFrame frame;

    frame.gx = src->gx;
    frame.gy = src->gy;
    frame.gz = src->gz;
    frame.ax = src->ax;
    frame.ay = src->ay;
    frame.az = src->az;
    return frame;
}

static void update_crc8_for_imu_data(const ImuGyroAccelData *frames, uint16_t count)
{
    const uint8_t *data = (const uint8_t *)frames;
    uint32_t bytes = (uint32_t)count * (uint32_t)sizeof(ImuGyroAccelData);
    uint32_t remain;

    /* 只统计前 320000 字节 IMU 原始数据，用于输入流校验。 */
    if (g_crc_bytes >= GESTURE_CRC_LIMIT_BYTES) {
        return;
    }

    remain = GESTURE_CRC_LIMIT_BYTES - g_crc_bytes;
    if (bytes > remain) {
        bytes = remain;
    }

    g_crc8 = crc8_smbus_update(g_crc8, data, bytes);
    g_crc_bytes += bytes;

    if (g_crc_bytes >= GESTURE_CRC_LIMIT_BYTES && !g_crc_printed) {
        uart_printf("CRC8_SMBus = 0x%02X\r\n", (unsigned int)g_crc8);
        g_crc_printed = true;
    }
}

static ImuDataBlock *alloc_imu_block(void)
{
    while (1) {
        for (uint32_t i = 0; i < IMU_BLOCK_COUNT; i++) {
            if (!g_imu_blocks[i].in_use) {
                g_imu_blocks[i].in_use = true;
                return &g_imu_blocks[i];
            }
        }
        osal_sleep(1);
    }
}

static void release_imu_block(ImuDataBlock *block)
{
    if (block != NULL) {
        block->count = 0;
        block->in_use = false;
    }
}

static void send_imu_block_to_algo(const ImuGyroAccelData *frames, uint16_t count)
{
    ImuDataBlock *block;

    if (count == 0 || g_algo_event_queue == NULL) {
        return;
    }

    /* 复制到静态 RAM block 后再通过 Event 交给算法任务。 */
    block = alloc_imu_block();
    block->count = count;
    memcpy(block->frames, frames, (size_t)count * sizeof(ImuGyroAccelData));

    if (osal_send_event(g_algo_event_queue, ALGO_EVENT_IMU_BLOCK, block, 10) != OSAL_SUCCESS) {
        release_imu_block(block);
    }
}

static int init_imu_device(void)
{
    int ret;

    /* QEMU board 中注册imu_simulator， IMU HAL接口读取。 */
    g_imu_dev = hal_imu_get_device(IMU_DEV_ID_0);
    if (g_imu_dev == NULL) {
        return VPI_ERR_NODEVICE;
    }

    ret = hal_imu_init(g_imu_dev);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    ret = hal_imu_set_accel_cfg(g_imu_dev, 8, 0, GESTURE_SAMPLE_RATE_HZ, IMU_SENSOR_ODR);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    ret = hal_imu_set_gyro_cfg(g_imu_dev, 500, 0, GESTURE_SAMPLE_RATE_HZ, IMU_SENSOR_ODR);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    (void)hal_imu_set_fifo_wm(g_imu_dev, FIFO_WATERMARK_LEVEL);
    ret = hal_imu_set_fifo_cfg(g_imu_dev, (uint8_t)(IMU_FIFO_ACCEL | IMU_FIFO_GYRO), true);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    ret = hal_imu_flush_fifo(g_imu_dev);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    ret = hal_imu_set_work_mode(g_imu_dev, IMU_ACCEL_GYRO, IMU_SEN_MODE_NORMAL);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    /* QEMU imu_simulator 使用 FIFO watermark 中断读取 gyro+accel 数据。 */
    ret = hal_imu_cfg_interrupt(g_imu_dev, true, IMU_ACC_GYRO_FIFO_WATERMARK_INTERRUPT, NULL);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    ret = hal_imu_enable_interrupt(g_imu_dev, IMU_DATA_PIN, true, imu_data_ready_isr);
    if (ret != 0) {
        return vsd_to_vpi(ret);
    }

    return VPI_SUCCESS;
}

static void imu_task(void *param)
{
    ImuGyroAccelData raw[IMU_READ_FRAMES];
    int ret;

    (void)param;

    /* 负责 IMU 初始化、等待中断标志、读取数据并发送给算法任务。 */
    ret = init_imu_device();
    if (ret != VPI_SUCCESS) {
        uart_printf("imu init failed %d\r\n", ret);
        goto exit;
    }

    g_imu_irq_pending = true;

    while (1) {
        uint16_t available = 0;

        if (!g_imu_irq_pending) {
            osal_sleep(5);
            continue;
        }
        g_imu_irq_pending = false;

        ret = hal_imu_read_gyro_accel(g_imu_dev, raw, IMU_READ_FRAMES, &available);
        if (ret == 0 && available > 0) {
            /* 防御异常返回值，避免超过 raw 数组容量。 */
            if (available > IMU_READ_FRAMES) {
                available = IMU_READ_FRAMES;
            }
            update_crc8_for_imu_data(raw, available);
            send_imu_block_to_algo(raw, available);
        } else if (ret != 0) {
            uart_printf("imu read failed raw=%d vpi=%d available=%u\r\n", ret, vsd_to_vpi(ret),
                        (unsigned int)available);
            (void)hal_imu_flush_fifo(g_imu_dev);
            osal_sleep(20);
        } else {
            osal_sleep(5);
        }

        (void)hal_imu_enable_interrupt(g_imu_dev, IMU_DATA_PIN, true, imu_data_ready_isr);
    }

exit:
    osal_delete_task(NULL);
}

static void algo_task(void *param)
{
    GestureProcessor processor;
    uint32_t processed_frames = 0;

    (void)param;
    gesture_processor_init(&processor);

    while (1) {
        void *event_data = NULL;
        int event_id = osal_wait_event(g_algo_event_queue, &event_data, OSAL_WAIT_FOREVER);
        ImuDataBlock *block;

        if ((uint32_t)event_id != ALGO_EVENT_IMU_BLOCK || event_data == NULL) {
            continue;
        }

        block = (ImuDataBlock *)event_data;
        for (uint16_t i = 0; i < block->count; i++) {
            GestureFrame frame = convert_imu_frame(&block->frames[i]);
            GestureResult result;

            processed_frames++;
            /* 算法返回结果，统一在非算法模块中打印。 */
            if (gesture_processor_push_frame(&processor, &frame, &result) &&
                app_should_print_label(result.label)) {
                uint32_t processed_algo_bytes = processed_frames * IMU_ALGO_BYTES_PER_FRAME;
                uint32_t time_ms =
                    (processed_algo_bytes * 1000u) /
                    (IMU_ALGO_BYTES_PER_FRAME * (uint32_t)GESTURE_SAMPLE_RATE_HZ);
                uart_printf("%ums, %s\r\n", (unsigned int)time_ms, gesture_label_name(result.label));
            }
        }
        release_imu_block(block);
    }
}

static void task_init_app(void *param)
{
    int ret;
    BoardDevice board_dev;

    (void)param;

    ret = board_register(board_get_ops());
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board register failed %d\r\n", ret);
        goto exit;
    }

    ret = board_init((void *)&board_dev);
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board init failed %d\r\n", ret);
        goto exit;
    }

    if (board_dev.name) {
        uart_printf("Board: %s\r\n", board_dev.name);
    }

    /* item_size 包含 OSAL 内部链表节点 */
    g_algo_event_queue = osal_create_event_queue(16, sizeof(OsalEventEntry));
    if (g_algo_event_queue == NULL) {
        uart_printf("event queue create failed\r\n");
        goto exit;
    }

    g_crc8 = CRC8_SMBUS_INIT;
    g_crc_bytes = 0;
    g_crc_printed = false;
    memset(g_imu_blocks, 0, sizeof(g_imu_blocks));

    osal_create_task(algo_task, "algo_task", 8192, 4, NULL);
    osal_create_task(imu_task, "imu_task", 2048, 5, NULL);

exit:
    osal_delete_task(NULL);
}

int main(void)
{
    int ret;

    ret = soc_init();
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("soc init error %d\r\n", ret);
        goto exit;
    }

    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
    osal_start_scheduler();

exit:
    goto exit;
}
