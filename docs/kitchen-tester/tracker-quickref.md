# Tracker quick reference — live view, aiming, and the command set

Day-to-day cheat sheet for the camera tracking system. The full setup story
(tag placement theory, measuring, camera height math) is in
[../production/tracker-setup.md](../production/tracker-setup.md); this page
is the "I'm standing in the room, what do I type" version.

All commands run from `tools/tracker/`:

```sh
cd tools/tracker
```

## The live view (aiming + tag checking)

```sh
uv run tracker view
```

Opens a window with the camera feed and live marker detection:

- **Green boxes + id labels** on every detected tag, ~25 fps.
- HUD top line: frame age. Bottom line: which ids are visible right now and
  the running percentage of frames where ALL expected tags were seen —
  that's the aiming meter. Re-aim / re-tape until it holds high.
- After calibration the window also draws the room-coordinate grid.
- **`q` quits.** Spacebar clears the target and stops the robot (when a
  control command is using the same window).

Use it as a viewfinder whenever anything physical changes: camera moved,
tags re-taped, lighting changed.

## Finding the right camera

Indices shuffle when USB devices are replugged. When the view shows the
wrong feed (or nothing):

```sh
uv run tracker cams --snap     # one snapshot per camera into out/
open out/cam_*.jpg             # look — which one is the tripod?
```

Put the winning index in `config/room.yaml` under `camera.source`.
Watch out: an iPhone appears as a camera via Continuity Camera.

## Headless detection meter (no window needed)

```sh
uv run tracker check --seconds 10
```

Prints per-tag hit rates + pixel sizes + fps and saves an annotated
snapshot to `out/check.jpg`. Healthy numbers: **>90% per tag, 30+ px**.
This is the command Claude runs remotely; it's also the proof a setup
change helped or hurt.

## The full bring-up ladder (in order)

| Step | Command | Pass criteria |
|---|---|---|
| 1. identify camera | `tracker cams --snap` | right feed in the snapshot |
| 2. aim + tape tags | `tracker view` | all 4 floor tags boxed steadily |
| 3. measure quality | `tracker check` | >90% hit, 30+ px per tag |
| 4. measure the room | tape measure → `config/room.yaml` | diagonal ≈ √(A²+B²) |
| 5. calibrate | `tracker calibrate` | reprojection RMS < 3 px |
| 6. sanity-check pose | `tracker pose` | hand-slid tag 0 tracks in cm |
| 7. sign calibration | `tracker jog` | prints signs/yaw block for room.yaml |
| 8. closed loop | `tracker hold`, `goto X Y`, `route file.yaml` | robot parks itself |

## Physical rules learned the hard way

- **Tags must be dead flat.** Bare paper buckles — mount on cardboard.
  A bent tag is invisible to the detector while looking fine to you.
- **All floor-tag arrows point the same direction** (that's +y). One
  rotated tag silently poisons the calibration.
- **Don't stand in the frame** during `check`/`calibrate` — a blocked tag
  reads as a flaky tag.
- **Prints must not be mirrored** (beware "mirror image" print options).
  Symptom: detector finds the square but never decodes it.
- **Don't move the camera after calibrating.** The HUD shows
  `CAMERA MOVED?` if the floor tags drift — recalibrate (it's one command).
- Robot tag 0 rides on the roof, arrow pointing forward.

## When something's weird

| Symptom | Likely cause | Fix |
|---|---|---|
| all cameras dead | macOS camera permission | System Settings → Privacy → Camera → allow the terminal |
| view shows wrong room | camera index shuffled | `cams --snap`, update room.yaml |
| tag seen but never decodes | mirrored print or wrong dictionary | reprint from `tracker markers` output; dictionary is in room.yaml |
| one tag flickers | glare band / oblique angle / curl | slide the tag, kill the lamp, flatten |
| robot ignores commands | not on WiFi | `curl http://cartbot.local/cmd?vx=0&vy=0&w=0` — reflash/power-cycle if dead |
