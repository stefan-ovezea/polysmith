# Active Task: STEP file import (feature/step)

> **Branch:** `feature/step` (from `dev`, after #65)
> **Date:** 2026-08-22

## Status

Implemented, **awaiting user verification in the running app** — nothing
committed yet (no-untested-commits rule).

## What landed

- **`import_step { file_path }`** — new IPC command + File menu →
  "Import STEP..." (mirrors the DXF import chain; no Tauri changes).
- **`step_import` body feature**: non-parametric imported solid. File
  parsed ONCE at import (mm conversion via `xstep.cascade.unit`;
  original unit in `parameters_summary`); live `TopoDS_Shape` handle in
  params + B-rep snapshot persisted in `part.json` (`include_opaque`
  gating) — self-contained, source file not needed afterwards, no
  `dependency_broken`. Multi-solid files = ONE body (compound).
- **Compile hooks**: `compile_bodies_mesh_requirement.inc` +
  `compile_bodies_modifier_replay.inc` (mesh path, no fuse); snapshot
  deserialization fallback after load. Fillet/boolean/etc. work on
  imported solids; re-export via existing `export_document`.
- **Tests**: new `cad_core_step_import_test` (11 cases incl.
  hand-written AP203 INCH fixture with `CONVERSION_BASED_UNIT`,
  downstream cut extrude, serialization round-trip, error paths).
  All 14 C++ suites green; UI `tsc --noEmit` clean.

Key files: `core/geometry/step_import_helpers.{h,cpp}`,
`core/document/impl/step_commands.inc`,
`compile_bodies_modifier_replay.inc`, `app/impl/step_command_handlers.inc`,
`tests/step_import_test.cpp`, UI `documentDialogs.ts` / `AppTopBar.tsx` /
`AppHeader.tsx`, wiki `Implementation-Log.md` / `IPC-Protocol.md` /
`AI-CAD-Command-Language.md`.

## Verification checklist (user, in the running app)

1. Import a STEP exported from PolySmith itself → body appears,
   selectable, timeline "STEP Import" with `file · N faces · … → mm`.
2. Import an inch-unit STEP → mm scale correct, summary shows `INCH → mm`.
3. Multi-solid STEP → ONE body/one timeline entry.
4. Sketch-on-face + extrude cut on the imported body; fillet an
   imported edge (solid path, unlike STL meshes).
5. Undo/redo the import.
6. Save, close, reopen → body intact; move/delete the .step, reopen →
   still intact, no warning badge.
7. Export STEP → re-exported file opens (`ISO-10303-21` header).
8. Import a garbage .step → error, document unchanged.

## Known out-of-scope (do not touch)

- Disabled `test_mesh_face_projection_stays_healthy_after_load` in
  `tests/face_projection_arc_test.cpp` (Task #27 crash — pre-existing).
- v1 limits: assembly structure/names/colors not read (plain reader,
  no CAF); shells-only imports render but solid-only ops no-op.

## Next session

- User verification → commit approval → separate commits (core, tests,
  UI, docs). Only stage the STEP import files — the working tree also
  has unrelated user changes (deleted `Ventola.STEP`, `part.json`,
  `untitled-part.*`, modified `.gitignore`).
