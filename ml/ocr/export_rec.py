"""Export a trained recogniser to ONNX and ncnn, and check that both give the same answers as the PyTorch model.

    python -m ocr.export_rec --checkpoint runs/rec-s/best.pt --out models/text-rec-small/text-rec-small

Writes <out>.onnx, <out>.ncnn.param, <out>.ncnn.bin and <out>.charset.txt. The graph takes a 1 x 1 x 48 x W float
image (values 0 to 1, W a multiple of 4) on "in0" and returns the log-probabilities, one row per 4 pixels of width and
one column per character of the charset (the first is the CTC blank). ncnn conversion needs `pip install pnnx`;
the ncnn check needs `pip install ncnn`.
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import tempfile

import numpy as np
import torch

from .model import LINE_HEIGHT, STRIDE, RecNet


def find_pnnx():
    found = shutil.which("pnnx")
    if found:
        return found
    import pnnx
    for path in glob.glob(os.path.join(os.path.dirname(pnnx.__file__), "pnnx*")):
        if os.path.isfile(path):
            os.chmod(path, 0o755)
            return path
    raise SystemExit("pnnx not found; pip install pnnx")


def check_onnx(path, model, widths):
    import onnxruntime as ort
    session = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    worst = 0.0
    for w in widths:
        x = torch.rand(1, 1, LINE_HEIGHT, w)
        with torch.no_grad():
            ref = model(x).numpy()
        got = session.run(None, {"in0": x.numpy()})[0]
        worst = max(worst, float(np.abs(ref - got).max()))
    return worst


def check_ncnn(param, bin_, model, widths):
    import ncnn
    net = ncnn.Net()
    net.load_param(param)
    net.load_model(bin_)
    worst = 0.0
    for w in widths:
        x = torch.rand(1, 1, LINE_HEIGHT, w)
        with torch.no_grad():
            ref = model(x).numpy()[:, 0, :]  # T, classes
        ex = net.create_extractor()
        ex.input("in0", ncnn.Mat(x.numpy()[0]).clone())
        _, out = ex.extract("out0")
        got = np.array(out)
        worst = max(worst, float(np.abs(ref - got.reshape(ref.shape)).max()))
    return worst


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--out", required=True, help="output prefix")
    ap.add_argument("--skip-ncnn", action="store_true")
    args = ap.parse_args()
    ck = torch.load(args.checkpoint, map_location="cpu")
    model = RecNet(len(ck["charset"]) + 1, **ck.get("config", {}))
    model.load_state_dict(ck["model"])
    model.eval()
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    widths = [64, 160, 320, 640]

    onnx_path = args.out + ".onnx"
    torch.onnx.export(model, torch.rand(1, 1, LINE_HEIGHT, 160), onnx_path, input_names=["in0"], output_names=["out0"],
                      dynamic_axes={"in0": {3: "width"}, "out0": {0: "steps"}}, opset_version=17, dynamo=False)
    print("onnx max difference to PyTorch over widths", widths, "=", check_onnx(onnx_path, model, widths))

    with open(args.out + ".charset.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(ck["charset"]))

    if not args.skip_ncnn:
        with tempfile.TemporaryDirectory() as tmp:
            pt = os.path.join(tmp, "rec.pt")
            torch.jit.trace(model, torch.rand(1, 1, LINE_HEIGHT, 160)).save(pt)
            subprocess.run([find_pnnx(), pt, f"inputshape=[1,1,{LINE_HEIGHT},160]", f"inputshape2=[1,1,{LINE_HEIGHT},320]"], check=True, cwd=tmp,
                           stdout=subprocess.DEVNULL)
            for ext in ("param", "bin"):
                shutil.copy(os.path.join(tmp, f"rec.ncnn.{ext}"), f"{args.out}.ncnn.{ext}")
        try:
            print("ncnn max difference to PyTorch over widths", widths, "=", check_ncnn(args.out + ".ncnn.param", args.out + ".ncnn.bin", model, widths))
        except ImportError:
            print("pip install ncnn to check the ncnn files")
    for suffix in (".onnx", ".ncnn.param", ".ncnn.bin", ".charset.txt"):
        path = args.out + suffix
        if os.path.exists(path):
            with open(path, "rb") as f:
                print(hashlib.sha256(f.read()).hexdigest(), os.path.basename(path), os.path.getsize(path))


if __name__ == "__main__":
    main()
