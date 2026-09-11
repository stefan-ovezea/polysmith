# Active task: GRBL workspace polish — P1 internal CAM→GRBL handoff (IN PROGRESS)

> **Branch:** `feature/grbl-polish` (from `dev` @ d0db60d)
> **Date:** 2026-09-12
> **Plan:** approved plan at `.claude/plans/jaunty-sleeping-moth.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

The GRBL workspace (#78) works, but "Send to GRBL workspace" pops a
file-save dialog and round-trips the posted G-code through disk. The
user wants a proper internal sender with LaserGRBL-style features.
Approved scope: P1 internal handoff (no file), P2 machine picker
(disk machine library → bed + pointer offset), P3 red laser pointer
crosshair + toggle, P4 laser utilities (test fire / focus pulse /
framing), P5 live overrides (FRO/SRO real-time bytes) + elapsed/ETA,
P6 bed check + alarm decode, P7 console history + $$ settings dialog,
P8 CAM test patterns ride the internal handoff.

## P1 status (implemented, gates running)

Internal handoff = in-memory posted G-code text end to end:

- **C++ core**: `cam_export.h/.cpp` — `post_cam_gcode_text` +
  `CamGcodeTextResult`; posting pipeline extracted into a shared
  `post_document` helper so file export and text export stay
  byte-identical. DocumentManager method
  (`document_manager_io_commands.inc` decl,
  `document_io_commands.inc` def), dispatch branch
  `cam_export_gcode_text` in `app/impl/cam_commands.inc`, event maker
  `make_cam_export_gcode_text_result_event` in `protocol/ipc.h/.cpp`.
  `document_manager.h` now includes `core/cam/cam_export.h` (the .inc
  declaration needs the struct — first build caught this).
- **Protocol**: `cam_export_gcode_text` command + result event in
  `protocol/schema/*.json`; documented in `wiki/IPC-Protocol.md` and
  `wiki/AI-CAD-Command-Language.md`.
- **TS**: `makeCamExportGcodeTextCommand` (camCommands.ts),
  `CamExportGcodeTextResultEvent` (types/ipc.ts), zod schema + union
  entry (ipcSchema.ts), `camExportGcodeText` awaited wrapper
  (useCadCore.ts).
- **Rust**: `grbl_parse_text(text, label)` (gcode_parser.rs, wraps
  pure parse_gcode); `gcode_sender.rs` — `prepare_job_lines` (now
  uses `gcode_parser::filter_lines`, the single-source parity
  contract) + `begin_job` split out of `start_job`, new
  `WorkerMsg::SendProgram` + `grbl_send_program` command; 5 new unit
  tests (filter/guard/ByteWindow). Registered in main.rs.
- **UI**: `grblClient.ts` (`grblParseText`, `grblSendProgram`);
  App.tsx — `grblHandoff {text,label}` state replaces `grblPreviewFile`,
  `exportCamGcodeToGrblWorkspaceAction` posts in memory (no dialog,
  no disk write; failure toast `cam.gcodePostFailed`); GrblWorkspace —
  `LoadedGrblProgram {source: "file"|"internal", text, label, info}`,
  handoff arrival parses via `grblParseText`; CamGrblPanel —
  `embeddedProgram` prop, Cycle Start streams `grblSendProgram(text)`
  for internal programs / `grblSendFile(path)` for disk picks.
- **Tests**: `native/cad-core/tests/cam_export_test.cpp`
  (laser test-pattern doc → non-empty post with header + M5 footer,
  file export byte-equals text, no-setup throws; main wrapped in
  try/catch per the 0xC0000409 trap), registered in CMakeLists.

**Gates — ALL GREEN (2026-09-12)**: cargo test 13/13 ✓, tsc --noEmit
✓, core rebuilt via the VS2022 wrapper (first pass failed on the
missing cam_export.h include in document_manager.h — .inc trap;
fixed), `pnpm test:core` **46/46 suites passed** ✓. New
`cad_core_cam_export_test` pins: non-empty post with operation
header + M5 footer, file export byte-equals in-memory text, no-setup
throws. Test-side bugs found and fixed along the way: dangling
reference to the temporary optional (get_document returns by value),
Windows file-lock on remove (read+close before delete), missing
DocumentState using-declaration.

## P1 — COMMITTED a67b26d (user verified the handoff in-app)

## P2+P3+P4 status (implemented 2026-09-12, user verification pending)

User asked for P2+P3+P4 together (they were looking for the "test"
utilities inside the GRBL page). All three implemented:

- **P2 machine picker**: GrblWorkspace fetches `camMachineList` on
  entry, dropdown in the header (selection persisted under
  `polysmith.grbl.machineName`, `__none__` option); bed fallback
  machine → document machine_settings → default; machine list failure
  toasts `cam.setup.machineListFailed`.
- **P3 red pointer**: `buildGrblPointerMarker`/`updateGrblPointerMarker`
  in grblPreviewScene (token `--cad-pointer-dot`, renderOrder 21,
  refactored shared crosshair builder); viewport props pointerOffset +
  pointerOn with a live effect at MPos + offset; toggle in the new
  utilities panel sends `M3 S13` (≈5 % of 255) / `M5` via grblSendRaw;
  legend entry; pointerOn resets on disconnect.
- **P4 laser utilities**: new `src-tauri/src/grbl_utilities.rs`
  (GrblUtilityRequest tagged enum framing/focusPulse, scale_power
  clamp 1..255, generate_utility_program, grbl_utility_program
  command, 3 unit tests incl. parse-through of both programs);
  WorkerMsg::LaserPower + grbl_laser_power (hold-to-fire); new
  GrblUtilitiesPanel.tsx under CamGrblPanel in the left aside (pointer
  toggle, hold-to-fire test fire, focus pulse, frame-job-from-bounds +
  margin/feed, clear overlay); workspace overlayProgram + sendingTarget
  state so utility progress highlights only the overlay
  (`--cad-framing-overlay` materials, executed still swaps to the
  standard executed color); CamGrblPanel onCycleStart callback clears
  the overlay when the main job starts.
- **Themes**: `--cad-pointer-dot` + `--cad-framing-overlay` added to
  all 6 theme JSONs. i18n: `grbl.machine*`, `grbl.pointer*`,
  `grbl.legendPointer`, `grbl.util.*`.
- **Fixed while type-checking**: the TS CoreCommand union + command
  interface were missing `cam_export_gcode_text` from P1
  (types/ipc/camCommands.ts + types/ipc.ts) — surfaced once the new
  code touched the union.

**Gates — ALL GREEN**: cargo test 17/17 (after the utility-invoke
serde fix below), tsc --noEmit clean. No C++ changes in this batch.

**User-reported bug fixed (2026-09-12):** focus pulse + frame job
toasted "Failed to run the laser utility" on the machine. Root cause:
serde's enum-level `rename_all` renames only VARIANT names, not
struct-variant fields — the UI sends `powerPercent`/`durationSeconds`
(camelCase), the Rust enum expected `power_percent`/`duration_seconds`,
so every `grbl_utility_program` invoke rejected with "missing field
`power_percent`" before any G-code was generated. Fixed with
`rename_all_fields = "camelCase"` (serde 1.0.228) + a regression test
deserializing the exact TS payload (verified: fails without the fix,
17/17 with it). User re-verification pending.

## P5+P6 status (implemented 2026-09-12, user verification pending)

- **P5 live overrides + stats**: WorkerMsg::WriteByte (bare byte, no
  newline, bypasses the RX window) + `grbl_write_byte` command with an
  8-byte allowlist (0x90–0x94 feed, 0x99–0x9B power); CamGrblPanel
  gains two sliders committed on pointer-up via deterministic
  reset-and-step (0x90/0x99 then n×±10 % + m×±1 % feed steps — the
  worker stays stateless), elapsed/ETA row (ticks while streaming,
  freezes on completion, resets on the next start), FluidNC caveat
  line; grblClient.grblWriteByte; i18n cam.grbl.feedOverride/
  powerOverride/overrideCaveat/elapsed/eta.
- **P6 bed check + alarm decode**: `grbl_alarm_message` table (GRBL
  1.1 codes 1–9 + unknown fallback) baked into the Alarm error event
  (test pins every code); GrblWorkspace shows a non-blocking
  "program exceeds the work area" banner when the parsed bounds
  escape the bed (WCS coordinates, warning only); i18n
  grbl.jobExceedsBed.
- **Gates**: cargo test 18/18, tsc --noEmit clean. No C++ changes.

## Next session checklist

1. User verification round on the FluidNC machine (`pnpm dev`), now
   covering P2–P6 together:
   - Machine dropdown resizes the bed; selection survives restart.
   - Red pointer toggle + crosshair at MPos + offset (VERIFY SIGN).
   - Test fire / focus pulse / framing all run after the serde fix
     (restart the app first — the shell rebuilt).
   - Feed/power override sliders: record what FluidNC actually does
     with 0x90–0x94/0x99–0x9B (partial support expected — the caveat
     line says so); elapsed/ETA tick and reset across jobs.
   - Bed banner on an oversized program (or pick a small machine);
     trigger a real alarm → decoded text.
2. On confirmation: commit P2–P6 (no Co-Authored-By trailer),
   then P7 (console history + $$ settings dialog).
