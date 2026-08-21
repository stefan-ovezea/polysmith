# Active Task: IGES import + export (feature/iges)

> **Branch:** `feature/iges` (from `dev`, after #66)
> **Date:** 2026-08-22

## Status

Implemented, **awaiting user verification in the running app** — nothing
committed yet (no-untested-commits rule).

## What landed

- **`import_iges { file_path }`** — mirror of the STEP import:
  `iges_import` body feature, parse-once, self-contained B-rep
  snapshot in `part.json`, mm conversion via `xstep.cascade.unit`,
  source units from the IGES global section, parse-before-mutate.
- **`export_document_iges { file_path }`** — new: `IGESControl_Writer`
  in BRep mode (`write.iges.brep.mode = 1`, set AFTER
  `IGESControl_Controller::Init()` — its first call registers the
  static with the Faces default) so bodies export as MSBO (186)
  solids, not surface-only faces mode.
- **UI**: File menu → Import IGES... / Export IGES...
  (`.iges`/`.igs`); `iges_import` in both `BODY_KINDS` sets;
  `document_exported` format enum += `"iges"`.
- **Tests**: `cad_core_iges_import_export_test` (11 cases, mirror of
  the STEP suite incl. INCH-unit file, export re-import round-trip).
  All 15 C++ suites green; `tsc --noEmit` clean.

Key files: `core/geometry/iges_import_helpers.{h,cpp}`,
`core/export/export.cpp` (`export_document_as_iges`),
`core/document/impl/iges_commands.inc`,
`app/impl/iges_command_handlers.inc`, `tests/iges_import_export_test.cpp`,
UI `documentDialogs.ts` / `AppTopBar.tsx` / `AppHeader.tsx`.

## Verification checklist (user, in the running app)

1. Import an IGES (export one from PolySmith itself via the new
   Export IGES action, or a real .iges/.igs file) → body appears,
   hierarchy lists "IGES Import" with the units summary.
2. Export IGES → re-import the exported file in PolySmith (or open
   in another CAD) → geometry intact.
3. Boolean/fillet on the imported body; undo/redo the import.
4. Save, close, reopen → body intact; delete the source .iges,
   reopen → still intact.
5. Garbage .iges → error, document unchanged.

## Known out-of-scope (do not touch)

- Disabled `test_mesh_face_projection_stays_healthy_after_load` in
  `tests/face_projection_arc_test.cpp` (Task #27 crash — pre-existing).

## Next session

- User verification → commit approval → push → draft PR to `dev`.
