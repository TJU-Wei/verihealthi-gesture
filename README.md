# verihealthi-gesture

[English](README.en.md)

在 VeriHealthi QEMU SDK 上采集 50 Hz IMU 六轴数据，识别四类手势：pinch（双指互点）、clench（握拳）、up（抬腕）、down（放下）。识别结果按 `1000ms, clench` 这种格式打印。

本仓库只有手势识别应用、模型和训练脚本。官方 SDK 与 IMU 数据集不在这里，需要自行准备。

设计说明见 [docs/design.md](docs/design.md)（[English](docs/design.en.md)）。

## 目录

- `firmware/app`：IMU 采集、任务通信、结果打印
- `firmware/gesture`：特征、推理、后处理和导出的模型参数
- `firmware/ble`：手势结果 GATT 服务
- `training`：离线数据集构建和 MLP 训练

## 编译

自备 VeriHealthi QEMU SDK 202606 preliminary，把 `firmware/app` 和 `firmware/gesture` 覆盖进 `galaxy_sdk/`。`firmware/app/main.h` 是本仓库的空头文件，用来替换 SDK 示例头文件；SDK 其余文件保持原样。`firmware/ble` 按原工程的 BLE 模块方式加入。

## 训练

在 `training/` 下安装依赖后，使用自备的数据集目录：

```bash
pip install -r requirements.txt
python build_dataset.py --dataset <数据集路径>
python train_gesture_model.py
```

## 作者

- 韦晓蜃：算法设计与代码整合。邮箱 2162648164wxs@gmail.com
- 杨永康：嵌入式。邮箱 coolcool1887@gmail.com
- 张立坤：BLE。邮箱 TODO

## 许可

本仓库源码使用 MIT License，见 [LICENSE](LICENSE)。VeriHealthi QEMU SDK 和比赛 IMU 数据集沿用主办方的许可，不因本仓库而改变。
