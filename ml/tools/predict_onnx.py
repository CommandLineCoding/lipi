"""Run an exported PageNet on a corpus and write predictions for lipi_eval.score.

    python tools/predict_onnx.py --model models/page-net.onnx --corpus data/real/test.jsonl --out preds.jsonl

Images are resized to 256 x 256 (area interpolation), exactly like training.
Latency is the time of the model call only, on this machine's CPU.
"""
import argparse
import os
import sys
import time

import cv2
import numpy as np
import onnxruntime as ort

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import read_jsonl, write_jsonl  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--model", required=True)
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--threads", type=int, default=1)
    args = ap.parse_args()
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = args.threads
    sess = ort.InferenceSession(args.model, opts, providers=["CPUExecutionProvider"])
    root = os.path.dirname(os.path.abspath(args.corpus))
    preds = []
    for row in read_jsonl(args.corpus):
        img = cv2.cvtColor(cv2.imread(os.path.join(root, row["image"])), cv2.COLOR_BGR2RGB)
        h, w = img.shape[:2]
        x = cv2.resize(img, (256, 256), interpolation=cv2.INTER_AREA).transpose(2, 0, 1)[None].astype(np.float32)
        t0 = time.perf_counter()
        corners, conf, orient = sess.run(None, {"image": x})
        ms = (time.perf_counter() - t0) * 1000
        quad = (corners[0] * np.array([w, h], dtype=np.float32)).round(2).tolist()
        preds.append({"id": row["id"], "quad": quad, "orientation": int(orient[0].argmax()),
                      "ms": round(ms, 3), "confidence": float(conf[0].min())})
    write_jsonl(args.out, preds)
    print(f"wrote {len(preds)} predictions to {args.out}")


if __name__ == "__main__":
    main()
