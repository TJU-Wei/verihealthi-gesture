# verihealthi-gesture

[中文](README.md)

This project reads 50 Hz six-axis IMU data on the VeriHealthi QEMU SDK and recognizes four gestures: pinch (two fingers tap each other), clench (make a fist), up (raise the wrist), and down (lower the wrist). Results are printed as `1000ms, clench`.

This repository contains only the gesture application, the model, and the training scripts. The official SDK and IMU dataset are not included and must be obtained separately.

See [docs/design.en.md](docs/design.en.md) for the design notes.

## Layout

- `firmware/app`: IMU capture, task communication, and result printing
- `firmware/gesture`: features, inference, post-processing, and exported model parameters
- `firmware/ble`: GATT service for gesture results
- `training`: offline dataset building and MLP training

## Build

Obtain the VeriHealthi QEMU SDK 202606 preliminary, then copy `firmware/app` and `firmware/gesture` over `galaxy_sdk/`. `firmware/app/main.h` is an empty header from this repository and replaces the SDK sample header. Leave the rest of the SDK unchanged. Add `firmware/ble` the same way the original project added its BLE module.

## Training

Install the dependencies under `training/`, then point the scripts at your own dataset directory:

```bash
pip install -r requirements.txt
python build_dataset.py --dataset <dataset-path>
python train_gesture_model.py
```

## Authors

- 韦晓蜃: algorithm design and code integration. Email 2162648164wxs@gmail.com
- 杨永康: embedded software. Email coolcool1887@gmail.com
- 张立坤: BLE. Email TODO

## License

Source code in this repository is under the MIT License. See [LICENSE](LICENSE). The VeriHealthi QEMU SDK and the contest IMU dataset keep the organizer's license. This repository does not change that.
