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

## Next session checklist

1. Hand to the user for in-app verification on the FluidNC machine
   (`pnpm dev`): laser-cut → "Send to GRBL workspace" shows NO
   dialog, preview shows the job labeled "CAM job — <name>", Cycle
   Start streams it, progress matches preview (filter parity
   regression to watch). Also confirm manual Export G-code +
   LaserGRBL launch still behave as before.
2. On user confirmation: commit P1 (no Co-Authored-By trailer;
   message names the suites that ran), then continue P2+P3 (machine
   picker + red pointer).
