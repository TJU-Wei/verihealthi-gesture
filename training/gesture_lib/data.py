# SPDX-License-Identifier: MIT
from __future__ import annotations

import csv
import re
from dataclasses import dataclass
from pathlib import Path

import numpy as np


SAMPLE_RATE_HZ = 50
FRAME_VALUES = 7
SENSOR_COLUMNS = ("gx", "gy", "gz", "ax", "ay", "az", "debug")
GESTURE_LABELS = ("pinch", "clench", "up", "down", "others")
VALID_OUTPUT_LABELS = ("pinch", "clench", "up", "down")


@dataclass(frozen=True)
class ImuRecording:
    path: Path
    label_dir: str
    user_id: str
    hand: str
    data: np.ndarray
    sample_rate_hz: int
    labels: tuple[tuple[float, str], ...]

    @property
    def duration_s(self) -> float:
        return float(len(self.data)) / float(self.sample_rate_hz)


def parse_user_id(path: Path) -> str:
    match = re.search(r"_ID(\d+)", path.stem)
    return f"ID{match.group(1)}" if match else "ID_UNKNOWN"


def parse_hand(path: Path) -> str:
    parts = path.stem.split("_")
    for part in parts:
        if part in ("left", "right"):
            return part
    return "unknown"


def label_path_for_txt(txt_path: Path) -> Path:
    name = txt_path.stem
    if name.startswith("IMU_"):
        name = name[4:]
    return txt_path.with_name(f"{name}.csv")


def read_imu_txt(path: Path) -> tuple[dict[str, str], np.ndarray]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 6:
        raise ValueError(f"{path} is too short to contain IMU data")

    header: dict[str, str] = {}
    for line in lines[:5]:
        if "=" in line:
            key, value = line.split("=", 1)
            header[key.strip()] = value.strip()

    values = np.array([int(x.strip()) for x in lines[5:] if x.strip()], dtype=np.float32)
    usable = (len(values) // FRAME_VALUES) * FRAME_VALUES
    if usable != len(values):
        values = values[:usable]
    data = values.reshape((-1, FRAME_VALUES))
    return header, data


def read_labels(path: Path) -> tuple[tuple[float, str], ...]:
    if not path.exists():
        return ()
    rows: list[tuple[float, str]] = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append((float(row["event_time_s"]), row["label"].strip()))
    return tuple(rows)


def load_recording(txt_path: Path, label_dir: str | None = None) -> ImuRecording:
    header, data = read_imu_txt(txt_path)
    sample_rate = int(header.get("sample_rate", SAMPLE_RATE_HZ))
    labels = read_labels(label_path_for_txt(txt_path))
    return ImuRecording(
        path=txt_path,
        label_dir=label_dir or txt_path.parent.name,
        user_id=parse_user_id(txt_path),
        hand=parse_hand(txt_path),
        data=data,
        sample_rate_hz=sample_rate,
        labels=labels,
    )


def iter_recording_paths(dataset_root: Path) -> list[Path]:
    paths: list[Path] = []
    for label in GESTURE_LABELS:
        label_dir = dataset_root / label
        if label_dir.exists():
            paths.extend(sorted(label_dir.glob("*.txt")))
    return paths


def load_all_recordings(dataset_root: Path) -> list[ImuRecording]:
    return [load_recording(path, path.parent.name) for path in iter_recording_paths(dataset_root)]


def _canonical_recording_key(path: Path) -> str:
    name = path.stem
    if name.startswith("IMU_"):
        name = name[4:]
    return name


def load_unique_recordings(dataset_root: Path) -> list[ImuRecording]:
    """Load recordings once, merging labels for duplicated up/down txt files."""
    grouped: dict[str, list[Path]] = {}
    for path in iter_recording_paths(dataset_root):
        grouped.setdefault(_canonical_recording_key(path), []).append(path)

    records: list[ImuRecording] = []
    for _, paths in sorted(grouped.items()):
        first = sorted(paths)[0]
        base = load_recording(first, first.parent.name)
        merged_labels: list[tuple[float, str]] = []
        label_dirs: list[str] = []
        for path in sorted(paths):
            rec = load_recording(path, path.parent.name)
            label_dirs.append(rec.label_dir)
            merged_labels.extend(rec.labels)
        merged_labels = sorted(set(merged_labels), key=lambda x: (x[0], x[1]))
        records.append(
            ImuRecording(
                path=base.path,
                label_dir="+".join(sorted(set(label_dirs))),
                user_id=base.user_id,
                hand=base.hand,
                data=base.data,
                sample_rate_hz=base.sample_rate_hz,
                labels=tuple(merged_labels),
            )
        )
    return records


def frame_at(time_s: float, sample_rate_hz: int = SAMPLE_RATE_HZ) -> int:
    return int(round(time_s * sample_rate_hz))


def slice_window(data: np.ndarray, center_frame: int, window_frames: int) -> np.ndarray | None:
    start = center_frame - window_frames // 2
    end = start + window_frames
    if start < 0 or end > len(data):
        return None
    return data[start:end]
