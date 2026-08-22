# Active Task: Sketch toolset finalization (feature/sketch)

> **Branch:** `feature/sketch` (from `dev`, after #67)
> **Date:** 2026-08-22
> **Plan:** approved plan at `.claude/plans/rosy-gliding-pizza.md` (milestones
> SK0–SK8; each independently committable; every milestone gated on
> `pnpm test:core` + `tsc` green + user runtime verification)

## Status

**SK0 (housekeeping) — code-complete, tested, pending commit.**
- `commands.schema.json` synced to the real dispatch chain (+40 commands,
  −4 stale point→vertex renames); `sketch_tool_ids.h` canonical tool list
  with both whitelists delegating; `aiCommandPayloadSchemas.ts` /
  `aiCommandProtocol.ts` aligned; `cad_core_tool_whitelist_test` suite.

**SK1 (parametric arcs) — code-complete, tested, pending commit.**
- `add_sketch_arc_angle_dimension` command end-to-end (core + DocumentManager
  + app handler + schema + AI schema).
- **Deterministic `enforce_arc_dimensions` pass** (in
  `private_dimension_relation_sync.inc`, called from
  `refresh_sketch_derived_state` after the line→arc rescue): driving
  `arc_radius` keeps center + endpoint angles and recomputes the circle;
  driving `arc_angle` keeps center + start + radius and moves the end;
  shared H/V-constrained lines are honored by sliding endpoints to the
  circle∩line intersection; dimensions degrade to driven when a fixed
  vertex would have to move. **Why not planegcs:** the solver wanders the
  null space of an unanchored arc (free translate/rotate/sweep) even from a
  zero-residual reference — measured ~1 mm drift per solve.
- **GCS::Arc registration plumbing** (mapping, params, unknowns, ArcRules,
  apply-sync) is in place but gated on `arc_participates_in_solver`
  (returns false until the constraint-completion milestone adds
  arc-referencing solver constraints). Unregistered arcs have their
  vertices pinned during solves so they can't drift.
- **Two latent bugs fixed along the way:**
  1. Stale `pending_append_focus_ids` (cleared only when the solver ran) —
     a later driving dimension could freeze EVERY vertex and
     over-constrain the solve. Now consumed on every refresh.
  2. Arc dims excluded from the solver-eligibility gate (they are
     ad-hoc enforced, so counting them as solver constraints produced
     empty solver systems).
- New suite `cad_core_parametric_arc_test` (6 cases: radius both epsilon
  sides, angle both sides, H-line circle∩line drag + free pivot drag,
  over-constraint → driven, stadium full `profiles_match`, fillet-arc
  exclusion) + new `sketch_move_test` case (arc-radius dim survives move).
- **All 17 suites green, tsc clean.**

## Next milestone

- **SK2 — constraint completion** (symmetric/collinear/midpoint/tangent
  pairs/anchor-t). Arc tangency constraints land here — that is when
  `arc_participates_in_solver` starts returning true and the GCS::Arc
  registration goes live. See plan for file-level detail.

## Just merged

- **IGES import + export** — squash-merged as `c1a5eec` (#67) into `dev`.

## Commit guidance

SK0 and SK1 are two separate commits, each pending user approval + in-app
smoke. SK1 verification checklist: draw an arc, add a radius dimension,
edit it both ways (arc stays on-circle); add an arc-angle dimension;
drag an arc endpoint shared with a line (slides along the H/V line);
undo/redo each operation. After merge of this branch: squash-merge to
`dev`, delete `feature/sketch`, next branch per user.
