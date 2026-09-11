# Active task: GRBL workspace (feature/grbl) — MILESTONE COMMITTED adf2572; TCP transport implemented (uncommitted) (2026-09-11)

> **Branch:** `feature/grbl` (from `dev` @ 48bd7c4)
> **Date:** 2026-09-11
> **Plan:** approved plan at `.claude/plans/woolly-crunching-balloon.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

User wanted a NEW standalone **GRBL workspace** (like CAD/CAM/ISO
Drawing) hosting the existing GRBL machine panel plus a **toolpath
preview**: parsed .nc rendering (dashed rapids, solid cuts, arcs),
live executed-progress highlight, live machine crosshair from MPos,
bed/work-area grid. Job sources: in-page open-file dialog AND a CAM
handoff ("Send to GRBL workspace" on the two laser panels). User
chose ALL preview features, KEEP BOTH panel entries (CAM setup +
workspace), BOTH job sources.

**Real machine: FluidNC v4.0.3 (esp32s3-wifi), COM7 @ 115200.** The
transport was debugged and verified against it over serial traces.
Key FluidNC findings baked into the code:

- FluidNC status reports `WCO:` (work coordinate OFFSET), not GRBL's
  `WPos:` — the parser derives WPos = MPos − WCO (Zero XY reads 0,0).
- `G10 L20 P0` (MCS zero) is acked but silently does nothing on
  FluidNC v4 — the "Zero machine" button was removed; **Home ($H) is
  the real re-sync** (it resets the MCS; the job only ran after it).
- `$20=0` (soft-limit toggle) is REJECTED on FluidNC v4 (error 162) —
  soft limits are config.yaml-only; the panel shows an
  outside-work-area hint instead.
- FluidNC emits `ALARM:n` lines; soft-limit alarms made the sender
  fake-complete (buffered lines kept acking). Now ALARM aborts the
  job with a clear error.
- Zero XY sends `$X` + `G92 X0 Y0` — one press, works in alarm state
  (GRBL 1.1 and FluidNC alike).

## Phases

- **P1 — COMMITTED 413abac** "feat(desktop): pure G-code parser for
  the GRBL preview": `gcode_parser.rs` (pure `parse_gcode` +
  `grbl_parse_file` command, filter parity with gcode_sender so
  `move.line` == `linesSent`; G0/G1/G2/G3 I/J + full circles, G20/
  G91/G4, M3/M4/M5, N-prefix/comments, warnings for G92/R-arcs) +
  the project's first Rust test module (7 tests incl. golden
  fixture from the user's real export). Gates: cargo test + check.
- **P2 — COMMITTED 9c91073** "feat(desktop): GRBL workspace in the
  switcher reusing the GRBL panel": WorkspaceView += grbl (both
  union copies), switcher entry, GrblWorkspace page (SlicerWorkspace
  template) hosting CamGrblPanel embedded (optional embedded/
  embeddedFilePath props — CAM setup path untouched). Gates: tsc +
  in-app (connect/stream verified by the user).
- **P3+P4 — COMMITTED adf2572** as ONE feature commit (user asked for
  a single commit): "feat(desktop): GRBL workspace preview, CAM
  handoff, FluidNC transport fixes". Everything from the two bullets
  above plus the follow-up fixes: panel Load always visible beside
  Cycle Start and feeding the workspace preview via `onFileLoaded`
  (disk-pick previously streamed a file the preview never showed);
  Zero XY derives WPos=(0,0,0) at the last known MPos LOCALLY and
  applies it instantly (FluidNC can take seconds to publish a G92 in
  its status reports — that latency was the "Zero XY hangs" and
  "preview changes by itself" reports). Gates: cargo test 8/8 +
  check + tsc; real-machine pass done (user: "well now it works").

## Increment: Network (TCP) transport — IMPLEMENTED (uncommitted)

User installed FigUI (FluidNC WebUI) at http://192.168.1.19/ and
asked for network support in the GRBL workspace. Implemented **TCP
(port 23)** — FluidNC's telnet port carries the same text protocol as
USB serial, so the worker's port became a `GrblLink` enum
(Serial | Tcp) with matching read/write; new `grbl_connect_tcp`
command (resolve → connect_timeout 5 s → nodelay → 50 ms read
timeout); panel Connection section gained a Transport dropdown
(Serial (USB) / Network (TCP), host + port inputs); grblStore
connected-log omits baud for TCP. WebSocket (81) deliberately NOT
done — same control capability, much heavier handshake; revisit only
if asked. Gates: cargo test 8/8 + check + tsc. **NOT yet exercised
against the real board — user must restart the app and connect to
192.168.1.19:23.**

## Next session checklist

- Have the user verify the TCP path on the real board: restart →
  Transport: Network (TCP) → 192.168.1.19:23 → connect → status/jog/
  zero/stream identical to serial. (If the board refuses a second
  connection while FigUI is open, close FigUI — the board only
  serves so many clients.)
- Then commit the TCP increment (or fold into the push), push
  feature/grbl, draft PR to dev; hold for the user's final look,
  merge on approval, delete branch.
- dist/ gotcha: fresh clones need `pnpm --filter desktop-ui build`
  before cargo runs (dist/ is gitignored; generate_context! needs it).
