"""Does the ncnn model read the same text as the PyTorch model, and how well does each read single words?

    python -m ocr.check_ncnn --checkpoint runs/rec-s/best.pt --prefix models/text-rec-small/text-rec-small --root ~/lipi-train/ocr

Decodes real Mozhi test words one by one (the way the app will, never in a padded batch) through both models and reports the character error rate of each and how often
they give the same string. A small difference in the raw scores (16-bit weights) does not matter if the text is the same.
"""
import argparse

import numpy as np
import torch

import ncnn  # after torch: both bundle an OpenMP runtime, and loading ncnn first can hang

from . import charset as cs
from .charset import edit_distance
from .data import MozhiWords
from .model import STRIDE, RecNet


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--prefix", required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--words", type=int, default=1000, help="words per language")
    ap.add_argument("--pad", type=int, default=0, help="pixels of background added after each word (0 reads the word alone, as a phone does)")
    args = ap.parse_args()
    torch.set_num_threads(1)  # many tiny inferences: more threads only fight each other
    ck = torch.load(args.checkpoint, map_location="cpu")
    charset = cs.Charset(ck["charset"])
    model = RecNet(len(charset), **ck.get("config", {}))
    model.load_state_dict(ck["model"])
    model.eval()
    net = ncnn.Net()
    net.opt.num_threads = 1
    net.load_param(args.prefix + ".ncnn.param")
    net.load_model(args.prefix + ".ncnn.bin")
    result = {}
    for lang in ("bengali", "hindi"):
        ds = MozhiWords(args.root, lang, "test")
        step = max(1, len(ds) // args.words)
        err_t = err_n = chars = same = n = 0
        for i in range(0, len(ds), step):
            img, text = ds.get(i)
            w = -(-(img.shape[1] + args.pad) // STRIDE) * STRIDE
            x = np.full((1, 1, 48, w), float(np.median(np.concatenate([img[:, 0], img[:, -1]]))) / 255.0, dtype=np.float32)
            x[0, 0, :, :img.shape[1]] = img / 255.0
            with torch.no_grad():
                p_t = charset.decode(model(torch.from_numpy(x)).argmax(2)[:, 0].tolist())
            ex = net.create_extractor()
            ex.input("in0", ncnn.Mat(x[0]).clone())
            _, out = ex.extract("out0")
            p_n = charset.decode(np.array(out).reshape(-1, len(charset)).argmax(1).tolist())
            err_t += edit_distance(p_t, text)
            err_n += edit_distance(p_n, text)
            chars += len(text)
            same += p_t == p_n
            n += 1
        result[lang] = {"words": n, "cer_pytorch": err_t / chars, "cer_ncnn": err_n / chars, "same_text": same / n}
        print(lang, result[lang], flush=True)
    return result


if __name__ == "__main__":
    main()
