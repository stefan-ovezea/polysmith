# Active task: Laser machine bring-up — FluidNC board swap + first cut (2026-09-13)

> **Branch:** `cam/laser-testing` (checked out; pushes to origin with this update)
> **Previous sprint (GRBL transport, PR #80) is merged — this file now tracks the live-machine work.**
> **2026-09-13 (this machine): connection-loss investigation COMPLETE — root
> cause found and fixed in `gcode_sender.rs` + workspace; see "Root cause" below.
> UNCOMMITTED on `cam/laser-testing`; pending user in-app verification.**
> **2026-09-13 round 2 (this machine): GRBL workspace polish + gcode generation
> work — orientation cube consolidated, burned-color cut progress, big STOP
> button, laser cut from body faces with contour cleanup. All 46 core suites
> green + tsc clean. UNCOMMITTED; pending user verification on the machine.**

## ROOT CAUSE of the first-cut failures (found 2026-09-13, this machine)

The sender's status poll sent **`?` WITH a newline** every 500 ms. On this
FluidNC board a newline-terminated `?` is a LINE command: it answers with the
status AND an extra **ok**. Each spurious ok pops a real line's slot from the
127-byte window accounting → the window drifts open without bound → the sender
over-drives the board's input queue → dropped bytes (USB error:36 "I/J tail
dropped"), dropped TCP connections (~line 862), and the WS channel wedging
silently. Verified empirically on the board (bare `?` → status only, no ok;
`?\n` → status + ok) and by replay (a faithful sender replica with `?\n`
fails, with bare `?` the whole 1876-line dense synthetic job completes on
TCP AND WS). LaserGRBL's separate failure at ~raw 1700 is not this bug (it
polls with a bare byte) — likely the board planner + file density, TBD.

Fixes applied in this working tree (gcode_sender.rs, uncommitted):
1. **Poll sends the bare real-time byte** `?` (no newline) — never an ok.
2. **Stray-ok guard**: an ok with nothing in sent_lengths mid-job is logged
   and ignored, never acked against a line.
3. **Link-loss handling**: EOF or hard read error mid-job now aborts the job
   state, tries a last-ditch reset byte, emits a loud error event
   ("CONNECTION LOST mid-job … press Reset or cut power immediately") +
   disconnected. EOF no longer busy-spins the worker.
4. **WS final-ack fallback**: FluidNC's WS channel never acks the FINAL line
   of a job (verified: 3-line job gets 2 oks) — completion now also fires
   when all lines are sent and the board reports Idle for 1.5 s.
5. **React loop fixed**: `GrblWorkspace` memoizes the embeddedProgram object
   (was: fresh object every render → panel effect → setState → "Maximum
   update depth exceeded"); the panel effect now content-compares too.
6. **GRBL viewport orientation**: left-drag rotates, right-drag pans (the
   shared config disabled both), plus a mini orientation cube overlay
   (click a face to snap top/front/right/…; new tokens --cad-cube-x/y/z/edge
   in all 6 themes).

Diagnostic harness: `%TEMP%\grbl_replay.mjs` — faithful sender replica
(filter parity, 127-byte window, pump-on-ok, poll cadence) for TCP 23 and
WS 80 (hand-rolled WS client), synthetic dense-job generator (830 1° arcs +
G1 fill). Usage: `node grbl_replay.mjs [host] [port] [file|synthetic] [pollMs] [tcp|ws]`.
Board must be in CHECK MODE (`$C`, status shows `<Check|…>`) before use.

## Follow-up (not urgent)

- **Orientation cube duplication:** `app/grbl/grblOrientationCube.ts` is a
  second implementation next to the main viewport's
  `layout/viewport/viewCubeRender.ts` + `utils/viewCube.utils.ts`. The
  split is architectural (GRBL preview = own renderer/canvas, Z-up bed
  convention; main cube = render-target blit inside the CAD viewport's
  renderer, Y-up, animated snaps). Still ~80% of the face-texture /
  token / edge / picking logic is shared — extract a common
  `orientationCubeCore` (mesh build + picking, parameterized by up-axis
  and snap-vs-animate) and let both consume it. Do AFTER the cutting
  tests (touches the CAD viewport's cube).

## Next-session checklist

1. **User verification in the app** (binding): stream the real .nc over TCP
   and WS — it should complete now; watch for the stray-ok warnings in the
   console on any transport.
2. Re-test the real file on the OTHER station (the 3490-line one lives
   there); USB error:36 should be gone with the poll fix (the CH340 drop was
   our over-send, not the FIFO).
3. LaserGRBL's failure (~raw 1700) is separate — if it persists after our
   fixes, the board planner / file density is still a factor.
4. WS note: FluidNC never acks the final line over WS — jobs end via the
   Idle fallback now; TCP acks everything, so TCP remains the recommended
   transport for streaming on this board.
5. Commit after verification (no Co-Authored-By trailer).

## G-code generation round (2026-09-13, uncommitted on this machine)

The burn-test file is terrible at the SOURCE: the part is mesh → body →
**projected sketch**, and the laser op cut the sketch profiles — the
projection/arrangement splits curves into ~1° arc fragments and duplicates
edges. Fix: cut the BODY directly (the laser generator already had a face
path) + heal every contour before the kerf offset.

- **Face-path exactness** (`laser/laser_generate.cpp`): the face path now
  builds `build_base_segments_from_wire` (exact line/circle edges → real
  G2/G3, one arc per circle) with the old sampled-polyline fallback for
  spline/ellipse wires. Full-circle loop area + orientation resolved via a
  wireLoopArea helper (shoelace is 0 for start == end).
- **Contour cleanup** (`cam2d.h/.cpp` — `cleanup_base_segments`): merges
  consecutive collinear lines (angle eps 1e-4 rad, both signs tested),
  consecutive co-circular arcs (center/radius eps 1e-4), drops consecutive
  exact duplicates, out-and-back spurs, and zero-length lines. Applied in
  `plan_loop` — one choke point for profile outers/holes AND face wires.
  One structured log line per generate: "contour cleanup: merged N lines…"
  (tag `cam_laser`, visible in the Logs panel).
- **UI** (`CamLaserCutPanel` + `CamFloatingPanels` + `App.tsx`): laser ops
  with a face region show "Cut from body face" + a **Pick face** armed pick
  (mirrors the contour op's face pick; TNP-safe capture, stock faces
  rejected). Face selection at 2D-Cut time already worked
  (`camLaserActions.ts`); this adds RE-picking for existing ops.
- **GRBL workspace polish (same uncommitted batch)**: orientation cube
  deduplicated into the shared `@/utils` cube core (GRBL shell only);
  executed toolpath segments render in the new `--cad-toolpath-burned`
  token (all 6 themes) — "Cut (burned)" legend; big STOP button in the
  workspace toolbar (bg-danger, grblReset + overlay clear, disabled when
  disconnected).
- **Tests**: `cam2d_test.cpp` Test 13 (cleanup unit rules, both epsilon
  signs); `cam_generators_test.cpp` Tests 8b (circular face → ONE exact
  arc, no chord polylines) + 8c (split sketch side merges into one cut).
  All 46 suites green (`pnpm test:core`), `tsc --noEmit` clean.

## Join-arc snap round (2026-09-13, after analyzing untitled-part2.nc)

The user regenerated: **untitled-part2.nc (1877 lines) vs
untitled-part.nc (3489)** — the face path + cleanup halved the file. File
analysis found the remaining noise: every G1 is followed by a **2-micron
G3 with radius 0.075 (= kerf/2) and 1.5–2° sweep** — degenerate
round-join arcs from `append_round_join`. They fire at REFLEX facet
corners where the miter crossing (distance ≈ d/sin θ) lands beyond the
short segment ends; convex corners miter at the corner point (t ≈ 1).
332 such G3s in part2.

Fix: `offset_closed_loop` now snaps the corner (current.end =
nextOffset.start) when |sweep| < 5° (kJoinSnapSweepRad) — the arc's
sagitta there is ~6 µm, far below the 150 µm kerf. Tests: cam2d Test 14
(reflex notch fixture, both sides of 5°: dy 0.02 → snap / dy 0.0225 →
one join in [5°,6°]) + cam_generators 8d (120-gon → ZERO arcs, pre-fix
it carried 120 degenerate joins). All 46 suites green.

Remaining in part2 after this fix (≈1545 lines): ~950 facet G1s ≥1 mm
(the mesh's own resolution) + ~320 micro-facets <0.2 mm at tight corners
(real model geometry). Optional next step: Douglas-Peucker polyline
simplification of the base loop at ~0.02–0.05 mm deviation.

**User verification on the machine (binding):** RESTART the app (a
running `pnpm dev` locks cad_core.exe — MSB3073), select the mesh body's
top face → 2D Cut → Generate → check the Logs panel for the cleanup
counts → export (untitled-part3) + burn. Acceptance: no G3 noise, GRBL
runs it clean.

## Machine state (all hardware verified in-hand)

- Old board MKS DLC32 (GRBL 1.1h) replaced by **MKS LS ESP32 PRO V2.1_002**,
  mainline **FluidNC v4.0.3 esp32s3-wifi**, WiFi STA `192.168.1.19`.
- Wiring: fully plug-and-play (DLC32 V2.1 shares XH connectors). Exceptions:
  - Dual-Y gantry: old PCB mirrored the Y2 pins, LS does NOT → swap BOTH
    phase pairs (A↔B) on ONE Y motor plug (verified working).
  - 2-pin power-switch port next to the DC jack must be jumpered (installed).
  - SPREAD jumpers ON (SpreadCycle — audible hum is normal).
  - No limit switches on this machine; Zero XY at the part corner before run.
- Live config: `C:\Users\PC\grbl_tools\laser-board.yaml` (uploaded to board):
  - `engine: Timed` — **I2S_STATIC causes "Configuration is invalid"
    error:152 on the S3 build; RMT is not compiled in.** Runtime switch:
    `$X` then `$/Stepping/Engine=Timed` (case-sensitive).
  - X step/dir gpio.16/15, Y gpio.7/**6:low** (`:low` = direction invert —
    FluidNC inverts direction via the pin attribute, not a stepstick field),
    limits gpio.39/40 (unused), laser gpio.2, 80 steps/mm, 6000 feed,
    500 accel, `junction_deviation_mm: 0.03`, `arc_tolerance_mm: 0.05`,
    `planner_blocks: 60` (all tuned 2026-09-13, not yet proven by a full cut).
- HTTP upload protocol that works: `POST /files` multipart with `path=/`,
  `/<name>S=<size>`, `myfile[]=@<win-path>;filename=/<name>` (curl must use
  a Windows path — `/tmp` breaks; filename override is REQUIRED or the file
  lands under the local basename).

## First-cut failures (the open problem)

The job (`res/untitled-part.nc`, 3490 raw / 1875 filtered lines, 830 tiny
1°-step arcs) does not complete on ANY transport:

| Path | Failure |
|---|---|
| App USB (CH340, 115200) | FluidNC **error 36** "no offsets in plane" at ~line 468 — I/J tail of the arc line dropped. Old GRBL board had error 1 at the SAME line (same cause). |
| App TCP :23 | Board drops the connection at ~filtered line 862 (reproduced in check mode with the app's exact 127-byte window accounting). Board survives. |
| App WS :80 | Dies within the first ~3 holes. |
| LaserGRBL USB | Died ~raw line 1700 ("board died" = connection drop; board survives). |

Working theory, two stacked causes:
1. **CH340 USB has no flow control** (tiny FIFO) — 127-byte bursts drop bytes
   mid-line → the clean parse errors (36/1) at a repeatable spot.
2. **The dense file overwhelms FluidNC v4.0.3** (1° arc steps × tight
   tolerances → planner churn; user saw jerky motion right before failures).
   Connection drops at varying lines per transport; check-mode replay
   reproduced it over TCP.

Fixes applied so far: tolerance/planner tuning (above). Not yet verified.
**Discriminator test for the other station:** upload the .nc to the board FS
and run `$SD/Run=untitled-part.nc` (no host streaming). Completes → host
transports are the problem; dies → file/firmware. Also: stream a trivial
30-line square over each transport; try a FluidNC build newer than v4.0.3.

**Safety gap:** when a connection dies mid-job the board KEEPS CUTTING
(buffered lines + laser on). Hit Reset immediately. App bug: on link
EOF/error while a job is in flight, `gcode_sender.rs` run() just stops
reading — no abort, no loud error event. Fix alongside the React bug.

## React bug found 2026-09-13 — "Maximum update depth exceeded" (TCP)

- `apps/desktop-ui/src/app/GrblWorkspace.tsx` ~line 474:
  ```tsx
  embeddedProgram={
    loadedProgram?.source === "internal"
      ? { text: loadedProgram.text, label: loadedProgram.label }  // NEW OBJECT EVERY RENDER
      : embeddedProgram
  }
  ```
- `apps/desktop-ui/src/layout/CamGrblPanel.tsx` line 254:
  ```tsx
  useEffect(() => { if (embeddedProgram) setLoadedProgram(embeddedProgram); },
    [embeddedProgram]);
  ```
- Object identity changes every render → effect → setState → render → loop.
  TCP makes it hot: 5 Hz status events re-render the workspace.
- **Fix:** wrap the object in `useMemo(..., [loadedProgram, embeddedProgram])`
  in GrblWorkspace (canonical), or compare by content in the panel effect.

## Next-session checklist

1. Fix the React loop (useMemo) — verify by connecting TCP and watching for
   the warning to disappear.
2. Fix the connection-loss safety gap in gcode_sender.rs (emit error + abort).
3. Run the discriminator tests on the other station ($SD/Run, simple square,
   newer FluidNC).
4. CAM-side optimization (the real cam/laser-testing work): bigger arc spans
   (5–10°), merge collinear G1s, dedupe the 36 duplicate holes — a leaner
   file reduces load on every transport and fixes the jerky section.
5. First completed cut → calibration check (jog 100 mm vs ruler), laser power
   curve, then commit CAM changes with test coverage.

## Key files

- `apps/desktop-ui/src-tauri/src/gcode_sender.rs` — worker, ByteWindow(127),
  500 ms `?` poll, EOF handling gap
- `apps/desktop-ui/src/app/GrblWorkspace.tsx` — embeddedProgram loop source
- `apps/desktop-ui/src/layout/CamGrblPanel.tsx` — loop consumer (effect 254)
- `C:\Users\PC\grbl_tools\laser-board.yaml` — machine config (mirror of board)
- `res/untitled-part.nc` — the problem job (bounds 0..232.7 × 0..172.4)
