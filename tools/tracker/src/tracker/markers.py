"""Printable marker sheets: one US-Letter page per marker at 300 DPI, with a
quiet zone, a caption, an orientation arrow, and a 10 cm ruler bar so a bad
print-scale is caught with a tape measure before it poisons the calibration."""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np
from PIL import Image

from .detect import DEFAULT_DICTIONARY

DPI = 300
PAGE_W, PAGE_H = int(8.5 * DPI), 11 * DPI  # US Letter


def _px(cm: float) -> int:
    return int(round(cm / 2.54 * DPI))


def marker_page(marker_id: int, size_cm: float, caption: str,
                dictionary_name: str = DEFAULT_DICTIONARY) -> np.ndarray:
    page = np.full((PAGE_H, PAGE_W), 255, np.uint8)
    side = _px(size_cm)

    dictionary = cv2.aruco.getPredefinedDictionary(
        getattr(cv2.aruco, dictionary_name))
    # Render at an exact multiple of the 6-module grid (4x4 bits + border),
    # then nearest-neighbor scale: cells stay razor-sharp.
    module = max(1, side // 6)
    marker = cv2.aruco.generateImageMarker(dictionary, marker_id, module * 6)
    marker = cv2.resize(marker, (side, side), interpolation=cv2.INTER_NEAREST)

    x0 = (PAGE_W - side) // 2
    y0 = (PAGE_H - side) // 2 - _px(1.5)
    page[y0 : y0 + side, x0 : x0 + side] = marker

    # Orientation arrow above the top (quiet zone is the empty margin itself;
    # the arrow sits beyond the required 2 cm).
    ax = PAGE_W // 2
    ay = y0 - _px(2.5)
    cv2.arrowedLine(page, (ax, ay + _px(1)), (ax, ay - _px(1)), 0, 8, tipLength=0.4)

    font = cv2.FONT_HERSHEY_SIMPLEX
    cv2.putText(page, "TOP EDGE", (ax + _px(0.6), ay), font, 1.2, 0, 3)
    cv2.putText(
        page, caption, (x0, y0 + side + _px(1.2)), font, 1.4, 0, 3
    )

    # 10 cm ruler bar with cm ticks near the bottom.
    rw = _px(10.0)
    rx, ry = (PAGE_W - rw) // 2, PAGE_H - _px(3.0)
    cv2.rectangle(page, (rx, ry), (rx + rw, ry + _px(0.3)), 0, -1)
    for i in range(11):
        tx = rx + _px(float(i))
        cv2.line(page, (tx, ry), (tx, ry - _px(0.4)), 0, 4)
    cv2.putText(
        page,
        "ruler must measure exactly 10 cm (print at 100% scale)",
        (rx - _px(2.0), ry + _px(1.3)),
        font,
        1.0,
        0,
        2,
    )
    return page


def write_all(
    out_dir: str | Path,
    robot_id: int,
    robot_size_cm: float,
    floor_ids: list[int],
    floor_size_cm: float,
    dictionary_name: str = DEFAULT_DICTIONARY,
) -> list[Path]:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    jobs = [(robot_id, robot_size_cm, f"robot marker  id {robot_id}  {robot_size_cm:g} cm  (top edge = forward)")]
    jobs += [
        (fid, floor_size_cm, f"floor marker  id {fid}  {floor_size_cm:g} cm  (top edge -> +y)")
        for fid in floor_ids
    ]
    paths = []
    for mid, size, caption in jobs:
        page = marker_page(mid, size, caption, dictionary_name)
        path = out / f"marker_{mid:02d}.png"
        # Pillow for the save: cv2.imwrite can't write DPI metadata, and
        # without it print dialogs guess the scale.
        Image.fromarray(page).save(path, dpi=(DPI, DPI))
        paths.append(path)
    return paths
