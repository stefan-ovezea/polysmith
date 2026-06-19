# Dimension Tool — Regression Todo List

**Branch:** `dimensions-2`
**Date:** 2026-06-19

## High Priority

- [ ] **Angle constraint not enforced** — Brace nesting error in `else` wrapper in `constraint_solver_dimension_constraints.inc`. Existing angle dimensions may lose solver constraint. Lines move freely.
- [ ] **Angle update wrong/missing** — Variable scoping issues in `dimension_angle_update.inc`. `delta`/`current_signed_for_value` declared at outer scope, sed rewrites introduced subtle bugs.
- [ ] **Angle rendering broken for normal dims** — Brace nesting error in `else` wrapper in `sketch_angle_dimension_primitive.inc`.
- [ ] **Length/value not recalculating after drag** — If `SketchDimension` serialization change caused silent parse failure, dimensions might not re-sync after geometry changes. Check `feature_parameter_sketch_dimension_parser.inc`.

## Medium Priority

- [ ] **Double-click same line no longer clears staged pick** — UI shortcut in `dimensionToolPicking.ts` bypasses `handleDimensionStagedEntity` which handles the re-click-to-clear case.
- [ ] **Angle preview appears for unrelated lines** — `sharedLineEndpoint` in `dimensionRelationPreviewGeometry.ts` now returns virtual pivot for ANY non-parallel lines, even ones the user isn't trying to dimension.

## DXF / DWG Support (libdxfrw)

- [ ] **DXF import** — Read DXF files into Polysmith sketches. libdxfrw integrated at `third_party/libdxfrw/`, built as `dxfrw.lib`. Needs DXF entity → SketchEntity mapping layer.
- [ ] **DXF export** — Write sketch geometry to DXF files. Inverse of import.
- [ ] **DWG support** — Enable DWG read/write. Requires iconv (`vcpkg install libiconv` on Windows), adding excluded DWG sources to CMake, replacing the `drw_textcodec_stub.cpp` with the real implementation. See `help/dxf-library.md` for full instructions.
- [ ] **DXF dimension round-trip** — Import/export dimension annotations (not just geometry). libdxfrw has `test_dimensions.cpp` — the DXF dimension entity parser exists.
- [ ] **Save/load integration** — Wire DXF import/export into the app UI (File > Import/Export menus, or drag-and-drop onto viewport).

## Approach

Most regressions stem from wrapping existing code in new `if/else` blocks and sed-based edits. Recommended fix:

1. Revert the structural changes (remove the `else` wrappers)
2. Add virtual pivot support as an **early return** at the TOP of each function — cleanly separate from existing paths
3. For `dimension_angle_update.inc`: rewrite the virtual pivot block from scratch without sed artifacts
