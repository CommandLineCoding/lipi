import numpy as np
import pytest

from lipi_eval.quad import corner_error, polygon_area, quad_iou, sort_quad
from lipi_eval.score import score

SQUARE = [[10, 10], [110, 10], [110, 110], [10, 110]]


def test_sort_quad_puts_top_left_first_and_goes_clockwise():
    shuffled = [SQUARE[2], SQUARE[0], SQUARE[3], SQUARE[1]]
    assert np.allclose(sort_quad(shuffled), SQUARE)
    assert polygon_area(sort_quad(shuffled)) > 0


def test_sort_quad_of_a_tilted_page():
    quad = [[100, 40], [60, 100], [0, 60], [40, 0]]
    out = sort_quad(quad)
    assert polygon_area(out) > 0
    assert np.allclose(out[0], [40, 0])  # smallest x + y


def test_iou_identical_disjoint_and_half():
    assert quad_iou(SQUARE, SQUARE) == pytest.approx(1.0)
    far = [[500, 500], [600, 500], [600, 600], [500, 600]]
    assert quad_iou(far, SQUARE) == 0.0
    half = [[60, 10], [160, 10], [160, 110], [60, 110]]  # shifted by half the width
    assert quad_iou(half, SQUARE) == pytest.approx(50 * 100 / (2 * 100 * 100 - 50 * 100))


def test_iou_ignores_corner_order_of_the_prediction():
    assert quad_iou([SQUARE[1], SQUARE[2], SQUARE[3], SQUARE[0]], SQUARE) == pytest.approx(1.0)


def test_corner_error_is_normalised_and_cyclic():
    shifted = [[p[0] + 3, p[1] + 4] for p in SQUARE]  # every corner off by 5 px
    diag = np.hypot(200, 100)
    assert corner_error(shifted, SQUARE, 200, 100) == pytest.approx(5 / diag)
    rolled = SQUARE[1:] + SQUARE[:1]
    assert corner_error(rolled, SQUARE, 200, 100) == pytest.approx(0.0)


def _row(i, source="synthetic"):
    return {"id": i, "image": f"{i}.jpg", "width": 200, "height": 200, "quad": SQUARE,
            "orientation": 1, "source": source, "license": "x", "split": "test"}


def test_score_counts_missing_predictions_as_failures():
    corpus = [_row("a"), _row("b"), _row("c", source="real")]
    preds = [{"id": "a", "quad": SQUARE, "orientation": 1, "ms": 2.0},
             {"id": "b", "quad": None}]
    r = score(corpus, preds)
    assert r["all"]["images"] == 3
    assert r["all"]["failures"] == 2
    assert r["sources"]["synthetic"]["iou"]["mean"] == pytest.approx(0.5)
    assert r["sources"]["real"]["failures"] == 1
    assert r["all"]["orientation_accuracy"] == 1.0
    assert r["all"]["latency_ms"]["mean"] == 2.0


def test_report_matches_the_committed_fixture():
    import json
    import os

    from lipi_eval.corpus import read_jsonl

    fx = os.path.join(os.path.dirname(__file__), "fixtures")
    got = score(read_jsonl(os.path.join(fx, "corpus.jsonl")), read_jsonl(os.path.join(fx, "preds.jsonl")))
    with open(os.path.join(fx, "expected_report.json")) as f:
        want = json.load(f)
    assert json.loads(json.dumps(got)) == want


def test_stability_metrics():
    from lipi_eval.stability import jitter, lag

    still = [SQUARE] * 5
    assert jitter(still, 200, 100) == 0.0
    moving = [[[x + 5 * i, y] for x, y in SQUARE] for i in range(4)]
    assert jitter(moving, 200, 100) == pytest.approx(5 / np.hypot(200, 100))
    assert jitter([SQUARE, None, SQUARE], 200, 100) is None  # a gap breaks the track
    assert jitter([], 200, 100) is None
    assert lag([SQUARE], [SQUARE], 200, 100) == 0.0
    shifted = [[x + 3, y + 4] for x, y in SQUARE]
    assert lag([shifted], [SQUARE], 200, 100) == pytest.approx(5 / np.hypot(200, 100))


def test_report_has_a_section_per_document_kind_when_rows_have_one():
    rows = [dict(_row("a"), kind="receipt"), dict(_row("b"), kind="receipt"), dict(_row("c"), kind="notes")]
    preds = [{"id": "a", "quad": SQUARE, "orientation": 1}, {"id": "b", "quad": SQUARE, "orientation": 0},
             {"id": "c", "quad": SQUARE, "orientation": 1}]
    r = score(rows, preds)
    assert set(r["kinds"]) == {"synthetic/receipt", "synthetic/notes"}
    assert r["kinds"]["synthetic/receipt"]["orientation_accuracy"] == 0.5
    assert r["kinds"]["synthetic/notes"]["orientation_accuracy"] == 1.0
    assert "kinds" not in score([_row("a")], [{"id": "a", "quad": SQUARE}])  # no kinds without a "kind" field
