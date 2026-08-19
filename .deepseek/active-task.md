# Active Task: none (DXF feature shipped)

> **Branch:** `dev` (up to date with origin/dev)
> **Date:** 2026-08-20

## Shipped: DXF import/export (PR #61, squashed to dev as 387c77c)

`import_dxf` (new editable sketch on ref-plane-xy, LINE/CIRCLE/ARC/
LWPOLYLINE+bulge/POLYLINE/POINT/SPLINE/ELLIPSE, skip+count unsupported,
$INSUNITS inches→mm) and `export_document_dxf` (active sketch → ASCII
AC1027). Full details in `wiki/Implementation-Log.md` (2026-08-19 entry).

Verified: 15-case `cad_core_dxf_import_export_test` (all 9 C++ suites
green), tsc clean, vitest green, manual round-trip with Fusion both
directions (exported DXF opened in Fusion; Fusion DXF imported cleanly,
no fix-badge clutter after the no-fixed-vertices change).

Branches: feature/dxf deleted (local + remote, GitHub auto-deleted on
merge); feature/stl deleted earlier after #60. Local dev fast-forwarded
to origin/dev.

## Open items

1. Optional cleanup: local `feature/drag` + `feature/occtv8` are merged
   leftovers (`git branch -d` works for both).
3. Known v1 limitations (documented): TEXT/MTEXT/DIMENSION annotations
   skipped; INSERT blocks not resolved; polygons export as lines;
   SPLINE/ELLIPSE approximated at 64 segments; HATCH skipped; DWG not
   supported (needs iconv + excluded sources).
