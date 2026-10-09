# SPDX-License-Identifier: MIT
from __future__ import annotations

import argparse
import json
from pathlib import Path

import joblib
import numpy as np
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import GroupShuffleSplit
from sklearn.neural_network import MLPClassifier
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler

from gesture_lib.dataset import LABEL_TO_ID, group_ids_from_meta


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--features", type=Path, default=Path("outputs/phase1/features_dataset.npz"))
    parser.add_argument("--model-out", type=Path, default=Path("outputs/phase1/gesture_mlp.joblib"))
    parser.add_argument("--report-out", type=Path, default=Path("outputs/phase1/train_report.json"))
    parser.add_argument("--random-state", type=int, default=42)
    args = parser.parse_args()

    data = np.load(args.features, allow_pickle=True)
    x = data["x"]
    y = data["y"]
    sample_weight = data["sample_weight"] if "sample_weight" in data.files else np.ones(len(y), dtype=np.float32)
    meta = data["meta"]
    groups = group_ids_from_meta(meta)

    splitter = GroupShuffleSplit(n_splits=1, test_size=0.25, random_state=args.random_state)
    train_idx, test_idx = next(splitter.split(x, y, groups))

    y_train = y[train_idx]
    y_test = y[test_idx]

    model = Pipeline(
        steps=[
            ("scaler", StandardScaler()),
            (
                "mlp",
                MLPClassifier(
                    hidden_layer_sizes=(32,),
                    activation="relu",
                    solver="adam",
                    alpha=1e-4,
                    max_iter=500,
                    random_state=args.random_state,
                    early_stopping=True,
                    n_iter_no_change=25,
                ),
            ),
        ]
    )
    model.fit(x[train_idx], y_train, mlp__sample_weight=sample_weight[train_idx])
    pred = model.predict(x[test_idx])
    labels = ["pinch", "clench", "up", "down", "others"]
    label_ids = [LABEL_TO_ID[label] for label in labels]
    report = {
        "train_samples": int(len(train_idx)),
        "test_samples": int(len(test_idx)),
        "train_groups": sorted(set(groups[train_idx].tolist())),
        "test_groups": sorted(set(groups[test_idx].tolist())),
        "classification_report": classification_report(
            y_test,
            pred,
            labels=label_ids,
            target_names=labels,
            output_dict=True,
            zero_division=0,
        ),
        "confusion_matrix_labels": labels,
        "confusion_matrix": confusion_matrix(y_test, pred, labels=label_ids).tolist(),
        "feature_names": data["feature_names"].tolist(),
    }

    args.model_out.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, args.model_out)
    args.report_out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("train_samples", "test_samples", "train_groups", "test_groups")}, indent=2))
    print(f"macro_f1={report['classification_report']['macro avg']['f1-score']:.4f}")
    print(f"wrote {args.model_out}")
    print(f"wrote {args.report_out}")


if __name__ == "__main__":
    main()
