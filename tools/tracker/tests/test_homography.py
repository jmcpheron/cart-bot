"""Round-trip the homography pipeline under a known synthetic camera."""

import math

import numpy as np
import pytest

from tracker import homography as hg

# A plausible overhead-camera homography: room cm -> pixels. Includes scale,
# rotation, the image-y flip, and mild projective terms (camera not at nadir).
ROOM_TO_PX = np.array(
    [
        [2.6, 0.3, 310.0],
        [0.2, -2.5, 460.0],
        [1e-4, -8e-5, 1.0],
    ]
)

FLOOR = {10: (0.0, 0.0), 11: (200.0, 0.0), 12: (200.0, 150.0), 13: (0.0, 150.0)}
SIZE = 15.0


def synthetic_detections() -> dict[int, np.ndarray]:
    return {
        mid: hg.apply(ROOM_TO_PX, hg.floor_corners_room(c, SIZE)).astype(np.float32)
        for mid, c in FLOOR.items()
    }


def test_fit_recovers_room_coords():
    H, rms, used = hg.fit(synthetic_detections(), FLOOR, SIZE)
    assert sorted(used) == [10, 11, 12, 13]
    assert rms < 0.1  # noise-free input
    # arbitrary probe points map back to room coords
    probes = np.array([[50.0, 25.0], [180.0, 140.0]])
    px = hg.apply(ROOM_TO_PX, probes)
    back = hg.apply(H, px)
    assert np.allclose(back, probes, atol=0.05)


def test_fit_requires_two_markers():
    dets = synthetic_detections()
    only_one = {10: dets[10]}
    with pytest.raises(ValueError):
        hg.fit(only_one, FLOOR, SIZE)


def test_fit_works_with_partial_visibility():
    dets = synthetic_detections()
    del dets[12]
    H, rms, used = hg.fit(dets, FLOOR, SIZE)
    assert sorted(used) == [10, 11, 13]
    assert rms < 0.1


def test_pose_from_marker_recovers_heading():
    H, _, _ = hg.fit(synthetic_detections(), FLOOR, SIZE)
    Hinv = np.linalg.inv(H)
    # Robot at (80, 60) heading 30° CCW: build its marker corners in room
    # coords (marker 'up' = heading), project to pixels, recover the pose.
    cx, cy, th = 80.0, 60.0, math.radians(30.0)
    h = 7.5
    up = np.array([math.cos(th), math.sin(th)])
    right = np.array([math.sin(th), -math.cos(th)])
    center = np.array([cx, cy])
    corners_room = np.array(
        [
            center + up * h - right * h,  # TL
            center + up * h + right * h,  # TR
            center - up * h + right * h,  # BR
            center - up * h - right * h,  # BL
        ]
    )
    corners_px = hg.apply(Hinv, corners_room)
    x, y, theta = hg.pose_from_marker(H, corners_px)
    assert abs(x - cx) < 0.1
    assert abs(y - cy) < 0.1
    assert abs(hg.wrap_deg(theta - 30.0)) < 0.2


def test_pose_yaw_offset_applied():
    H, _, _ = hg.fit(synthetic_detections(), FLOOR, SIZE)
    Hinv = np.linalg.inv(H)
    corners_room = hg.floor_corners_room((100.0, 75.0), SIZE)  # 'up' = +y = 90°
    corners_px = hg.apply(Hinv, corners_room)
    _, _, theta = hg.pose_from_marker(H, corners_px, yaw_offset_deg=-15.0)
    assert abs(hg.wrap_deg(theta - 75.0)) < 0.2


def test_drift_detects_camera_bump():
    dets = synthetic_detections()
    H, _, _ = hg.fit(dets, FLOOR, SIZE)
    assert hg.drift_px(H, dets, FLOOR, SIZE) < 0.1
    # nudge every detection 12 px — as if the camera rotated on its mount
    bumped = {mid: c + 12.0 for mid, c in dets.items()}
    assert hg.drift_px(H, bumped, FLOOR, SIZE) > 8.0


def test_wrap_deg():
    assert hg.wrap_deg(190.0) == pytest.approx(-170.0)
    assert hg.wrap_deg(-190.0) == pytest.approx(170.0)
    assert hg.wrap_deg(45.0) == pytest.approx(45.0)
