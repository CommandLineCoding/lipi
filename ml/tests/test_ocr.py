import os
import random

import pytest

torch = pytest.importorskip("torch")

from ocr.charset import Charset, build, normalise
from ocr.model import LINE_HEIGHT, STRIDE, RecNet, count_parameters
from ocr.train_rec import edit_distance


def test_normalise_drops_joiners_and_composes():
    assert normalise("র‍্য") == "র্য"
    assert normalise("  কী ") == "কী"


def test_charset_round_trip_with_conjuncts():
    cs = build(["বাংলা ভাষা", "हिन्दी भाषा", "hello 123"], min_count=1)
    for text in ["বাংলা ভাষা", "हिन्दी भाषा", "hello 123"]:
        ids = cs.encode(text)
        emitted = []
        for i in ids:  # the network puts a blank between two identical characters
            if emitted and emitted[-1] == i:
                emitted.append(0)
            emitted.append(i)
        assert cs.decode(emitted) == normalise(text)


def test_ctc_greedy_collapses_repeats_and_blanks():
    cs = Charset(["a", "b"])
    # a a blank a b b -> "aab"
    assert cs.decode([1, 1, 0, 1, 2, 2]) == "aab"


def test_unknown_characters_are_skipped():
    cs = Charset(["a"])
    assert cs.encode("aXa") == [1, 1]
    assert not cs.covers("aXa")


def test_charset_saves_and_loads(tmp_path):
    cs = Charset(["a", "ক", " "])
    p = tmp_path / "c.txt"
    cs.save(p)
    assert Charset.load(p).chars == cs.chars


def test_edit_distance():
    assert edit_distance("kitten", "sitting") == 3
    assert edit_distance("", "abc") == 3
    assert edit_distance("same", "same") == 0


def test_model_output_shape_and_size():
    model = RecNet(300).eval()
    out = model(torch.rand(2, 1, LINE_HEIGHT, 160))
    assert out.shape == (160 // STRIDE, 2, 300)
    assert torch.allclose(out.exp().sum(2), torch.ones(160 // STRIDE, 2), atol=1e-4)
    assert count_parameters(model) < 1_500_000


FONTS = os.environ.get("LIPI_FONTS")


@pytest.mark.skipif(not FONTS, reason="set LIPI_FONTS to a folder of OFL fonts to test the renderer")
def test_renderer_draws_each_script():
    from ocr.fonts import scan
    from ocr.synth_text import LineRenderer
    found = scan(FONTS)
    r = LineRenderer(found)
    rng = random.Random(1)
    for script, text in [("bengali", "বাংলা ভাষা"), ("devanagari", "हिन्दी भाषा"), ("latin", "hello world")]:
        if not found[script]:
            continue
        img = None
        for _ in range(5):
            img = r.render(text, script, rng)
            if img is not None:
                break
        assert img is not None and img.shape[0] == LINE_HEIGHT and img.dtype.name == "uint8"
