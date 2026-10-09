# SPDX-License-Identifier: MIT
from __future__ import annotations

import numpy as np


FEATURE_NAMES: tuple[str, ...] = ()


def _axis_features(values: np.ndarray, prefix: str, names: list[str], feats: list[float]) -> None:
    stats = {
        "mean": float(np.mean(values)),
        "std": float(np.std(values)),
        "min": float(np.min(values)),
        "max": float(np.max(values)),
        "range": float(np.max(values) - np.min(values)),
        "rms": float(np.sqrt(np.mean(values * values))),
    }
    for stat_name, value in stats.items():
        names.append(f"{prefix}_{stat_name}")
        feats.append(value)


def _summary_features(values: np.ndarray, prefix: str, names: list[str], feats: list[float]) -> None:
    stats = {
        "mean": float(np.mean(values)),
        "std": float(np.std(values)),
        "max": float(np.max(values)),
        "rms": float(np.sqrt(np.mean(values * values))),
    }
    for stat_name, value in stats.items():
        names.append(f"{prefix}_{stat_name}")
        feats.append(value)


def extract_features(window: np.ndarray, sample_rate_hz: int = 50) -> np.ndarray:
    features, _ = extract_features_with_names(window, sample_rate_hz)
    return features


def extract_features_with_names(window: np.ndarray, sample_rate_hz: int = 50) -> tuple[np.ndarray, tuple[str, ...]]:
    if window.ndim != 2 or window.shape[1] < 6:
        raise ValueError("window must have at least 6 columns")

    sensor = window[:, :6].astype(np.float32)
    gyro = sensor[:, :3]
    accel = sensor[:, 3:6]
    names: list[str] = []
    feats: list[float] = []

    for idx, axis_name in enumerate(("gx", "gy", "gz", "ax", "ay", "az")):
        _axis_features(sensor[:, idx], axis_name, names, feats)

    accel_mag = np.linalg.norm(accel, axis=1)
    gyro_mag = np.linalg.norm(gyro, axis=1)
    _summary_features(accel_mag, "acc_mag", names, feats)
    _summary_features(gyro_mag, "gyro_mag", names, feats)

    diffs = np.diff(sensor, axis=0)
    diff_mag = np.linalg.norm(diffs, axis=1)
    _summary_features(diff_mag, "jerk_mag", names, feats)

    half = max(1, len(sensor) // 2)
    first = np.mean(sensor[:half], axis=0)
    second = np.mean(sensor[half:], axis=0)
    for idx, axis_name in enumerate(("gx", "gy", "gz", "ax", "ay", "az")):
        names.append(f"{axis_name}_half_mean_delta")
        feats.append(float(second[idx] - first[idx]))

    gyro_angle_delta = np.sum(gyro, axis=0) / float(sample_rate_hz)
    for idx, axis_name in enumerate(("gx", "gy", "gz")):
        names.append(f"{axis_name}_angle_delta")
        feats.append(float(gyro_angle_delta[idx]))

    ax, ay, az = accel[:, 0], accel[:, 1], accel[:, 2]
    pitch = np.arctan2(-ax, np.sqrt(ay * ay + az * az))
    roll = np.arctan2(ay, az)
    names.extend(["pitch_delta", "roll_delta"])
    feats.extend([float(pitch[-1] - pitch[0]), float(roll[-1] - roll[0])])

    return np.array(feats, dtype=np.float32), tuple(names)


def feature_names(window_frames: int = 50) -> tuple[str, ...]:
    dummy = np.zeros((window_frames, 7), dtype=np.float32)
    _, names = extract_features_with_names(dummy)
    return names

