"""Controller math: frame rotation, deadband, clamps, sign flips."""

import pytest

from tracker.config import ControlConfig, Signs
from tracker.controller import Pose, Target, arrived, step

CFG = ControlConfig(
    rate_hz=10.0,
    kp_lin=1.0,
    kp_ang=1.0,
    max_lin=40,
    max_w=30,
    deadband_cm=3.0,
    deadband_deg=5.0,
    arrive_cm=5.0,
    arrive_deg=10.0,
    arrive_hold_s=0.5,
    lost_stop_s=0.5,
)
PLUS = Signs(vy=1, w=1)


def test_error_ahead_drives_forward():
    cmd = step(Pose(0, 0, 0), Target(10, 0), CFG, PLUS)
    assert cmd.vx == 10 and cmd.vy == 0 and cmd.w == 0


def test_error_left_strafes_left():
    # Facing +x, target at +y (robot's left) -> vy negative (vy+ = right).
    cmd = step(Pose(0, 0, 0), Target(0, 10), CFG, PLUS)
    assert cmd.vx == 0 and cmd.vy == -10


def test_rotation_identity():
    # Same body-frame error at a rotated heading gives the same command:
    # robot facing +y (90°), target straight ahead of it.
    cmd = step(Pose(0, 0, 90), Target(0, 10), CFG, PLUS)
    assert cmd.vx == 10 and abs(cmd.vy) <= 1


def test_heading_error_ccw_gives_negative_w():
    # Target heading CCW of us; w+ is CW, so command must be negative.
    cmd = step(Pose(0, 0, 0), Target(0, 0, heading_deg=20), CFG, PLUS)
    assert cmd.w == -20


def test_heading_wraps_shortest_way():
    cmd = step(Pose(0, 0, 170), Target(0, 0, heading_deg=-170), CFG, PLUS)
    # shortest path is +20° (CCW) -> negative w, magnitude 20
    assert cmd.w == -20


def test_deadbands_zero_independently():
    near = step(Pose(0, 0, 0), Target(2, 0, heading_deg=45), CFG, PLUS)
    assert near.vx == 0 and near.vy == 0 and near.w != 0
    aligned = step(Pose(0, 0, 44), Target(20, 0, heading_deg=45), CFG, PLUS)
    assert aligned.w == 0 and aligned.vx != 0


def test_clamps():
    cmd = step(Pose(0, 0, 0), Target(500, -500, heading_deg=180), CFG, PLUS)
    assert abs(cmd.vx) <= CFG.max_lin
    assert abs(cmd.vy) <= CFG.max_lin
    assert abs(cmd.w) <= CFG.max_w


def test_sign_flips():
    plus = step(Pose(0, 0, 0), Target(0, 10, heading_deg=20), CFG, PLUS)
    flipped = step(Pose(0, 0, 0), Target(0, 10, heading_deg=20), CFG, Signs(vy=-1, w=-1))
    assert flipped.vy == -plus.vy
    assert flipped.w == -plus.w
    assert flipped.vx == plus.vx


def test_arrived_position_only():
    assert arrived(Pose(98, 101, 77), Target(100, 100), CFG)
    assert not arrived(Pose(90, 100, 0), Target(100, 100), CFG)


def test_arrived_with_heading():
    assert arrived(Pose(100, 100, 85), Target(100, 100, heading_deg=90), CFG)
    assert not arrived(Pose(100, 100, 60), Target(100, 100, heading_deg=90), CFG)


def test_commands_within_protocol_range():
    for tx, ty, th in [(1000, 1000, 179), (-1000, 1000, -179), (0.1, -0.1, 0)]:
        cmd = step(Pose(0, 0, 0), Target(tx, ty, heading_deg=th), CFG, PLUS)
        for v in (cmd.vx, cmd.vy, cmd.w):
            assert -100 <= v <= 100
