# Gesture Recognition Design

[中文](design.md)

This repository implements six-axis IMU gesture recognition. The firmware runs on the VeriHealthi QEMU SDK. The training scripts run on the host. The official SDK and IMU dataset are not included.

## 1. Overview

The application creates two tasks inside the SDK project:

1. `imu_task` initializes the IMU through the IMU HAL and reads samples.
2. `algo_task` receives IMU blocks, runs gesture recognition, and prints results.

In QEMU, IMU samples come from the SDK's built-in `imu_simulator`. The application reads the gyroscope and accelerometer with `hal_imu_read_gyro_accel()`. The sample rate is 50 Hz. Each read takes at most 5 frames, which are then passed to the algorithm task through an OSAL event.

Recognized gestures:

- pinch: two fingers tap each other
- clench: make a fist
- up: raise the wrist
- down: lower the wrist

Other motions are not printed.

Modules:

- `gesture_types`: gesture types, label names, and basic data structures
- `gesture_features`: sliding-window feature extraction
- `gesture_model`: model inference
- `gesture_params`: model parameters, normalization parameters, and thresholds
- `gesture_processor`: live windows, confirmation, and cooldown
- `main.c`: initialization, IMU capture, task communication, and printing
- `firmware/ble`: exposes the gesture result in a GATT characteristic for the host to read or subscribe to

## 2. Algorithm

### 2.1 Input

Each frame uses the raw six-axis values `gx`, `gy`, `gz`, `ax`, `ay`, and `az`. `ImuGyroAccelData` from the SDK also carries `sensortime` or `data_set_idx`. Those fields are not used for recognition.

### 2.2 Sliding window

The window is 1.0 second, which is 50 frames. The step is 0.1 second, which is 5 frames. Once a window is full, `gesture_processor` extracts features and runs the classifier.

### 2.3 Features

For the six axes inside a window, the extractor computes:

- per-axis mean, standard deviation, minimum, and maximum
- acceleration magnitude and angular-velocity magnitude
- frame-to-frame change and jerk
- motion intensity and direction change inside the window

Features are normalized with the exported scaler parameters, then passed to the classifier.

### 2.4 Model

Offline training uses scikit-learn `MLPClassifier` with a hidden layer of size 32. It classifies five labels: pinch, clench, up, down, and others. Splits are grouped by `user_id`, so the same user does not appear in both the training set and the test set.

The exported weights, scaler, and thresholds live in `firmware/gesture/gesture_params.c`. `gesture_model.c` returns a confidence for each class.

### 2.5 Post-processing

- Global confidence threshold: 0.80
- pinch: 0.99, confirmed across 3 consecutive windows
- up: 0.80, confirmed across the default 2 consecutive windows
- down: 0.70, confirmed in 1 window
- clench uses the global threshold 0.80 and the default confirmation length of 2 windows
- Cooldown: 60 frames, about 1.2 seconds

After the threshold and confirmation checks pass, `gesture_processor` returns the result. `algo_task` decides whether to print it.

### 2.6 Output

Only pinch, clench, up, and down are printed. `others` is not printed.

```text
1000ms, clench
```

Time comes from the number of frames already processed: `time_ms = processed_frames * 1000 / 50`. At 50 Hz, each frame is 20 ms.

The algorithm modules do not print. They return the result to `algo_task`.

## 3. Software flow

### 3.1 Startup

1. `main()` calls `soc_init()`.
2. It calls `osal_pre_start_scheduler()`.
3. It creates `task_init_app` and starts the scheduler.
4. `task_init_app` registers and initializes the board. `board_init()` registers the QEMU IMU simulator.
5. It creates an OSAL event queue, then creates `algo_task` and `imu_task`.

### 3.2 IMU initialization

1. `hal_imu_get_device(IMU_DEV_ID_0)` gets the device.
2. `hal_imu_init()`.
3. Both the accelerometer and the gyroscope are configured for 50 Hz.
4. `hal_imu_set_fifo_cfg(IMU_FIFO_ACCEL | IMU_FIFO_GYRO, true)`.
5. The FIFO is cleared, then the sensor starts in `IMU_ACCEL_GYRO` / `IMU_SEN_MODE_NORMAL`.
6. A FIFO watermark interrupt is configured, with `imu_data_ready_isr` as the callback.

The interrupt only sets `g_imu_irq_pending`. Reading samples and sending events happen in `imu_task`.

### 3.3 Read path and task communication

When `imu_task` sees the pending flag, it clears the flag and calls `hal_imu_read_gyro_accel()`, taking at most 5 frames. If `available` is greater than `IMU_READ_FRAMES`, the count is capped at 5 so the copy cannot run past the buffer. On success it updates the CRC8/SMBus tally, copies the samples into an `ImuDataBlock`, and calls `osal_send_event()`. On failure it prints the error code, clears the FIFO, and sleeps briefly. After the read, it enables the interrupt again.

The queue is created with `osal_create_event_queue(16, sizeof(OsalEventEntry))`. The event id is `ALGO_EVENT_IMU_BLOCK`.

When `algo_task` receives a block, it converts each frame to a `GestureFrame` and calls `gesture_processor_push_frame()`. A result in one of the four printed classes is printed in the common format, and the block is then released.

### 3.4 CRC

CRC8/SMBus is computed over the first IMU bytes read, up to `GESTURE_CRC_LIMIT_BYTES` (320000 bytes). When that limit is reached, the firmware prints `CRC8_SMBus = 0xXX`. The CRC only checks the input stream.

## 4. Using it with the SDK

Obtain the VeriHealthi QEMU SDK 202606 preliminary. Copy `firmware/app` and `firmware/gesture` over `galaxy_sdk/`. `firmware/app/main.h` contains only an include guard and replaces the sample `main.h` shipped with the SDK. Do not put the SDK's `bsp`, `drivers`, `os`, or `prebuilts` into this repository. Add `firmware/ble` the same way the original project added its BLE module. That module depends on the SDK header `drivers/ble/gatt.h`.

This repository does not contain the official IMU recordings, and it does not contain replay arrays exported from those recordings.

## 5. Training

Dependencies are listed in `training/requirements.txt`: `numpy`, `scikit-learn`, and `joblib`.

The dataset is supplied by the user. An example layout:

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

The first 5 lines of each txt file are a header. From line 6, every 7 int16 values are one frame: `gx, gy, gz, ax, ay, az, debug`. The default sample rate is 50 Hz. `gesture_lib/data.py` reads the txt and csv files. `dataset.py` cuts 1-second positive samples from labeled intervals and cuts `others` negative samples from intervals with no action. `features.py` extracts features.

```bash
cd training
pip install -r requirements.txt
python build_dataset.py --dataset <dataset-path>
python train_gesture_model.py
```

`build_dataset.py` writes `outputs/phase1/features_dataset.npz` by default, plus a `.json` summary with the same stem. `train_gesture_model.py` writes `outputs/phase1/gesture_mlp.joblib` and `train_report.json` by default. Those outputs are not tracked. After the trained weights are exported into `gesture_params.c`, the firmware can use the new model.

## 6. Authors

- 韦晓蜃: algorithm design and code integration. Email 2162648164wxs@gmail.com
- 杨永康: embedded software. Email coolcool1887@gmail.com
- 张立坤: BLE. Email 3574715686@qq.com
