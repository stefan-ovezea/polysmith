# Active Task: Stale selection/highlight fix (feature/highlight)

> **Branch:** `feature/highlight` (from `dev`)
> **Date:** 2026-08-21

## Problem (user-reported, pre-dates the text feature)

Areas remained highlighted/selected after tool cancel/confirm and
extrude/sketch-exit (sketch entities, points, profile fills, 3D
faces/edges).

## Fix (implemented, suites green, awaiting user verification + commit)

- Central `prune_document_selection()` in `bump_geometry_revision()`
  drops orphaned sketch-selection ids systemically.
- Per-mutator clears: extrude family + finish_sketch + start_sketch_on_face
  (plural + 3D selections), fillet/chamfer confirm (edges),
  delete_feature (owned 3D ids), clear_selection (text id).
- UI: trim overlay cleared on Escape/tool-switch/before-trim; profile
  hover cleared on Escape; unconditional pointer-leave hover clears;
  `isSelected` added to the scene build key for primitives + reference
  planes.
- New `cad_core_selection_test` suite (11 cases). All 12 C++ suites
  green; tsc clean; vitest green.

## Verification checklist (user)

1. Extrude a profile → finished sketch profile fill not lit; no 3D
   face/edge stays lit.
2. Select a body face → start sketch on another plane → face not lit.
3. Fillet/chamfer: pick edges → Confirm → edges not lit, no different
   edge re-lights; undo restores the pre-session edge selection.
4. Trim: hover (red overlay) → Escape / commit / failed click → overlay
   gone.
5. Click a box → highlight appears (previously broken); click empty
   space → clears. Reference plane selection highlights too.
6. Delete a feature whose face was selected / a selected dimension →
   no stale highlight.
7. Save/load mid-sketch → no phantom highlights.

## Next session

- User verification → fixes → commit approval (squash-merge
  `feature/highlight` → `dev`).
