"""Export a trained checkpoint to ONNX.

    python -m train.export_onnx --checkpoint runs/page-net/best.pt --out page-net.onnx

The graph takes a float32 image of shape 1 x 3 x 256 x 256, RGB, values 0..255.
"""
import argparse
import hashlib
import inspect

import torch

from .model import INPUT_SIZE, PageNet


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    ck = torch.load(args.checkpoint, map_location="cpu")
    model = PageNet(ck.get("width", 1.0))
    model.load_state_dict(ck["model"])
    model.eval()
    dummy = torch.rand(1, 3, INPUT_SIZE, INPUT_SIZE) * 255
    extra = {"dynamo": False} if "dynamo" in inspect.signature(torch.onnx.export).parameters else {}  # classic exporter
    torch.onnx.export(model, dummy, args.out, input_names=["image"], output_names=["corners", "confidence", "orientation"],
                      opset_version=17, do_constant_folding=True, **extra)
    with open(args.out, "rb") as f:
        print(args.out, hashlib.sha256(f.read()).hexdigest())


if __name__ == "__main__":
    main()
