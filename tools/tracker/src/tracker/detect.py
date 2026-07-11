"""Marker detection. Small dictionaries only (4x4-bit grids): fewer bits =
chunkier cells that survive MJPEG compression at low pixel counts. The
dictionary is a room.yaml setting — DICT_APRILTAG_16H5 matches the FRC-style
tags Jason printed; DICT_4X4_50 is the classic ArUco equivalent. Both decode
with the same cv2.aruco machinery, so the swap is config, not code."""

from __future__ import annotations

import cv2
import numpy as np

DEFAULT_DICTIONARY = "DICT_APRILTAG_16H5"


def make_detector(dictionary: str = DEFAULT_DICTIONARY) -> cv2.aruco.ArucoDetector:
    params = cv2.aruco.DetectorParameters()
    # Subpixel corners: the homography and heading vector are only as good
    # as the corner fixes.
    params.cornerRefinementMethod = cv2.aruco.CORNER_REFINE_SUBPIX
    return cv2.aruco.ArucoDetector(
        cv2.aruco.getPredefinedDictionary(getattr(cv2.aruco, dictionary)), params
    )


def detect(detector: cv2.aruco.ArucoDetector, frame: np.ndarray) -> dict[int, np.ndarray]:
    """Returns {marker_id: corners (4,2) float32 px, order TL,TR,BR,BL}."""
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.ndim == 3 else frame
    corners, ids, _ = detector.detectMarkers(gray)
    if ids is None:
        return {}
    return {int(i): c.reshape(4, 2) for i, c in zip(ids.flatten(), corners)}


def marker_center_px(corners: np.ndarray) -> np.ndarray:
    return corners.mean(axis=0)


def marker_up_px(corners: np.ndarray) -> np.ndarray:
    """Vector (px) from the bottom-edge midpoint to the top-edge midpoint —
    the marker's own 'up'. On the robot, mount the marker with 'up' pointing
    forward (any residual angle is absorbed by yaw_offset_deg)."""
    tl, tr, br, bl = corners
    return (tl + tr) / 2.0 - (bl + br) / 2.0
