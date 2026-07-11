"""HTTP client for the robot's /cmd endpoint.

The firmware failsafe is the real safety net: 500 ms of silence -> limp,
1500 ms -> stop. So errors here are swallowed and counted, never raised —
if this process wedges or the WiFi drops, the robot stops itself."""

from __future__ import annotations

import socket
from urllib.parse import urlsplit, urlunsplit

import requests


def clamp(v: float, lo: int = -100, hi: int = 100) -> int:
    return max(lo, min(hi, int(round(v))))


def _resolve_once(url: str) -> str:
    """Replace an mDNS .local hostname with its IP, resolved a single time.
    Re-resolving cartbot.local on every /cmd added 100s of ms per control
    tick — enough to stall the loop that also draws the video."""
    parts = urlsplit(url)
    host = parts.hostname or ""
    if not host.endswith(".local"):
        return url
    try:
        ip = socket.getaddrinfo(host, None, socket.AF_INET)[0][4][0]
    except socket.gaierror:
        return url  # let requests try (and fail visibly) with the name
    netloc = parts.netloc.replace(host, ip)
    return urlunsplit((parts.scheme, netloc, parts.path, parts.query, ""))


class RobotClient:
    def __init__(self, cmd_url: str, timeout_s: float = 0.3) -> None:
        self.cmd_url = _resolve_once(cmd_url)
        self.timeout_s = timeout_s
        self.errors = 0
        self.sent = 0
        self.last_reply = ""
        self._session = requests.Session()

    def drive(self, vx: float, vy: float, w: float) -> bool:
        params = {"vx": clamp(vx), "vy": clamp(vy), "w": clamp(w)}
        try:
            r = self._session.get(self.cmd_url, params=params, timeout=self.timeout_s)
            r.raise_for_status()
            self.last_reply = r.text.strip()
            self.sent += 1
            return True
        except requests.RequestException:
            self.errors += 1
            return False

    def stop(self) -> bool:
        return self.drive(0, 0, 0)
