# Overhead camera tracking — setup & bring-up

Roadmap Milestone 2, first working version: one AI-Thinker ESP32-CAM on the
kitchen ceiling, ArUco markers on the robot and floor, `tools/tracker/` on
the Mac closing the loop through the robot's existing `/cmd` endpoint.

Division of labor (unchanged from the roadmap): the ESP32-CAM **only
streams**; the robot ESP32 stays a **dumb motor controller** behind its
existing failsafe; all vision and control run on the host.

## 1. One-time setup

### Secrets
```
cp secrets.env.example secrets.env     # repo root, gitignored
# edit: HOME_WIFI_SSID / HOME_WIFI_PASS
```
Both firmware builds pick the credentials up automatically
(`firmware/load_secrets.py`). No file → AP-only firmware, as before.

### Flash the robot
```
cd firmware && pio run -e robot -t upload && pio device monitor
```
Serial prints the LAN IP (`[comms] LAN http://…`) and warns if the router's
channel breaks the legacy transmitter link (remedy: pin the router's 2.4 GHz
band to channel 1, or ignore — the tracker replaces the transmitter).
Check from the Mac: `curl 'http://cartbot.local/cmd?vx=0&vy=0&w=0'`

### Flash the camera (bare board — no auto-reset)
1. FTDI on **5V**, wired to the ESP32-CAM (TX↔U0R, RX↔U0T, GND, 5V).
2. Jumper **GPIO0 → GND**, press RESET (enters bootloader).
3. `cd firmware && pio run -e camera -t upload`
4. Remove the jumper, press RESET again.
5. `pio device monitor -e camera` → prints `[cam] ready — …/jpg`.

Browser check: `http://cartbot-cam.local:81/stream` (aiming view),
`http://cartbot-cam.local/jpg` (what the tracker polls),
`/status`, and `/set?framesize=uxga` for runtime tuning.

### Print the markers
```
cd tools/tracker && uv run tracker markers
```
Five US-Letter pages in `out/markers/`: robot marker (id 0) + floor markers
(ids 10–13), all 15 cm. **Print at 100% scale** and tape-measure the ruler
bar on each sheet — it must read exactly 10 cm. Mount on cardboard/foamboard
so they lie dead flat; curl ruins corner accuracy.

## 2. Physical layout

- **Camera**: kitchen ceiling (~2.4 m), lens pointed straight down over the
  test area (within ~10° of vertical is fine — the homography absorbs it).
  At 2.2 m above the floor the OV2640's ~66°×49° FOV covers about
  **2.9 × 2.0 m**. Power from a **solid 5 V / 2 A** wall adapter with a short,
  thick USB cable — long thin cables brown out the ESP32-CAM the moment WiFi
  transmits (this is the #1 bring-up killer).
- **Floor markers 10–13**: tape one near each corner of the camera's view,
  **axis-aligned**, printed arrow ("TOP EDGE") pointing the same way on all
  four — that direction becomes **+y**. Marker 10 is the room origin; +x runs
  toward marker 11. Tape-measure each marker's **center** from marker 10's
  center and write the numbers into `tools/tracker/config/room.yaml`.
- **Robot marker 0**: flat on the top plate, arrow pointing forward (any
  error gets measured by `tracker jog` into `yaw_offset_deg`).

Detection target: the 15 cm markers are ~42 px across at SVGA from that
height — comfortably above the ~40 px reliability floor. If detection is
flaky: lock exposure (`/set?aec=0&aec_value=300`), then raise resolution
(`/set?framesize=uxga`, ~5 fps is fine — the robot is slow), and only then
consider AprilTag (isolated inside `src/tracker/detect.py`).

## 3. Bring-up (each step gates the next)

```
cd tools/tracker
uv run tracker view        # all 5 markers detected in >90% of frames
uv run tracker calibrate   # homography, must report RMS < 3 px
uv run tracker pose        # push the robot by hand; coords track the tape measure
uv run tracker jog         # pulses each axis; paste the printed signs/yaw block into room.yaml
uv run tracker hold        # robot fights back when pushed; tune kp gains here
uv run tracker goto 100 75 # drive to a point; click the window to retarget
uv run tracker route demo.yaml   # waypoint list — "drive the square FOR REAL"
```

Gain tuning in `hold`: raise `control.kp_lin` (room.yaml) until the robot
faintly oscillates around the target, then back off ~30%. Same for `kp_ang`.

## 4. Safety model

- The tracker sends `/cmd` at 10 Hz. The robot's own failsafe
  (`firmware/src/robot/failsafe.cpp`) limps at 500 ms of silence and stops at
  1500 ms — killing the tracker process (or the WiFi) stops the robot.
- Marker lost > 0.5 s → tracker sends one explicit `0,0,0` then goes silent.
- `q` quits any window, space bar clears the target and stops, Ctrl-C stops.
- The tracker continuously reprojects the floor markers; a "CAMERA MOVED?"
  HUD warning means recalibrate before trusting coordinates.

## 5. Latency & where this goes next

`/jpg` polling + `CAMERA_GRAB_LATEST` on the camera bounds frame staleness to
roughly one frame + network time; the HUD shows frame age live. If p95 age
exceeds ~300 ms, move the tracker onto lovelace (Ethernet, always on) — it's
plain Python + OpenCV, nothing Mac-specific.

Multi-camera tiling later: every camera gets its own homography into the
same floor-marker room frame, so fusion is "prefer the camera that sees the
robot" — the coordinate system already agrees. That work lives entirely in
`src/tracker/homography.py` + a camera list in room.yaml.
