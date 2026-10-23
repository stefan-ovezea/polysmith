# Sketch — next session handoff

**Read this first, then the three documents it points at.** Written 2026-10-24 at the end of the session that finished S1, S2, S5–S8, S30/S30b, D-002, D-001f and closed T2.3.

**Branch:** `feature/sketch-refactor` (local only). **The tree is clean** — everything below is committed; `git log --oneline -20` shows the session's work.

---

## 1. Where the plan stands

| Plan item | State |
|---|---|
| **S1** constraint buttons, line flavours, dimension modes | **Done.** Five new constraint buttons (collinear, midpoint, tangent, tangent pair, symmetric) with pick flows, glyphs, badges (`L`/`M`/`T`/`S`) and hints; `DimensionToolMode` cut to the seven sketch modes with real target filtering (`dimensionModeRules.ts`); line-type flavours stay a dropdown (a one-row segmented control pushed "Infinite" out of the 288 px panel — reverted, see CLAUDE.md's UI Layout Rules). |
| **S2** schema/dispatch integrity | **Done.** Unknown commands answer `UNKNOWN_COMMAND` with the request id; `lint:architecture` keeps dispatch and `commands.schema.json` in step both ways; `unknownCommand.core.test.ts` pins the reply and that the loop keeps serving. |
| **S5** loose-ends diagnostic | **Done.** `get_sketch_diagnostics { feature_id }` → open ends + untied pairs (< 0.05 mm) from the profile walk's own vertex table; sketch panel block with counts and a **Tie ends** action (one undo step); the extrude panel points at it when nothing is extrudable. |
| **S6** report destructive cleanup | **Done.** `remove_collapsed_line_records` returns a report and the refresh logs "collapsed line … removed N dimension(s), N constraint(s), N relation(s)". The cleanup is a safety net — the D-003 guard refuses a degenerate drag. |
| **S7** logging hygiene | **Done.** Fifteen `fprintf(stderr)` sites in the sketch core → `log_*("sketch", …)`; lint check 5 rejects raw stdio there. |
| **S8** stop swallowing drive failures | **Done.** `onDriveFailed` threads out of `draftDimensionPostCommit.ts`; the viewport logs a warn entry and shows a message naming the expression. |
| **S30 / S30b** extrude refresh | **Done.** Parameters are copied and only profile fields overwritten (S30); a missing profile degrades to the survivors with a warning instead of `dependency_broken` (S30b, D-007) — which required the general rule that `warning` never suppresses geometry. |
| **T2.1 / T2.3** | **Done**, including the spline split button, arc slots (core + UI) and the degree picker. |
| **D-002** theme alpha | **Fixed** — `themeColorSolid` / `themeColorWithAlpha`; the token sweep proved only the two sketch tokens reached a `THREE.Color`. |
| **D-001f** timeline click cleared the profile selection | **Fixed in the core** — selecting a profile-consuming feature carries its consumed profiles into the selection. |

**Gates at the end of the session:** `pnpm test:core` **83/83**, `pnpm --filter desktop-ui exec tsc --noEmit`, `pnpm lint:architecture` **5/5**, UI suites **106 passed** in the touched areas, `pnpm registry:write` (262 commands).

## 2. Read these, in this order

1. **[Sketch-Tools-Parity.md](Sketch-Tools-Parity.md)** — the SolidCraft↔PolySmith matrix and the session-by-session progress log (§8).
2. **[Sketch-Defect-Log.md](Sketch-Defect-Log.md)** — D-001a–f, D-002, D-003, D-004, D-005 (De Boor direction), D-006 (a new constraint kind wiped the document state), D-007 (`warning` deleted the body), **D-008 (S10's blocker)**.
3. **[Sketch-Completion-Plan.md](Sketch-Completion-Plan.md)** — solver coverage, reach, legibility.

## 3. What is left, in order

1. **S4 — name the conflict (`determined_by`).** The solver already returns `conflicting` / `redundant` / `partially_redundant` as **constraint indices**, and the core reports only counts. The work, in order:
   - Build the missing map `solver constraint index → owner id`. **Every typed `GCS::System::addConstraint*` helper returns the index planegcs assigned** (`int addConstraintP2PCoincident(...)`, `addConstraintP2PDistance`, `addConstraintCircleRadius`, …), so the map must be recorded from that return value at **every** add site in `constraint_solver_geometric_constraints.inc` (~24 calls), never from a running counter — one sketch constraint can add several solver constraints (equal_length → P2PDistance, midpoint → three, tangent pair → two radius pins plus the tangency). Pattern:

     ```cpp
     const auto record_owner = [this](int index, const std::string& owner_id) {
         if (index < 0) return;
         if (constraint_owner_ids_.size() <= static_cast<std::size_t>(index)) {
             constraint_owner_ids_.resize(static_cast<std::size_t>(index) + 1);
         }
         constraint_owner_ids_[static_cast<std::size_t>(index)] = owner_id;
     };
     record_owner(system_->addConstraintP2PCoincident(a, b), sc.constraint_id);
     ```

     Then assert **no holes**: every index in `[0, clist.size())` must have a non-empty owner id, checked once after `build()` (a missed site must fail loudly, not shift ids). Sites in `params.constraints` are the ones that matter first (coincident, concentric, midpoint, symmetric, tangent pair) because that is where a user-visible conflict comes from.
   - Carry `conflicting_constraint_ids` / `redundant_constraint_ids` on `SketchFeatureParameters` and emit them in the viewport payload (the counts are already there).
   - UI: the DOF badge tooltip and a small conflict panel list the determining constraints by kind; then the "refuse an over-constraining edit, naming the determining constraints" half, plus `if_redundant: "driven"` for demotion.
   - Acceptance from the plan: editing a dimension on a fully constrained sketch produces "determined by <kind>" text, with a test locking the wording.
2. **S11 — auto-constrain / fully-constrain** (`auto_constrain_sketch {tolerance, angle_tolerance, dimensions}`), 3 d. **There is no inference pass in the core today** — the core only has per-constraint setters (`set_sketch_line_constraint` for horizontal/vertical, `equal_length`, `perpendicular`, `parallel`, `coincident`, `tangent`, `tangent_pair`, `symmetric`, `midpoint`, `collinear`), and the UI-side snap inference is interaction state that never reaches the core. So the work is:
   - **Candidate detection over the parameter list** (pure, testable): a line within `angle_tolerance` of horizontal/vertical → that constraint; two lines whose lengths agree within `tolerance` → `equal_length`; two endpoints within `tolerance` that are not already the same vertex → `coincident`; a point within `tolerance` of a circle's radius → `point_on_circle` **once D-008 is fixed**; concentric circles whose centres agree → `concentric`; parallel/perpendicular pairs from the angle between their directions.
   - **The `try_add` acceptance rule**: for each candidate, add it, re-solve, keep it **only if `solver_dofs` drops** (use the solver's number, not `count_sketch_dof` — the latter is the heuristic S3 wants to retire), otherwise roll that one candidate back. This is what makes the command safe on a sketch that is already partly constrained.
   - **Then dimension the remaining DOF** when `dimensions` is set: distances from the fixed/selected geometry outward, one per surviving DOF, named by the entities they bound.
   - **One undo step**: the manager pushes undo once around the whole batch (`push_undo_state()` before, `bump_geometry_revision()` after), like the S5 "Tie ends" batch.
   - **Entry points**: a sketch-panel button, the command for AI/scripting, and a **single dismissible offer** when a sketch is finished with DOF > 20 — never an automatic run.
   - **Acceptance from the plan**: a hand-drawn rectangle reaches DOF 0 in ≤ 8 dimensions, and one undo restores it; finishing an unconstrained sketch never changes it without a click.
   - Order: detection + `try_add` + a C++ test (hand-drawn rectangle → DOF 0, undo restores); then the dimensioning pass; then the UI (button + offer, with the offer suppressed per sketch once dismissed).
3. **S3 — one DOF source.** `dof_status_emit.inc` still computes per-entity statuses with the heuristic `count_sketch_dof(...)` while `solver_dofs` comes from the solver, and `SolveResult` has **no per-variable determined information** — planegcs only reports dependent/redundant constraints. This needs the bridge to expose which variables were determined (rank analysis) plus a variable→entity map before the emitter can be switched over.
4. **S10 — point-on-object for curves.** **Blocked, see D-008.** `point_on_circle` was implemented (P2PDistance(center, point) == radius), stored correctly, `solver_ok=1` — and the point did not move while `solver_dofs` stayed 2. Reverted rather than shipped. Next: compare a line endpoint's vs a standalone point's solver parameters, check the appended-focus path for kind `point`, then re-add **with a test that proves the geometry moves**.

## 4. Gates

`pnpm core:build`, `pnpm test:core` (must stay **83/83**), `pnpm --filter desktop-ui exec tsc --noEmit`, `pnpm lint:architecture` (5 checks), `pnpm registry:write` after any command change.

## 5. Environment notes that will waste your time otherwise

- **The app holds `native/cad-core/build/cad_core.exe`.** `pnpm core:build` then fails at the post-build copy; the compile and link succeed. Close the app first, or copy `Release/cad_core.exe` afterwards — a stale copy means you test the old core.
- **The zod schemas strip unknown keys and reject undeclared events.** A new field, enum value or event type must be declared in `apps/desktop-ui/src/lib/schemas/ipc/*` or `ipcSchema.ts`, or the whole reply is dropped. This has now cost time four times: chamfer `mode`, line `line_type`, the `collinear` line relation (D-006), and the `sketch_diagnostics` event (which surfaced as a 15 s command timeout, not an error).
- **A command timeout usually means "the reply failed to parse", not "the core is stuck".** Probe the core directly with a PowerShell pipeline of NDJSON lines before believing a hang.
- **`latestDocument()` can be stale after a sketch edit** — send `get_document_state` before reading. Pattern: `apps/desktop-ui/src/lib/ipc/sketchLineType.core.test.ts`.
- **The core harness is the fastest end-to-end test available.** `apps/desktop-ui/src/lib/ipc/*.core.test.ts` drive the real binary through the real TS builders; they skip when it is missing.
- **The app has no terminal.** C++ diagnostics go through `log_*` (lint check 5 enforces it in the sketch core); UI diagnostics through `addMessage`/`addLogEntry`.
- **Do not rewrite UTF-8 files with PowerShell `Get-Content`/`Set-Content`** — it double-encodes em dashes and arrows (this session's only self-inflicted damage). Use the file tools, or `[System.IO.File]::ReadAllText/WriteAllText` with an explicit `UTF8Encoding($false)`.

## 6. Things deliberately not done

- Line tool parity — out of scope; PolySmith's is better than SolidCraft's.
- Conic, blend curves, G2/regular-polygon constraints, 3D sketch curves — T3.
- Fit points are not stored for fit splines (the entity becomes a control-point spline through them); Fusion-style refinement is a follow-up.
- Editing an arc slot from the panel (centre drags + re-create for now).
- **S3 and S10** — see above; both need a focused session, not a slice at the end of a long one.