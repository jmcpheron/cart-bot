"""Frame sources. The control loop never blocks on the network: a background
thread keeps the freshest frame, callers take it with its age attached."""

from __future__ import annotations

import threading
import time

import cv2
import numpy as np
import requests


class FrameSource:
    """Background-threaded source; `latest()` returns (frame|None, age_s)."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._frame: np.ndarray | None = None
        self._stamp: float = 0.0
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self) -> "FrameSource":
        self._thread.start()
        return self

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=2.0)

    def latest(self) -> tuple[np.ndarray | None, float]:
        with self._lock:
            if self._frame is None:
                return None, float("inf")
            return self._frame, time.monotonic() - self._stamp

    def _publish(self, frame: np.ndarray) -> None:
        with self._lock:
            self._frame = frame
            self._stamp = time.monotonic()

    def _run(self) -> None:  # pragma: no cover - thread loop
        raise NotImplementedError


class JpegPoller(FrameSource):
    """Polls the ESP32-CAM /jpg endpoint. With the camera's GRAB_LATEST mode
    each response is the freshest frame the sensor has — staleness is bounded
    by one frame plus network time."""

    def __init__(self, url: str, timeout_s: float = 1.0) -> None:
        super().__init__()
        self.url = url
        self.timeout_s = timeout_s
        self._session = requests.Session()

    def _run(self) -> None:  # pragma: no cover - network loop
        while not self._stop.is_set():
            try:
                r = self._session.get(self.url, timeout=self.timeout_s)
                r.raise_for_status()
                frame = cv2.imdecode(
                    np.frombuffer(r.content, np.uint8), cv2.IMREAD_COLOR
                )
                if frame is not None:
                    self._publish(frame)
            except requests.RequestException:
                time.sleep(0.5)  # camera offline — retry gently


class CvCapture(FrameSource):
    """cv2.VideoCapture wrapper — local webcams (int index) and MJPEG URLs.
    The primary source since the ESP32-CAM was benched: a USB webcam does
    720p+ at 30fps where the ESP32 managed one SVGA frame per 10+ seconds."""

    def __init__(self, source: str | int, width: int = 1280,
                 height: int = 720, flip: bool = False) -> None:
        super().__init__()
        self.source = source
        self.width = width
        self.height = height
        # Un-mirror a camera that flips in firmware (some conference cams
        # do). Diagnosis order matters: check TEXT in the scene first — if
        # text reads correctly but tags only decode after flipping, the
        # PRINTED TAGS are mirrored, not the camera (seen 2026-07-10).
        self.flip = flip

    def _run(self) -> None:  # pragma: no cover - capture loop
        cap = cv2.VideoCapture(self.source)
        if isinstance(self.source, int):
            # OpenCV defaults webcams to 640x480; ask for more. The driver
            # gives the nearest mode it supports — check with `tracker cams`.
            cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        while not self._stop.is_set():
            ok, frame = cap.read()
            if ok:
                self._publish(cv2.flip(frame, 1) if self.flip else frame)
            else:
                time.sleep(0.5)
        cap.release()


def probe_cameras(max_index: int = 5) -> list[dict]:
    """Try webcam indices 0..max_index; return [{index, width, height,
    frame}] for the ones that deliver a frame. macOS: if EVERY index is
    dead, the terminal probably lacks camera permission (System Settings ->
    Privacy & Security -> Camera)."""
    found = []
    for i in range(max_index + 1):
        cap = cv2.VideoCapture(i)
        ok, frame = cap.read() if cap.isOpened() else (False, None)
        if ok and frame is not None:
            found.append(
                {"index": i, "width": frame.shape[1], "height": frame.shape[0],
                 "frame": frame}
            )
        cap.release()
    return found


def open_source(source: str | int, width: int = 1280, height: int = 720,
                flip: bool = False) -> FrameSource:
    if isinstance(source, int) or (isinstance(source, str) and source.isdigit()):
        return CvCapture(int(source), width, height, flip).start()
    if isinstance(source, str) and source.rstrip("/").endswith("/jpg"):
        return JpegPoller(source).start()
    return CvCapture(source).start()  # MJPEG stream URL
