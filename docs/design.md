# 手势识别设计说明

[English](design.en.md)

本仓库实现 IMU 六轴手势识别。固件跑在 VeriHealthi QEMU SDK 上，训练脚本在主机上运行。官方 SDK 和 IMU 数据集不包含在本仓库中。

## 1. 方案

系统在 SDK 工程里创建两个任务：

1. `imu_task` 通过 IMU HAL 初始化并读取数据。
2. `algo_task` 接收 IMU 数据块，调用手势识别，并统一打印结果。

QEMU 环境中的 IMU 数据由 SDK 内置的 `imu_simulator` 提供。应用层调用 `hal_imu_read_gyro_accel()` 读取陀螺仪和加速度计。采样率 50 Hz，每次最多读 5 帧，再通过 OSAL Event 交给算法任务。

有效手势：

- pinch：双指互点
- clench：握拳
- up：抬腕
- down：放下

其它动作不打印。

模块划分：

- `gesture_types`：手势类型、标签名称、基础数据结构
- `gesture_features`：滑动窗口特征提取
- `gesture_model`：模型推理
- `gesture_params`：模型参数、归一化参数、阈值参数
- `gesture_processor`：实时窗口、确认和冷却
- `main.c`：初始化、IMU 采集、任务通信、输出打印
- `firmware/ble`：把手势结果放到 GATT 特征里，供主机读取或订阅

## 2. 算法

### 2.1 输入

每帧使用六轴原始值：`gx`、`gy`、`gz`、`ax`、`ay`、`az`。SDK 的 `ImuGyroAccelData` 还带有 `sensortime` 或 `data_set_idx`，这些字段不参与识别。

### 2.2 滑动窗口

窗口长度 1.0 秒，即 50 帧。步长 0.1 秒，即 5 帧。窗口攒满后，`gesture_processor` 提取特征并调用分类模型。

### 2.3 特征

对窗口内六轴数据计算：

- 各轴均值、标准差、最小值、最大值
- 加速度模长、角速度模长
- 帧间变化和 jerk
- 窗口内运动强度和方向变化

特征先用导出的 scaler 参数归一化，再送入分类器。

### 2.4 模型

离线训练使用 scikit-learn 的 `MLPClassifier`，隐层大小为 32，做五分类：pinch、clench、up、down、others。按 `user_id` 做 group split，避免同一用户同时出现在训练集和测试集。

导出后的权重、scaler 和阈值放在 `firmware/gesture/gesture_params.c`。`gesture_model.c` 输出各类置信度。

### 2.5 后处理

- 全局置信度阈值：0.80
- pinch：0.99，连续 3 个窗口确认
- up：0.80，默认连续 2 个窗口确认
- down：0.70，1 个窗口确认
- clench 使用全局阈值 0.80 和默认确认窗口 2
- 冷却：60 帧，约 1.2 秒

通过阈值和确认后，`gesture_processor` 返回结果。`algo_task` 决定是否打印。

### 2.6 输出

只打印 pinch、clench、up、down。`others` 不打印。

```text
1000ms, clench
```

时间按已处理帧数计算：`time_ms = processed_frames * 1000 / 50`。50 Hz 下每帧 20 ms。

算法模块不直接打印，结果经返回值交给 `algo_task`。

## 3. 软件流程

### 3.1 启动

1. `main()` 调用 `soc_init()`。
2. 调用 `osal_pre_start_scheduler()`。
3. 创建 `task_init_app` 并启动调度器。
4. `task_init_app` 注册并初始化 board。`board_init()` 注册 QEMU IMU simulator。
5. 创建 OSAL Event 队列，再创建 `algo_task` 和 `imu_task`。

### 3.2 IMU 初始化

1. `hal_imu_get_device(IMU_DEV_ID_0)` 取得设备。
2. `hal_imu_init()`。
3. 加速度计和陀螺仪都配置为 50 Hz。
4. `hal_imu_set_fifo_cfg(IMU_FIFO_ACCEL | IMU_FIFO_GYRO, true)`。
5. 清空 FIFO，以 `IMU_ACCEL_GYRO` / `IMU_SEN_MODE_NORMAL` 启动。
6. 配置 FIFO watermark 中断，回调为 `imu_data_ready_isr`。

中断里只把 `g_imu_irq_pending` 置位。读取和发送事件都在 `imu_task` 中进行。

### 3.3 读取与通信

`imu_task` 看到 pending 后清除标志，调用 `hal_imu_read_gyro_accel()`，最多取 5 帧。`available` 大于 `IMU_READ_FRAMES` 时截断到 5，避免越界。读取成功后更新 CRC8/SMBus 统计，把数据复制到 `ImuDataBlock`，再 `osal_send_event()`。失败时打印错误码、清空 FIFO 并短暂休眠。读完后重新打开中断。

队列用 `osal_create_event_queue(16, sizeof(OsalEventEntry))` 创建。事件号为 `ALGO_EVENT_IMU_BLOCK`。

`algo_task` 收到数据块后，把每帧转成 `GestureFrame`，调用 `gesture_processor_push_frame()`。有效的四类结果按统一格式打印，然后释放数据块。

### 3.4 CRC

对最先读到的 IMU 原始数据做 CRC8/SMBus 统计，上限 `GESTURE_CRC_LIMIT_BYTES`（320000 字节）。达到上限后打印 `CRC8_SMBus = 0xXX`。CRC 只用来核对输入流。

## 4. 接入 SDK

自备 VeriHealthi QEMU SDK 202606 preliminary。将 `firmware/app` 与 `firmware/gesture` 覆盖到 `galaxy_sdk/`。`firmware/app/main.h` 只保留头文件保护，用来替换 SDK 自带的示例 `main.h`；不要把 SDK 的 `bsp`、`drivers`、`os`、`prebuilts` 放进本仓库。`firmware/ble` 按原工程加入 BLE 模块，它依赖 SDK 的 `drivers/ble/gatt.h`。

本仓库不包含官方 IMU 录音，也不包含由官方录音导出的回放数组。

## 5. 训练

依赖见 `training/requirements.txt`：`numpy`、`scikit-learn`、`joblib`。

数据集由使用者自行准备，目录示例：

```text
VeriHealthi_IMU_Dataset/
    pinch/
        IMU_xxx.txt
        xxx.csv
    clench/
    up/
    down/
    others/
        IMU_xxx.txt
```

txt 前 5 行是头信息，从第 6 行起每 7 个 int16 为一帧：`gx, gy, gz, ax, ay, az, debug`。采样率默认 50 Hz。`gesture_lib/data.py` 读取 txt 和 csv，`dataset.py` 按标签时间切 1 秒正样本，并从无动作区间切 `others` 负样本，`features.py` 提取特征。

```bash
cd training
pip install -r requirements.txt
python build_dataset.py --dataset <数据集路径>
python train_gesture_model.py
```

`build_dataset.py` 默认写出 `outputs/phase1/features_dataset.npz`，以及同名的 `.json` 统计。`train_gesture_model.py` 默认写出 `outputs/phase1/gesture_mlp.joblib` 和 `train_report.json`。这些输出不纳入版本库。把训练好的权重导出到 `gesture_params.c` 后，固件才能使用新模型。

## 6. 作者

- 韦晓蜃：算法设计与代码整合。邮箱 2162648164wxs@gmail.com
- 杨永康：嵌入式。邮箱 coolcool1887@gmail.com
- 张立坤：BLE。邮箱 3574715686@qq.com
