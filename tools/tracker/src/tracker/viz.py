"""Live OpenCV debug window: detected markers, room grid, pose arrow,
waypoint, and a HUD. Click anywhere on the floor -> room-coord waypoint."""

from __future__ import annotations

import math

import cv2
import numpy as np

from . import homography as hg
from .controller import Command, Pose, Target

WINDOW = "cart-bot tracker"

GRID_STEP_CM = 25.0
COL_GRID = (90, 90, 90)
COL_FLOOR = (0, 200, 255)
COL_ROBOT = (0, 255, 0)
COL_TARGET = (255, 0, 255)
COL_HUD = (255, 255, 255)


class Display:
    def __init__(self, H: np.ndarray | None = None) -> None:
        self.H = H
        self.Hinv = np.linalg.inv(H) if H is not None else None
        self.clicked: tuple[float, float] | None = None  # room cm
        cv2.namedWindow(WINDOW)
        cv2.setMouseCallback(WINDOW, self._on_mouse)

    def _on_mouse(self, event: int, x: int, y: int, *_args) -> None:
        if event == cv2.EVENT_LBUTTONDOWN and self.H is not None:
            rx, ry = hg.apply(self.H, np.array([[x, y]], dtype=float))[0]
            self.clicked = (float(rx), float(ry))

    def take_click(self) -> tuple[float, float] | None:
        c, self.clicked = self.clicked, None
        return c

    def draw_markers(self, frame: np.ndarray, detections: dict[int, np.ndarray],
                     robot_id: int) -> None:
        for mid, corners in detections.items():
            color = COL_ROBOT if mid == robot_id else COL_FLOOR
            cv2.polylines(frame, [corners.astype(int)], True, color, 2)
            cx, cy = corners.mean(axis=0).astype(int)
            cv2.putText(frame, str(mid), (cx + 8, cy - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)

    def draw_grid(self, frame: np.ndarray,
                  floor_markers: dict[int, tuple[float, float]]) -> None:
        if self.Hinv is None:
            return
        xs = [p[0] for p in floor_markers.values()]
        ys = [p[1] for p in floor_markers.values()]
        x_lo, x_hi = min(xs) - GRID_STEP_CM, max(xs) + GRID_STEP_CM
        y_lo, y_hi = min(ys) - GRID_STEP_CM, max(ys) + GRID_STEP_CM
        for gx in np.arange(x_lo, x_hi + 1, GRID_STEP_CM):
            self._line_room(frame, (gx, y_lo), (gx, y_hi), COL_GRID)
        for gy in np.arange(y_lo, y_hi + 1, GRID_STEP_CM):
            self._line_room(frame, (x_lo, gy), (x_hi, gy), COL_GRID)

    def _line_room(self, frame, a, b, color, thickness=1) -> None:
        pts = hg.apply(self.Hinv, np.array([a, b])).astype(int)
        cv2.line(frame, tuple(pts[0]), tuple(pts[1]), color, thickness)

    def draw_pose(self, frame: np.ndarray, pose: Pose) -> None:
        if self.Hinv is None:
            return
        th = math.radians(pose.theta_deg)
        tip = (pose.x + 20 * math.cos(th), pose.y + 20 * math.sin(th))
        pts = hg.apply(self.Hinv, np.array([[pose.x, pose.y], tip])).astype(int)
        cv2.arrowedLine(frame, tuple(pts[0]), tuple(pts[1]), COL_ROBOT, 3,
                        tipLength=0.3)

    def draw_target(self, frame: np.ndarray, target: Target | None) -> None:
        if target is None or self.Hinv is None:
            return
        px, py = hg.apply(self.Hinv, np.array([[target.x, target.y]]))[0].astype(int)
        cv2.drawMarker(frame, (px, py), COL_TARGET, cv2.MARKER_CROSS, 24, 3)

    def draw_route(self, frame: np.ndarray, waypoints: list[Target],
                   next_idx: int) -> None:
        """The plan: numbered waypoints joined by a polyline. Visited legs
        dim, the active leg bright."""
        if not waypoints or self.Hinv is None:
            return
        pts = hg.apply(self.Hinv,
                       np.array([[w.x, w.y] for w in waypoints])).astype(int)
        for i in range(1, len(pts)):
            color = COL_TARGET if i == next_idx else COL_GRID
            cv2.line(frame, tuple(pts[i - 1]), tuple(pts[i]), color, 2,
                     cv2.LINE_AA)
        for i, p in enumerate(pts):
            done = i < next_idx
            cv2.circle(frame, tuple(p), 12, COL_GRID if done else COL_TARGET, 2)
            cv2.putText(frame, str(i + 1), (p[0] - 5, p[1] + 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        COL_GRID if done else COL_TARGET, 2)

    def draw_trail(self, frame: np.ndarray, trail) -> None:
        """Breadcrumbs of where the robot has actually been."""
        if self.Hinv is None or len(trail) < 2:
            return
        pts = hg.apply(self.Hinv, np.array(trail)).astype(int)
        for i in range(1, len(pts)):
            cv2.line(frame, tuple(pts[i - 1]), tuple(pts[i]), COL_ROBOT, 1,
                     cv2.LINE_AA)

    def hud(self, frame: np.ndarray, lines: list[str]) -> None:
        for i, text in enumerate(lines):
            y = 28 + 26 * i
            cv2.putText(frame, text, (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.65,
                        (0, 0, 0), 4)
            cv2.putText(frame, text, (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.65,
                        COL_HUD, 1)

    def show(self, frame: np.ndarray) -> int:
        cv2.imshow(WINDOW, frame)
        return cv2.waitKey(1) & 0xFF


def hud_lines(age_s: float, pose: Pose | None, cmd: Command | None,
              extra: str = "") -> list[str]:
    lines = [f"frame age {age_s * 1000:5.0f} ms"]
    if pose is not None:
        lines.append(f"pose x {pose.x:6.1f}  y {pose.y:6.1f}  th {pose.theta_deg:6.1f}")
    else:
        lines.append("pose --- (robot marker not seen)")
    if cmd is not None:
        lines.append(f"cmd vx {cmd.vx:4d}  vy {cmd.vy:4d}  w {cmd.w:4d}")
    if extra:
        lines.append(extra)
    return lines
