# Active Task: STL Import/Convert/Project — freeze FIXED and user-verified; awaiting commit approval

> **Branch:** `feature/stl`
> **Date:** 2026-08-19 (post-verification)

## Status: done except the commit

The post-projection UI freeze was traced to `rectangleDimensionEntityIds`
(O(n⁴) 4-line-combination scan) in `apps/desktop-ui/src/lib/viewportScene.ts`,
rewritten to O(n²) with identical semantics + a skip-guard when no
line_length/line_angle dimensions exist. **User confirmed the fix at runtime:
fan-panel STL now projects and edits fast ("like Fusion").**

All temporary instrumentation from the debug sessions has been removed
(core_debug.log tee, debug_log handler, debugLog marks, proj_diag lines,
REPLAY alias + replay files, remote-debugging arg — one genuinely useful
`log_error("projection", "project_face outline failed")` was kept).
Verification after cleanup: C++ core rebuilt clean, **all 8 C++ suites pass**,
`tsc --noEmit` clean, vitest 32 passed / 5 skipped (incl. new permanent
`viewportSceneRectangleDims.test.ts`).

## Remaining: commit (needs user approval)

- Commit feature/stl (fix + feature work + cleanup). Working tree has the
  whole feature/stl diff (~88 tracked files + ~11 new files).
- EXCLUDE from the commit: `Top Panel with Fan.stl`, `body.stl`,
  `apps/desktop-ui/src-tauri/part.json` (user test saves),
  `native/cad-core/build-asan/` (build dir, not gitignored yet),
  `core_debug.log` (already gitignored).
- Consider adding `build-asan/` to .gitignore.
- After merge: delete feature/stl branch (per branch workflow).

## Log of what was fixed (for the commit message)

1. **O(n⁴) rectangle scan** in `viewportScene.ts` → O(n²) + dimension guard
   (THE freeze fix — see `viewportSceneRectangleDims.test.ts`).
2. Core-side fixes from the earlier session: topology-cache move-order bug,
   coplanar-section leak, silhouette wobble threshold, mesh-face pick storm,
   hidden-body pick invalidation, grid depthTest, project-tool pick skips,
   face-pick tie-break.
3. STL import/convert/project feature work (mesh_import_helpers,
   mesh_projection, mesh_commands, protocol mesh payload blocks,
   `stl_import_test.cpp` — self-contained, generates its own fixtures).
