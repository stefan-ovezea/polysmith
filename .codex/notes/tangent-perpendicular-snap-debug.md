# Tangent / Perpendicular snap debug — continue from here

## What's done (committed)
- Circle body snap: `speculativeCircleBodySnap()` works, shows "c" badge
- Tangent overrides circle body on same circle (priority logic)
- `onCircle` label, `on_circle` constraint kind → "c" glyph
- All TypeScript types, interfaces, and tests pass (7 tests)
- Speculative solver iterations bumped 5→12

## What's NOT working
- **Tangent snap** (`tangent_lc`): never fires in actual app
- **Perpendicular snap** (`perpendicular_ll`): never fires in actual app
- **Parallel snap** (`parallel`): WORKS — proves the bridge is fine

## Hypothesis
The TS proximity gating passes (verified by tests). The issue must be in the
WASM solver: `tangent_lc` and `perpendicular_ll` speculative constraints
don't converge, even with 12 iterations. `parallel` converges easily.

## Next steps to debug
1. Add temporary logging in `runInferenceAndReadCursor()` to capture
   solver status codes for each snap type (status 0/1 = converged, 
   status 2 = failed, status 3 = invalid solution).
2. Check if `tangent_lc` and `perpendicular_ll` return status 2 (failed)
   or status 3 (invalid) — this would tell us the solver can't handle
   these constraint types in speculative mode.
3. Check `get_gcs_conflicting_constraints()` — maybe existing sketch
   constraints conflict with the speculative tangent/perpendicular.
4. Verify circle `center_vertex_id` is set (not undefined) when pushing
   circles to the solver — if undefined, tangent_lc has no center ref.
5. Check planegcs WASM docs: does `tangent_lc` support `temporary: true`?

## Files changed
- `apps/desktop-ui/src/layout/viewport/snapResolution.ts` — main snap logic
- `apps/desktop-ui/src/layout/viewport/snapResolution.test.ts` — tests
- `apps/desktop-ui/src/layout/viewport/constraintPreview.ts` — badge kinds
- `apps/desktop-ui/src/layout/viewport/ViewportOverlays.tsx` — glyphs
- `apps/desktop-ui/src/layout/ViewportPanel.tsx` — label wiring
- `apps/desktop-ui/src/types/viewport.ts` — SketchPreviewPoint fields
- `apps/desktop-ui/src/lib/speculativeSolve.ts` — iterations 5→12
