"""Pixel -> room-frame mapping. The robot lives on the floor plane, so a
single homography H replaces full solvePnP. Calibrated from the four floor
markers using all 16 corners (far stiffer than 4 centers).

Room frame: origin at floor marker 10's center, +x toward 11, +y toward 13,
headings degrees CCW from +x. Every module downstream speaks room-frame cm,
so a second camera later just contributes another H into the same frame."""

from __future__ import annotations

import json
import math
import time
from pathlib import Path

import cv2
import numpy as np

from .detect import marker_center_px, marker_up_px


def floor_corners_room(center: tuple[float, float], size_cm: float) -> np.ndarray:
    """Room-frame corners (TL,TR,BR,BL) of an axis-aligned floor marker laid
    with its printed top edge toward +y. ArUco reports corners in the
    marker's own orientation, so detected order matches this order."""
    x, y = center
    h = size_cm / 2.0
    return np.array(
        [[x - h, y + h], [x + h, y + h], [x + h, y - h], [x - h, y - h]],
        dtype=np.float64,
    )


def fit(
    detections: dict[int, np.ndarray],
    floor_markers: dict[int, tuple[float, float]],
    size_cm: float,
) -> tuple[np.ndarray, float, list[int]]:
    """Fit H from detected floor markers. Returns (H, rms_px, used_ids).
    Needs at least 2 markers (8 correspondences) but all 4 is the intent."""
    src, dst, used = [], [], []
    for mid, center in floor_markers.items():
        if mid not in detections:
            continue
        src.append(detections[mid].astype(np.float64))
        dst.append(floor_corners_room(center, size_cm))
        used.append(mid)
    if len(used) < 2:
        raise ValueError(f"need >=2 floor markers visible, saw {used}")
    src_pts = np.concatenate(src)
    dst_pts = np.concatenate(dst)
    H, _ = cv2.findHomography(src_pts, dst_pts, 0)
    if H is None:
        raise ValueError("findHomography failed")
    # RMS in *pixels*: reproject room corners back into the image.
    back = apply(np.linalg.inv(H), dst_pts)
    rms = float(np.sqrt(np.mean(np.sum((back - src_pts) ** 2, axis=1))))
    return H, rms, used


def apply(H: np.ndarray, pts: np.ndarray) -> np.ndarray:
    """Apply a homography to an (N,2) array of points."""
    pts = np.asarray(pts, dtype=np.float64).reshape(-1, 2)
    return cv2.perspectiveTransform(pts.reshape(-1, 1, 2), H).reshape(-1, 2)


def pose_from_marker(
    H: np.ndarray, corners: np.ndarray, yaw_offset_deg: float = 0.0
) -> tuple[float, float, float]:
    """Robot pose (x_cm, y_cm, heading_deg CCW from +x) from its marker.
    Heading = direction the marker's 'up' vector points in room coords,
    corrected by the jog-calibrated mounting offset."""
    center = marker_center_px(corners)
    up = marker_up_px(corners)
    mapped = apply(H, np.array([center, center + up]))
    dx, dy = mapped[1] - mapped[0]
    theta = math.degrees(math.atan2(dy, dx)) + yaw_offset_deg
    return float(mapped[0][0]), float(mapped[0][1]), wrap_deg(theta)


def drift_px(
    H: np.ndarray,
    detections: dict[int, np.ndarray],
    floor_markers: dict[int, tuple[float, float]],
    size_cm: float,
) -> float:
    """Max reprojection error (px) of currently-visible floor markers under a
    saved H — rises sharply if the camera has been bumped since calibration."""
    Hinv = np.linalg.inv(H)
    worst = 0.0
    for mid, center in floor_markers.items():
        if mid not in detections:
            continue
        expect = apply(Hinv, floor_corners_room(center, size_cm))
        err = np.max(np.linalg.norm(expect - detections[mid], axis=1))
        worst = max(worst, float(err))
    return worst


def wrap_deg(a: float) -> float:
    return (a + 180.0) % 360.0 - 180.0


def save(path: str | Path, H: np.ndarray, rms_px: float, used_ids: list[int]) -> None:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(
        json.dumps(
            {
                "H": H.tolist(),
                "rms_px": rms_px,
                "floor_marker_ids": used_ids,
                "created": time.strftime("%Y-%m-%d %H:%M:%S"),
            },
            indent=2,
        )
    )


def load(path: str | Path) -> np.ndarray:
    data = json.loads(Path(path).read_text())
    return np.array(data["H"], dtype=np.float64)
