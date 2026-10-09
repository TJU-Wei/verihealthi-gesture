# SPDX-License-Identifier: MIT
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

import numpy as np

from gesture_lib.dataset import build_samples, labels_from_ids, meta_to_array
from gesture_lib.features import feature_names


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", type=Path, default=Path("VeriHealthi_IMU_Dataset"))
    parser.add_argument("--out", type=Path, default=Path("outputs/phase1/features_dataset.npz"))
    parser.add_argument("--window-s", type=float, default=1.0)
    parser.add_argument("--hard-negatives", type=Path, default=None)
    parser.add_argument("--hard-negative-weight", type=float, default=3.0)
    args = parser.parse_args()

    hard_negatives = args.hard_negatives if args.hard_negatives and args.hard_negatives.exists() else None
    x, y, sample_weight, metas = build_samples(
        args.dataset,
        window_s=args.window_s,
        hard_negatives_path=hard_negatives,
        hard_negative_weight=args.hard_negative_weight,
    )
    meta = meta_to_array(metas)
    names = np.array(feature_names(int(round(args.window_s * 50))), dtype=object)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.out, x=x, y=y, sample_weight=sample_weight, meta=meta, feature_names=names)

    counts = Counter(labels_from_ids(y))
    source_counts = Counter(str(row[-1]) for row in meta)
    report = {
        "sample_count": int(len(y)),
        "feature_count": int(x.shape[1]),
        "class_counts": dict(sorted(counts.items())),
        "source_counts": dict(sorted(source_counts.items())),
        "hard_negatives": str(hard_negatives) if hard_negatives else None,
        "hard_negative_weight": args.hard_negative_weight if hard_negatives else None,
        "window_s": args.window_s,
    }
    report_path = args.out.with_suffix(".json")
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
