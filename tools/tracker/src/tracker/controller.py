"""P-controller: room-frame pose error -> robot-frame (vx, vy, w).

Pure functions only — no I/O, no clocks — so the math is unit-testable.
Conventions (see lib/kinematics/kinematics.h): vx+ = forward, vy+ = strafe
right, w+ = clockwise. Room headings are degrees CCW from +x. The physical
signs of vy/w are jog-calibrated into Signs rather than trusted."""

from __future__ import annotations

import math
from dataclasses import dataclass

from .config import ControlConfig, Signs
from .homography import wrap_deg


@dataclass(frozen=True)
class Pose:
    x: float
    y: float
    theta_deg: float


@dataclass(frozen=True)
class Target:
    x: float
    y: float
    heading_deg: float | None = None  # None = don't care about heading


@dataclass(frozen=True)
class Command:
    vx: int
    vy: int
    w: int

    def is_zero(self) -> bool:
        return self.vx == 0 and self.vy == 0 and self.w == 0


STOP = Command(0, 0, 0)


def _clamp(v: float, limit: int) -> int:
    return max(-limit, min(limit, int(round(v))))


def step(pose: Pose, target: Target, cfg: ControlConfig, signs: Signs) -> Command:
    """One control step. Translation and rotation run simultaneously — the
    platform is holonomic; rotate-then-translate would waste the mecanums."""
    ex = target.x - pose.x
    ey = target.y - pose.y
    et = wrap_deg(target.heading_deg - pose.theta_deg) if target.heading_deg is not None else 0.0

    # Rotate the room-frame error into the robot frame.
    th = math.radians(pose.theta_deg)
    e_fwd = ex * math.cos(th) + ey * math.sin(th)
    e_right = ex * math.sin(th) - ey * math.cos(th)

    vx = vy = w = 0
    if math.hypot(ex, ey) > cfg.deadband_cm:
        vx = _clamp(cfg.kp_lin * e_fwd, cfg.max_lin)
        vy = signs.vy * _clamp(cfg.kp_lin * e_right, cfg.max_lin)
    if abs(et) > cfg.deadband_deg:
        # et > 0 means the target heading is CCW of us; w+ is CW, hence the
        # minus. signs.w then absorbs whatever the hardware actually does.
        w = signs.w * _clamp(-cfg.kp_ang * et, cfg.max_w)
    return Command(vx, vy, w)


def arrived(pose: Pose, target: Target, cfg: ControlConfig) -> bool:
    if math.hypot(target.x - pose.x, target.y - pose.y) > cfg.arrive_cm:
        return False
    if target.heading_deg is None:
        return True
    return abs(wrap_deg(target.heading_deg - pose.theta_deg)) <= cfg.arrive_deg
