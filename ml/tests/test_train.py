import json
import os

import numpy as np
import pytest

torch = pytest.importorskip("torch")
cv2 = pytest.importorskip("cv2")

from train.data import PageDataset, page_order, rotate_cw, sorted_and_orientation  # noqa: E402
from train.model import INPUT_SIZE, PageNet, count_parameters  # noqa: E402
from train.train import gaussian_targets  # noqa: E402


def test_orientation_follows_image_rotation():
    page = np.array([[40, 20], [160, 30], [150, 90], [50, 80]], dtype=np.float32)  # c0..c3, clockwise
    img = np.zeros((100, 200, 3), dtype=np.uint8)
    s0, k0 = sorted_and_orientation(page)
    assert k0 == 0
    for r in range(1, 5):
        rimg, rq = rotate_cw(img, page, r)
        s, k = sorted_and_orientation(rq)
        assert k == (k0 - r) % 4
        assert rq.min() >= 0 and rq[:, 0].max() <= rimg.shape[1] and rq[:, 1].max() <= rimg.shape[0]
    # a full turn is the identity
    _, q4 = rotate_cw(img, page, 4)
    assert np.allclose(q4, page)


def test_page_order_inverts_the_label():
    sorted_quad = np.array([[10, 10], [90, 12], [88, 70], [12, 68]], dtype=np.float32)
    for k in range(4):
        page = page_order(sorted_quad, k)
        s, kk = sorted_and_orientation(page)
        assert kk == k
        assert np.allclose(s, sorted_quad)


def test_model_shapes_and_size():
    model = PageNet().eval()
    x = torch.rand(2, 3, INPUT_SIZE, INPUT_SIZE) * 255
    corners, conf, orient = model(x)
    assert corners.shape == (2, 4, 2) and conf.shape == (2, 4) and orient.shape == (2, 4)
    assert float(corners.min()) >= 0 and float(corners.max()) <= 1
    assert count_parameters(model) < 1_500_000
    *_, heat, mask = model(x, train_outputs=True)
    assert heat.shape == (2, 4, 64, 64) and mask.shape == (2, 1, 64, 64)


def test_gaussian_targets_are_distributions_centred_on_the_corner():
    corners = torch.tensor([[[0.25, 0.5], [0.75, 0.5], [0.75, 0.9], [0.25, 0.9]]])
    t = gaussian_targets(corners)
    assert torch.allclose(t.sum(dim=(2, 3)), torch.ones(1, 4), atol=1e-4)
    peak = t[0, 0].argmax().item()
    assert abs(peak % 64 - 16) <= 1 and abs(peak // 64 - 32) <= 1


def test_dataset_items(tmp_path):
    rng = np.random.default_rng(0)
    img = (rng.random((384, 512, 3)) * 80).astype(np.uint8)
    quad = np.array([[100, 80], [420, 90], [410, 300], [110, 290]], dtype=np.int32)
    cv2.fillConvexPoly(img, quad, (220, 220, 220))
    os.makedirs(tmp_path / "images")
    cv2.imwrite(str(tmp_path / "images" / "a.jpg"), img)
    row = {"id": "a", "image": "images/a.jpg", "width": 512, "height": 384, "quad": quad.tolist(),
           "orientation": 0, "source": "synthetic", "license": "x", "split": "train"}
    jl = tmp_path / "train.jsonl"
    jl.write_text(json.dumps(row) + "\n")
    for train in (True, False):
        ds = PageDataset([str(jl)], train=train)
        for _ in range(8):
            item = ds[0]
            assert item["image"].shape == (3, INPUT_SIZE, INPUT_SIZE) and item["image"].dtype == torch.uint8
            c = item["corners"]
            assert float(c.min()) >= -0.01 and float(c.max()) <= 1.01
            assert item["mask"].shape == (1, 64, 64) and 0.1 < float(item["mask"].mean()) < 0.9
            assert int(item["orientation"]) in (0, 1, 2, 3)
    # real photos have no orientation label
    row["orientation"] = None
    jl.write_text(json.dumps(row) + "\n")
    assert int(PageDataset([str(jl)], train=False)[0]["orientation"]) == -1
