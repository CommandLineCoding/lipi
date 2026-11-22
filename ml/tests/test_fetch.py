import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import fetch_mendeley as fm  # noqa: E402


def test_orientation_6_maps_stored_pixels_to_the_rotated_image():
    # 400x200 stored image rotated 90 degrees clockwise becomes 200x400.
    assert fm.to_display_space([(0, 0)], 6, 400, 200) == [(200, 0)]
    assert fm.to_display_space([(400, 0)], 6, 400, 200) == [(200, 400)]


def test_orientation_8_and_3():
    assert fm.to_display_space([(0, 0)], 8, 400, 200) == [(0, 400)]
    assert fm.to_display_space([(10, 20)], 3, 400, 200) == [(390, 180)]


def test_labels_in_stored_space_are_converted_when_only_they_fit():
    pts = [(300, 20), (380, 20), (380, 150), (300, 150)]  # fits 400x200, not 200x400
    mapped, how = fm.corners_in_display_space(pts, 6, 400, 200)
    assert how == "stored" and mapped is not None
    assert all(0 <= x <= 200 and 0 <= y <= 400 for x, y in mapped)


def test_labels_already_in_display_space_are_kept():
    pts = [(20, 300), (180, 300), (180, 390), (20, 390)]  # fits 200x400, not 400x200
    mapped, how = fm.corners_in_display_space(pts, 6, 400, 200)
    assert how == "display" and mapped == pts


def test_labels_outside_both_are_skipped():
    mapped, how = fm.corners_in_display_space([(900, 900)] * 4, 6, 400, 200)
    assert mapped is None
