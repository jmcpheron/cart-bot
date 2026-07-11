"""tracker CLI — bring-up order: markers -> view -> calibrate -> pose -> jog
-> hold -> goto -> route. Run from tools/tracker: `uv run tracker <cmd>`."""

from __future__ import annotations

import math
import time
from pathlib import Path

import click
import cv2
import numpy as np
import yaml

from . import homography as hg
from . import markers as marker_gen
from . import viz
from .camera import FrameSource, open_source, probe_cameras
from .config import RoomConfig, load_room
from .controller import Command, Pose, Target, arrived, step
from .detect import detect, make_detector
from .robot import RobotClient

DEFAULT_ROOM = "config/room.yaml"
DEFAULT_CAL = "out/calibration.json"


@click.group()
def main() -> None:
    """Overhead-camera tracking + closed-loop control for cart-bot."""


def _room_option(fn):
    return click.option("--room", default=DEFAULT_ROOM, show_default=True,
                        help="room.yaml path")(fn)


def _load(room: str) -> RoomConfig:
    return load_room(room)


def _load_H(cal: str) -> np.ndarray:
    if not Path(cal).exists():
        raise click.ClickException(
            f"no calibration at {cal} — run `tracker calibrate` first")
    return hg.load(cal)


def _open(cfg: RoomConfig, source: str | None = None) -> FrameSource:
    src = cfg.camera_source if source is None else source
    return open_source(src, cfg.camera_width, cfg.camera_height, cfg.camera_flip)


# ------------------------------------------------------------------- cams ---

@main.command()
@click.option("--snap", is_flag=True, help="save one frame per camera to out/")
@click.option("--max-index", default=5, show_default=True)
def cams(snap: bool, max_index: int) -> None:
    """Probe webcam indices — find which one is the overhead camera."""
    found = probe_cameras(max_index)
    if not found:
        raise click.ClickException(
            "no cameras delivered a frame — on macOS check System Settings "
            "-> Privacy & Security -> Camera for your terminal, and close "
            "apps (Zoom) that may be holding the device")
    for cam in found:
        line = f"index {cam['index']}: {cam['width']}x{cam['height']}"
        if snap:
            out = Path("out") / f"cam_{cam['index']}.jpg"
            out.parent.mkdir(parents=True, exist_ok=True)
            cv2.imwrite(str(out), cam["frame"])
            line += f" -> {out}"
        click.echo(line)
    click.echo("set camera.source in room.yaml to the overhead camera's index")


# ------------------------------------------------------------------ check ---

@main.command()
@_room_option
@click.option("--source", default=None, help="override room.yaml camera source")
@click.option("--seconds", default=10.0, show_default=True)
@click.option("--out", "out_path", default="out/check.jpg", show_default=True)
def check(room: str, source: str | None, seconds: float, out_path: str) -> None:
    """Headless detection meter: hit rates, marker sizes, fps, snapshot."""
    cfg = _load(room)
    src = _open(cfg, source)
    det = make_detector(cfg.dictionary)
    hits: dict[int, int] = {}
    sizes: dict[int, float] = {}
    frames = 0
    last = None
    t0 = time.time()
    try:
        deadline = t0 + seconds
        prev_frame = None
        while time.time() < deadline:
            frame, _age = src.latest()
            if frame is None or frame is prev_frame:
                time.sleep(0.01)
                continue
            prev_frame = frame
            frames += 1
            detections = detect(det, frame)
            for mid, corners in detections.items():
                hits[mid] = hits.get(mid, 0) + 1
                sizes[mid] = float(np.linalg.norm(corners[0] - corners[1]))
            last = (frame, detections)
    finally:
        src.close()
    if frames == 0:
        raise click.ClickException("no frames — wrong index? camera in use?")
    click.echo(f"{frames} frames in {time.time() - t0:.1f}s "
               f"({frames / (time.time() - t0):.1f} fps)")
    expected = sorted(set(cfg.floor_markers) | {cfg.robot_marker_id})
    for mid in sorted(set(expected) | set(hits)):
        rate = 100.0 * hits.get(mid, 0) / frames
        size = f"{sizes[mid]:.0f}px" if mid in sizes else "--"
        tag = "" if mid in expected else "  (unexpected id)"
        click.echo(f"  marker {mid:2d}: {rate:5.1f}%  {size}{tag}")
    if last is not None:
        frame, detections = last
        for mid, corners in detections.items():
            cv2.polylines(frame, [corners.astype(int)], True, (0, 255, 0), 2)
            cx, cy = corners.mean(axis=0).astype(int)
            cv2.putText(frame, f"id {mid}", (cx + 8, cy - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        Path(out_path).parent.mkdir(parents=True, exist_ok=True)
        cv2.imwrite(out_path, frame)
        click.echo(f"snapshot -> {out_path}")


# ---------------------------------------------------------------- markers ---

@main.command()
@_room_option
@click.option("--out", default="out/markers", show_default=True)
def markers(room: str, out: str) -> None:
    """Generate printable marker pages (one Letter sheet each, 300 DPI)."""
    cfg = _load(room)
    paths = marker_gen.write_all(
        out,
        robot_id=cfg.robot_marker_id,
        robot_size_cm=cfg.robot_marker_size_cm,
        floor_ids=sorted(cfg.floor_markers),
        floor_size_cm=cfg.floor_marker_size_cm,
        dictionary_name=cfg.dictionary,
    )
    for p in paths:
        click.echo(f"wrote {p}")
    click.echo("Print at 100% scale and confirm the ruler bar is 10 cm.")


# ------------------------------------------------------------------- view ---

@main.command()
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
@click.option("--source", default=None, help="override room.yaml camera source")
def view(room: str, cal: str, source: str | None) -> None:
    """Live camera view with marker detection (calibration optional)."""
    cfg = _load(room)
    H = hg.load(cal) if Path(cal).exists() else None
    src = _open(cfg, source)
    det = make_detector(cfg.dictionary)
    disp = viz.Display(H)
    seen = hits = 0
    try:
        while True:
            frame, age = src.latest()
            if frame is None:
                time.sleep(0.1)
                continue
            detections = detect(det, frame)
            seen += 1
            expected = set(cfg.floor_markers) | {cfg.robot_marker_id}
            hits += len(expected & set(detections)) == len(expected)
            disp.draw_grid(frame, cfg.floor_markers)
            disp.draw_markers(frame, detections, cfg.robot_marker_id)
            disp.hud(frame, viz.hud_lines(
                age, None, None,
                f"markers {sorted(detections)}  all-visible {100 * hits / seen:.0f}%"))
            if disp.show(frame) == ord("q"):
                break
    finally:
        src.close()
        cv2.destroyAllWindows()


# -------------------------------------------------------------- calibrate ---

@main.command()
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
@click.option("--frames", default=30, show_default=True)
@click.option("--max-rms", default=3.0, show_default=True,
              help="reject calibration above this reprojection RMS (px)")
def calibrate(room: str, cal: str, frames: int, max_rms: float) -> None:
    """Fit the pixel->room homography from the floor markers."""
    cfg = _load(room)
    src = _open(cfg)
    det = make_detector(cfg.dictionary)
    sums: dict[int, np.ndarray] = {}
    counts: dict[int, int] = {}
    try:
        collected = 0
        while collected < frames:
            frame, _ = src.latest()
            if frame is None:
                time.sleep(0.1)
                continue
            detections = detect(det, frame)
            for mid in cfg.floor_markers:
                if mid in detections:
                    sums[mid] = sums.get(mid, 0) + detections[mid]
                    counts[mid] = counts.get(mid, 0) + 1
            collected += 1
            time.sleep(0.05)
    finally:
        src.close()

    averaged = {mid: sums[mid] / counts[mid] for mid in sums}
    for mid in cfg.floor_markers:
        n = counts.get(mid, 0)
        click.echo(f"floor marker {mid}: seen in {n}/{frames} frames")
    H, rms, used = hg.fit(averaged, cfg.floor_markers, cfg.floor_marker_size_cm)
    click.echo(f"homography from markers {used}, reprojection RMS {rms:.2f} px")
    if rms > max_rms:
        raise click.ClickException(
            f"RMS {rms:.2f} px > {max_rms} px — check the tape-measured "
            "positions in room.yaml and that markers lie flat")
    hg.save(cal, H, rms, used)
    click.echo(f"saved {cal}")


# ---------------------------------------------------------------- tracking --

class Tracker:
    """Shared frame->pose plumbing for pose/jog/hold/goto/route."""

    def __init__(self, cfg: RoomConfig, H: np.ndarray) -> None:
        self.cfg = cfg
        self.H = H
        self.src: FrameSource = open_source(
            cfg.camera_source, cfg.camera_width, cfg.camera_height,
            cfg.camera_flip)
        self.det = make_detector(cfg.dictionary)
        self.detections: dict[int, np.ndarray] = {}
        self.frame: np.ndarray | None = None
        self.age = float("inf")
        self.last_pose_mono = 0.0

    def read(self) -> Pose | None:
        self.frame, self.age = self.src.latest()
        if self.frame is None:
            return None
        self.detections = detect(self.det, self.frame)
        corners = self.detections.get(self.cfg.robot_marker_id)
        if corners is None:
            return None
        pose = Pose(*hg.pose_from_marker(self.H, corners, self.cfg.yaw_offset_deg))
        self.last_pose_mono = time.monotonic()
        return pose

    def lost_for(self) -> float:
        return time.monotonic() - self.last_pose_mono

    def drift_warn(self) -> str:
        d = hg.drift_px(self.H, self.detections, self.cfg.floor_markers,
                        self.cfg.floor_marker_size_cm)
        return f"CAMERA MOVED? floor drift {d:.0f} px" if d > 8.0 else ""

    def close(self) -> None:
        self.src.close()


def _run_loop(cfg: RoomConfig, H: np.ndarray, targets: list[Target],
              click_to_go: bool, label: str) -> None:
    """The closed-loop driver. Marker lost > lost_stop_s: one explicit stop,
    then silence — the firmware failsafe backstops a dead process."""
    tracker = Tracker(cfg, H)
    robot = RobotClient(cfg.robot_cmd_url)
    disp = viz.Display(H)
    ctl = cfg.control
    queue = list(targets)
    target: Target | None = queue.pop(0) if queue else None
    stop_sent = False
    arrive_since: float | None = None
    period = 1.0 / ctl.rate_hz

    # Ctrl-C raises KeyboardInterrupt; the finally block zeros the robot.
    try:
        while True:
            t0 = time.monotonic()
            pose = tracker.read()
            cmd: Command | None = None

            if pose is None or tracker.age > ctl.lost_stop_s:
                if tracker.lost_for() > ctl.lost_stop_s and not stop_sent:
                    robot.stop()
                    stop_sent = True
                    click.echo("robot marker lost — sent stop, holding silent")
            elif target is not None:
                stop_sent = False
                cmd = step(pose, target, ctl, cfg.signs)
                robot.drive(cmd.vx, cmd.vy, cmd.w)
                if arrived(pose, target, ctl):
                    arrive_since = arrive_since or time.monotonic()
                    if time.monotonic() - arrive_since >= ctl.arrive_hold_s:
                        click.echo(f"arrived at ({target.x:.0f}, {target.y:.0f})")
                        target = queue.pop(0) if queue else None
                        arrive_since = None
                        if target is None:
                            robot.stop()
                            if not click_to_go:
                                break
                else:
                    arrive_since = None
            else:
                stop_sent = False

            if tracker.frame is not None:
                frame = tracker.frame.copy()
                disp.draw_grid(frame, cfg.floor_markers)
                disp.draw_markers(frame, tracker.detections, cfg.robot_marker_id)
                if pose:
                    disp.draw_pose(frame, pose)
                disp.draw_target(frame, target)
                extra = f"{label}  errors {robot.errors}  {tracker.drift_warn()}"
                disp.hud(frame, viz.hud_lines(tracker.age, pose, cmd, extra))
                key = disp.show(frame)
                if key == ord("q"):
                    break
                if key == ord(" "):
                    target = None
                    queue.clear()
                    robot.stop()
                    click.echo("target cleared — robot stopped")
            if click_to_go and (clicked := disp.take_click()):
                target = Target(*clicked)
                click.echo(f"new target ({clicked[0]:.0f}, {clicked[1]:.0f})")

            time.sleep(max(0.0, period - (time.monotonic() - t0)))
    except KeyboardInterrupt:
        pass
    finally:
        robot.stop()
        tracker.close()
        cv2.destroyAllWindows()


# ------------------------------------------------------------------- pose ---

@main.command()
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
def pose(room: str, cal: str) -> None:
    """Live pose overlay — push the robot around by hand and sanity-check
    coordinates against a tape measure. No commands are sent."""
    cfg = _load(room)
    tracker = Tracker(cfg, _load_H(cal))
    disp = viz.Display(tracker.H)
    try:
        while True:
            p = tracker.read()
            if tracker.frame is None:
                time.sleep(0.1)
                continue
            frame = tracker.frame.copy()
            disp.draw_grid(frame, cfg.floor_markers)
            disp.draw_markers(frame, tracker.detections, cfg.robot_marker_id)
            if p:
                disp.draw_pose(frame, p)
            disp.hud(frame, viz.hud_lines(tracker.age, p, None, tracker.drift_warn()))
            if disp.show(frame) == ord("q"):
                break
    finally:
        tracker.close()
        cv2.destroyAllWindows()


# -------------------------------------------------------------------- jog ---

@main.command()
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
@click.option("--power", default=20, show_default=True)
@click.option("--secs", default=0.6, show_default=True)
def jog(room: str, cal: str, power: int, secs: float) -> None:
    """Pulse each axis and measure what the robot actually did. Prints the
    signs/yaw_offset block to paste into room.yaml — never trust the wiring."""
    cfg = _load(room)
    tracker = Tracker(cfg, _load_H(cal))
    robot = RobotClient(cfg.robot_cmd_url)

    def settle_pose(timeout: float = 3.0) -> Pose:
        deadline = time.monotonic() + timeout
        samples: list[Pose] = []
        while time.monotonic() < deadline:
            p = tracker.read()
            if p:
                samples.append(p)
                if len(samples) >= 5:
                    return Pose(
                        float(np.mean([s.x for s in samples[-5:]])),
                        float(np.mean([s.y for s in samples[-5:]])),
                        float(samples[-1].theta_deg),
                    )
            time.sleep(0.1)
        raise click.ClickException("robot marker not visible")

    def pulse(vx: int, vy: int, w: int) -> tuple[Pose, Pose]:
        before = settle_pose()
        end = time.monotonic() + secs
        while time.monotonic() < end:
            robot.drive(vx, vy, w)
            time.sleep(0.08)
        robot.stop()
        time.sleep(0.8)  # let it coast to rest
        return before, settle_pose()

    try:
        click.echo(f"jogging vx=+{power} …")
        b, a = pulse(power, 0, 0)
        move_ang = math.degrees(math.atan2(a.y - b.y, a.x - b.x))
        yaw_off = hg.wrap_deg(move_ang - b.theta_deg + cfg.yaw_offset_deg)
        click.echo(f"  moved {math.hypot(a.x-b.x, a.y-b.y):.1f} cm toward "
                   f"{move_ang:.0f}°; marker heading was {b.theta_deg:.0f}°")

        click.echo(f"jogging vy=+{power} …")
        b, a = pulse(0, power, 0)
        rel = hg.wrap_deg(math.degrees(math.atan2(a.y - b.y, a.x - b.x))
                          - (b.theta_deg - cfg.yaw_offset_deg + yaw_off))
        vy_sign = 1 if rel < 0 else -1  # right of heading = negative rel angle
        click.echo(f"  moved {rel:.0f}° relative to heading -> "
                   f"{'right' if rel < 0 else 'left'}")

        click.echo(f"jogging w=+{power} …")
        b, a = pulse(0, 0, power)
        dth = hg.wrap_deg(a.theta_deg - b.theta_deg)
        w_sign = 1 if dth < 0 else -1  # w+ should be CW (heading decreases)
        click.echo(f"  heading changed {dth:+.0f}° -> "
                   f"{'CW' if dth < 0 else 'CCW'}")

        click.echo("\npaste into room.yaml:")
        click.echo(f"  robot.marker.yaw_offset_deg: {yaw_off:.1f}")
        click.echo(f"  signs.vy: {vy_sign}")
        click.echo(f"  signs.w: {w_sign}")
    finally:
        robot.stop()
        tracker.close()


# ------------------------------------------------------------- hold/goto ----

@main.command()
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
def hold(room: str, cal: str) -> None:
    """Hold the robot's current pose against pushes — the gain-tuning test."""
    cfg = _load(room)
    H = _load_H(cal)
    tracker = Tracker(cfg, H)
    try:
        p = None
        deadline = time.monotonic() + 5.0
        while p is None and time.monotonic() < deadline:
            p = tracker.read()
            time.sleep(0.1)
    finally:
        tracker.close()
    if p is None:
        raise click.ClickException("robot marker not visible")
    click.echo(f"holding ({p.x:.0f}, {p.y:.0f}, {p.theta_deg:.0f}°) — "
               "push the robot, it should fight back. q quits.")
    _run_loop(cfg, H, [Target(p.x, p.y, p.theta_deg)], click_to_go=True,
              label="hold")


@main.command()
@click.argument("x", type=float)
@click.argument("y", type=float)
@click.option("--heading", type=float, default=None,
              help="final heading deg CCW from +x (default: don't care)")
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
def goto(x: float, y: float, heading: float | None, room: str, cal: str) -> None:
    """Drive to (X, Y) cm in the room frame. Click the view to retarget."""
    cfg = _load(room)
    _run_loop(cfg, _load_H(cal), [Target(x, y, heading)], click_to_go=True,
              label=f"goto {x:.0f},{y:.0f}")


@main.command()
@click.argument("waypoints", type=click.Path(exists=True))
@_room_option
@click.option("--cal", default=DEFAULT_CAL, show_default=True)
def route(waypoints: str, room: str, cal: str) -> None:
    """Follow a YAML waypoint list: [{x: 50, y: 100, heading: 90}, …]."""
    cfg = _load(room)
    raw = yaml.safe_load(Path(waypoints).read_text())
    targets = [Target(float(p["x"]), float(p["y"]),
                      float(p["heading"]) if "heading" in p else None)
               for p in raw]
    click.echo(f"{len(targets)} waypoints")
    _run_loop(cfg, _load_H(cal), targets, click_to_go=False, label="route")
