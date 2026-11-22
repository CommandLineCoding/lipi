"""Convert a trained checkpoint to ncnn (a text .param and a .bin).

    python -m train.export_ncnn --checkpoint runs/page-net/best.pt --out models/page-net

This traces the model to TorchScript and runs pnnx (pip install pnnx), which
writes <out>.ncnn.param and <out>.ncnn.bin. The graph takes a 1 x 3 x 256 x 256
float image with values 0..255 on blob "in0" and produces "out0" (corners),
"out1" (corner confidence) and "out2" (orientation logits).
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import tempfile

import torch

from .model import INPUT_SIZE, PageNet


def find_pnnx(explicit):
    if explicit:
        return explicit
    found = shutil.which("pnnx")
    if found:
        return found
    import pnnx  # the pip package ships the binary next to its python files

    for path in glob.glob(os.path.join(os.path.dirname(pnnx.__file__), "pnnx*")):
        if os.path.isfile(path):
            os.chmod(path, 0o755)
            return path
    raise SystemExit("pnnx not found; pip install pnnx or pass --pnnx")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--out", required=True, help="output prefix, e.g. models/page-net")
    ap.add_argument("--pnnx")
    args = ap.parse_args()
    ck = torch.load(args.checkpoint, map_location="cpu")
    model = PageNet(ck.get("width", 1.0))
    model.load_state_dict(ck["model"])
    model.eval()
    with tempfile.TemporaryDirectory() as tmp:
        pt = os.path.join(tmp, "pagenet.pt")
        torch.jit.trace(model, torch.rand(1, 3, INPUT_SIZE, INPUT_SIZE) * 255).save(pt)
        subprocess.run([find_pnnx(args.pnnx), pt, f"inputshape=[1,3,{INPUT_SIZE},{INPUT_SIZE}]"], check=True, cwd=tmp,
                       stdout=subprocess.DEVNULL)
        for ext in ("param", "bin"):
            dst = f"{args.out}.ncnn.{ext}"
            os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
            shutil.copy(os.path.join(tmp, f"pagenet.ncnn.{ext}"), dst)
    for ext in ("param", "bin"):
        path = f"{args.out}.ncnn.{ext}"
        with open(path, "rb") as f:
            print(path, os.path.getsize(path), "bytes", hashlib.sha256(f.read()).hexdigest())


if __name__ == "__main__":
    main()
