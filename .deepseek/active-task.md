# Active Task: DXF import/export feature

> **Branch:** `feature/dxf` (based on merged dev `b89e599`)
> **Date:** 2026-08-19

## Status: core + TS + UI implemented and unit-verified; awaiting manual app test + commit approval

**Scope (user-approved):** `import_dxf` creates a NEW sketch on ref-plane-xy
(default, `plane_id` optional) with LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE
(bulge→arc)/POINT/SPLINE (de Boor 64-seg)/ELLIPSE; unsupported + degenerate
entities skipped + counted (log warning + `parameters_summary`).
`export_document_dxf` writes the ACTIVE sketch as ASCII AC1027 DXF
(LINE/CIRCLE/ARC/POINT; polygons stay constituent lines). `$INSUNITS`
inches→mm scaling on import; units echoed on export.

## Done

- Core parse/write layers `native/cad-core/src/dxf/dxf_import.{h,cpp}` /
  `dxf_export.{h,cpp}`; document manager methods in
  `impl/dxf_commands.inc` + declaration `.inc`; handlers
  `app/impl/dxf_command_handlers.inc`; schema enum +=
  `import_dxf`/`export_document_dxf`.
- `add_sketch_line` gained `infer_constraints` flag (default true) —
  DXF import passes false + `snap_start=false` so imported geometry
  stays exact (H/V hint snapping deformed near-axis segments).
- Imported vertices are NOT fixed (user feedback 2026-08-20: fix badges
  cluttered the drawing and fixed endpoints block later constraint
  edits). Imported POINTs are unfixed after the first refresh;
  `sync_fixed_point_flags` preserves the unfix.
- dxfrw target fixed: added `intern/dwgBuffer.cpp` + `intern/dwgutil.cpp`
  (link-required by entity parseDwg paths; previous builds reused a
  stale lib). Sanity gate rejects non-DXF files (atoi quirk).
- Tests: new `cad_core_dxf_import_export_test` suite, **15/15 pass**;
  **all 9 C++ suites pass** (`pnpm test:core`).
- TS: `ipcProtocol.ts` builders, `types/ipc.ts`, `aiCommandPayloadSchemas.ts`,
  `ipcSchema.ts` (`format: "dxf"`), `useCadCore.ts` hooks — `tsc --noEmit`
  clean.
- UI: `documentDialogs.ts` (import/open + export/save dxf), `AppHeader.tsx`
  (File menu + ribbon buttons), `AppTopBar.tsx` handlers, `en.json` keys.
- Docs: `wiki/Implementation-Log.md` entry, `help/dxf-library.md` note.

## Remaining

1. **Manual app test** (never commit untested code): restart `pnpm dev`,
   File → Import DXF with a real-world DXF (LibreCAD/QCAD export with
   layers/polylines), confirm sketch activates with geometry, extrude a
   profile, Export DXF, re-import the export, check Logs panel warnings
   for skipped entities.
2. Run full vitest (`npx vitest run`) — TS suites.
3. Commit (needs user approval) + wiki mirror sync (`polysmith.wiki`
   rsync — the mirror dir is not checked out on this machine).
4. Follow-ups (explicitly out of v1, logged in Implementation-Log):
   DXF dimension round-trip, INSERT block resolution, DWG support
   (iconv + excluded sources), drag-drop import.
