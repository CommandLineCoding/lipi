"""Quad geometry shared by the corpus tools and the scorer.

Corner order everywhere is top-left, top-right, bottom-right, bottom-left as
seen on the image, in pixel coordinates with y pointing down. This matches
docs/CORE_API.md.
"""
import numpy as np


def sort_quad(points):
    """Order four points clockwise on screen, starting at the top-left corner."""
    pts = np.asarray(points, dtype=float).reshape(4, 2)
    c = pts.mean(axis=0)
    angle = np.arctan2(pts[:, 1] - c[1], pts[:, 0] - c[0])
    pts = pts[np.argsort(angle)]
    start = int(np.argmin(pts.sum(axis=1)))
    return np.roll(pts, -start, axis=0)


def polygon_area(poly):
    """Signed area; positive for clockwise on screen (y down)."""
    p = np.asarray(poly, dtype=float)
    q = np.roll(p, -1, axis=0)
    return 0.5 * float(np.sum(p[:, 0] * q[:, 1] - q[:, 0] * p[:, 1]))


def _clip(subject, a, b):
    out = []
    ab = b - a
    for i, cur in enumerate(subject):
        prev = subject[i - 1]
        c_cur = ab[0] * (cur[1] - a[1]) - ab[1] * (cur[0] - a[0])
        c_prev = ab[0] * (prev[1] - a[1]) - ab[1] * (prev[0] - a[0])
        if c_cur >= 0:
            if c_prev < 0:
                out.append(_cross_point(prev, cur, c_prev, c_cur))
            out.append(cur)
        elif c_prev >= 0:
            out.append(_cross_point(prev, cur, c_prev, c_cur))
    return out


def _cross_point(p, q, cp, cq):
    t = cp / (cp - cq)
    return p + t * (q - p)


def quad_iou(pred, gt):
    """Intersection over union of two quads. The ground truth must be convex."""
    gt = sort_quad(gt)
    subject = [np.asarray(p, dtype=float) for p in sort_quad(pred)]
    for i in range(4):
        if not subject:
            return 0.0
        subject = _clip(subject, gt[i], gt[(i + 1) % 4])
    if len(subject) < 3:
        return 0.0
    inter = abs(polygon_area(np.array(subject)))
    union = abs(polygon_area(pred)) + abs(polygon_area(gt)) - inter
    return float(inter / union) if union > 0 else 0.0


def corner_error(pred, gt, width, height):
    """Mean corner distance divided by the image diagonal.

    Order-invariant: the smallest value over the four cyclic shifts is used.
    """
    pred = np.asarray(pred, dtype=float).reshape(4, 2)
    gt = np.asarray(gt, dtype=float).reshape(4, 2)
    diag = float(np.hypot(width, height))
    best = min(
        float(np.mean(np.linalg.norm(np.roll(pred, s, axis=0) - gt, axis=1))) for s in range(4)
    )
    return best / diag
