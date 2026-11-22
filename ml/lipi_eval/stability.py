"""Frame-to-frame stability of a quad track, for live detection.

A track is a list of quads, one per frame, as produced by running a detector
(optionally followed by the core's smoother) over a video or a rendered sequence.
"""
import numpy as np


def jitter(track, width, height):
    """Mean frame-to-frame movement of the corners, as a fraction of the image diagonal.

    Lower is steadier. Frames without a detection (None) are skipped and break the track.
    """
    diag = float(np.hypot(width, height))
    steps = []
    prev = None
    for q in track:
        if q is None:
            prev = None
            continue
        q = np.asarray(q, dtype=float).reshape(4, 2)
        if prev is not None:
            steps.append(float(np.linalg.norm(q - prev, axis=1).max()) / diag)
        prev = q
    return float(np.mean(steps)) if steps else None


def lag(track, truth, width, height):
    """Mean distance of the corners from the true quad, as a fraction of the diagonal."""
    diag = float(np.hypot(width, height))
    errs = [float(np.linalg.norm(np.asarray(q, dtype=float).reshape(4, 2) - np.asarray(t, dtype=float).reshape(4, 2), axis=1).mean()) / diag
            for q, t in zip(track, truth) if q is not None]
    return float(np.mean(errs)) if errs else None
