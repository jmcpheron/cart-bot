"""room.yaml loading — one typed object the rest of the package consumes."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import yaml


@dataclass(frozen=True)
class ControlConfig:
    rate_hz: float
    kp_lin: float
    kp_ang: float
    max_lin: int
    max_w: int
    deadband_cm: float
    deadband_deg: float
    arrive_cm: float
    arrive_deg: float
    arrive_hold_s: float
    lost_stop_s: float


@dataclass(frozen=True)
class Signs:
    vy: int
    w: int


@dataclass(frozen=True)
class RoomConfig:
    camera_source: str | int
    camera_width: int
    camera_height: int
    camera_flip: bool
    dictionary: str
    robot_cmd_url: str
    robot_marker_id: int
    robot_marker_size_cm: float
    yaw_offset_deg: float
    floor_marker_size_cm: float
    floor_markers: dict[int, tuple[float, float]]  # id -> center (x_cm, y_cm)
    signs: Signs
    control: ControlConfig


def load_room(path: str | Path) -> RoomConfig:
    raw = yaml.safe_load(Path(path).read_text())
    marker = raw["robot"]["marker"]
    floors = raw["floor_markers"]
    return RoomConfig(
        camera_source=raw["camera"]["source"],
        camera_width=int(raw["camera"].get("width", 1280)),
        camera_height=int(raw["camera"].get("height", 720)),
        camera_flip=bool(raw["camera"].get("flip", False)),
        dictionary=raw.get("dictionary", "DICT_APRILTAG_16H5"),
        robot_cmd_url=raw["robot"]["cmd_url"],
        robot_marker_id=int(marker["id"]),
        robot_marker_size_cm=float(marker["size_cm"]),
        yaw_offset_deg=float(marker.get("yaw_offset_deg", 0.0)),
        floor_marker_size_cm=float(floors["size_cm"]),
        floor_markers={
            int(mid): (float(xy[0]), float(xy[1]))
            for mid, xy in floors["positions"].items()
        },
        signs=Signs(vy=int(raw["signs"]["vy"]), w=int(raw["signs"]["w"])),
        control=ControlConfig(**{k: v for k, v in raw["control"].items()}),
    )
