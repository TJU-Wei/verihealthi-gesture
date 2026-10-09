# SPDX-License-Identifier: MIT
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .data import GESTURE_LABELS, frame_at, load_all_recordings, load_recording, slice_window
from .features import extract_features


LABEL_TO_ID = {label: idx for idx, label in enumerate(GESTURE_LABELS)}
ID_TO_LABEL = {idx: label for label, idx in LABEL_TO_ID.items()}


@dataclass(frozen=True)
class SampleMeta:
    source: str
    user_id: str
    hand: str
    label: str
    center_s: float
    start_frame: int
    source_type: str


def _near_label(center_s: float, labels: tuple[tuple[float, str], ...], margin_s: float) -> bool:
    return any(abs(center_s - event_s) < margin_s for event_s, _ in labels)


def build_samples(
    dataset_root: Path,
    window_s: float = 1.0,
    positive_offsets_s: tuple[float, ...] = (-0.2, -0.1, 0.0, 0.1, 0.2),
    negative_stride_s: float = 1.0,
    negative_margin_s: float = 1.5,
    max_negative_per_file: int = 80,
    hard_negatives_path: Path | None = None,
    hard_negative_weight: float = 3.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, list[SampleMeta]]:
    recordings = load_all_recordings(dataset_root)
    x_rows: list[np.ndarray] = []
    y_rows: list[int] = []
    weights: list[float] = []
    metas: list[SampleMeta] = []

    for rec in recordings:
        window_frames = int(round(window_s * rec.sample_rate_hz))
        half = window_frames // 2

        if rec.label_dir != "others":
            for event_s, label in rec.labels:
                for offset_s in positive_offsets_s:
                    center_s = event_s + offset_s
                    center_frame = frame_at(center_s, rec.sample_rate_hz)
                    window = slice_window(rec.data, center_frame, window_frames)
                    if window is None:
                        continue
                    x_rows.append(extract_features(window, rec.sample_rate_hz))
                    y_rows.append(LABEL_TO_ID[label])
                    weights.append(1.0)
                    metas.append(
                        SampleMeta(
                            source=str(rec.path),
                            user_id=rec.user_id,
                            hand=rec.hand,
                            label=label,
                            center_s=center_s,
                            start_frame=center_frame - half,
                            source_type="positive",
                        )
                    )

            # Add background negatives from non-gesture regions in positive files.
            added = 0
            stride_frames = max(1, int(round(negative_stride_s * rec.sample_rate_hz)))
            for start in range(0, max(0, len(rec.data) - window_frames + 1), stride_frames):
                center_frame = start + half
                center_s = center_frame / rec.sample_rate_hz
                if _near_label(center_s, rec.labels, negative_margin_s):
                    continue
                window = rec.data[start : start + window_frames]
                x_rows.append(extract_features(window, rec.sample_rate_hz))
                y_rows.append(LABEL_TO_ID["others"])
                weights.append(1.0)
                metas.append(
                    SampleMeta(str(rec.path), rec.user_id, rec.hand, "others", center_s, start, "background")
                )
                added += 1
                if added >= max_negative_per_file:
                    break
        else:
            added = 0
            stride_frames = max(1, int(round(negative_stride_s * rec.sample_rate_hz)))
            for start in range(0, max(0, len(rec.data) - window_frames + 1), stride_frames):
                center_frame = start + half
                center_s = center_frame / rec.sample_rate_hz
                window = rec.data[start : start + window_frames]
                x_rows.append(extract_features(window, rec.sample_rate_hz))
                y_rows.append(LABEL_TO_ID["others"])
                weights.append(1.0)
                metas.append(
                    SampleMeta(str(rec.path), rec.user_id, rec.hand, "others", center_s, start, "background")
                )
                added += 1
                if added >= max_negative_per_file:
                    break

    if hard_negatives_path is not None and hard_negatives_path.exists():
        hard_data = json.loads(hard_negatives_path.read_text(encoding="utf-8"))
        for row in hard_data.get("hard_negatives", []):
            source = Path(row["source"])
            rec = load_recording(source, source.parent.name)
            window_frames = int(round(window_s * rec.sample_rate_hz))
            center_s = float(row["time_s"])
            center_frame = frame_at(center_s, rec.sample_rate_hz)
            window = slice_window(rec.data, center_frame, window_frames)
            if window is None:
                continue
            x_rows.append(extract_features(window, rec.sample_rate_hz))
            y_rows.append(LABEL_TO_ID["others"])
            weights.append(hard_negative_weight)
            metas.append(
                SampleMeta(
                    source=str(rec.path),
                    user_id=rec.user_id,
                    hand=rec.hand,
                    label="others",
                    center_s=center_s,
                    start_frame=center_frame - window_frames // 2,
                    source_type=f"hard_negative:{row.get('pred_label', 'unknown')}",
                )
            )

    if not x_rows:
        raise ValueError("no samples built")
    return np.vstack(x_rows), np.array(y_rows, dtype=np.int64), np.array(weights, dtype=np.float32), metas


def meta_to_array(metas: list[SampleMeta]) -> np.ndarray:
    return np.array(
        [
            (m.source, m.user_id, m.hand, m.label, m.center_s, m.start_frame, m.source_type)
            for m in metas
        ],
        dtype=object,
    )


def group_ids_from_meta(meta: np.ndarray) -> np.ndarray:
    return meta[:, 1].astype(str)


def labels_from_ids(y: np.ndarray) -> list[str]:
    return [ID_TO_LABEL[int(v)] for v in y]
