# Sketch Completion Plan — Research and Implementation

**Date:** 2026-10-23 · **Scope:** `polysmith` (primary) and `solidcraft` (reference + secondary target)
**Baseline:** PolySmith `dev` @ `cfcae31` (post-refactor), SolidCraft 0.18.1 working tree
**Status:** plan only — this document changes no code.

---

## 0. How this was researched, and what to trust

| Claim type | How it was established |
|---|---|
| PolySmith sketch command surface | handler dispatch in `native/cad-core/src/app/sketch_*_command_handlers.cpp` (+ trim/corner-trim), cross-checked against `protocol/schema/commands.schema.json` and the generated `wiki/Command-Reference.generated.md` |
| Which constraints/dimensions reach the solver | direct reading of `core/sketch/impl/constraint_solver_geometric_constraints.inc`, `constraint_solver_dimension_constraints.inc`, and `impl/private_dimension_relation_sync.inc` |
| SolidCraft sketch surface | **its own built binary's registry**: `solidcraft-cli.exe commands` → 404 commands, 106 of them `sketch.*`. Authoritative, not the README. |
| SolidCraft behaviour | the registry's own `params` documentation + crate sources |
| Effort estimates | calibrated against shipped work in this repo (§7); one developer who knows the codebase, **+30–50 %** if not |

Two wiki pages are **stale and must not be trusted** until rewritten:
`Sketch-Dimension-Implementation-Status.md` (claims arcs have no dimensions — there are three arc dimension commands with tests), and `V1-Roadmap.md` §"What Remains" (still lists arc fillets and arc dimension-drive as missing; both shipped).

---

## 1. Verdict in one page

PolySmith's sketch is **not** a thin implementation. Measured today:

- **19 toolbar tools**, **76 IPC command ids containing `sketch`**, **94 sketch-relevant command types** once trim-preview, corner-trim, array, mirror and transform are counted.
- **14 storable constraint kinds**, **12 dimension kinds**, planegcs natively plus a planegcs-WASM preview in the UI, solver diagnostics, exact-curve profile arrangement, trim/split/extend/corner/fillet/chamfer/offset/mirror/array, parametric projection links, control-point splines, text, and an inference engine at commit.
- **74 C++ test suites / 810 test functions**, of which ~23 files and ~668 assertions cover sketch geometry, profile detection and dimensions.

The gaps are therefore concentrated, and the most important ones are **not** "missing Fusion tools":

### 1.1 Trust — enforcement is genuinely hybrid

**7 of 12 dimension kinds** are planegcs constraints; **5 bypass the solver** (`arc_radius`, `arc_angle`, `arc_length`, `polygon_radius`, `line_line_distance`) and are re-imposed by direct coordinate mutation in `private_dimension_relation_sync.inc` / `dimension_update.inc`. The refresh pipeline is solver-first, then **10+ hand-written `enforce_*` passes** that re-impose constraints by moving coordinates.

That hybrid is defensible as a fallback — and the source says so — but today some kinds *only* work that way, so:
- the solver's rank does not know those dimensions exist, so "fully constrained" can be wrong;
- two dimensions can fight without a conflict report;
- trim-transfer, undo and update each special-case them.

DOF is also published by **two independent mechanisms** — planegcs rank (`solver_dofs`) and a hand-rolled per-entity counter (`dof_counter.cpp` with hardcoded costs) — and the badge prefers one while entity colouring uses the other.

Over-constraint is **detected but never explained**: planegcs reports conflicting/redundant constraints, the app ships only the *count*, and the per-entity DOF painter is an **empty function** (`viewportVisualState.ts:201`) that four call sites still call.

### 1.2 Reach — features exist that no user can reach

This is the biggest surprise in the research and it is cheap to fix:

- **6 constraint kinds have no toolbar button**: tangent, concentric, symmetric, midpoint, collinear, tangent-pair. Four of them (symmetric, midpoint, collinear, tangent-pair) are reachable **only through the AI command channel** — no toolbar, no pointer path, no builder in `sketchCommands.ts`. They are implemented and tested (`constraint_completion_test.cpp`).
- **10 of 17 dimension modes are literal `break` no-ops**, and `aligned` has no implementation block at all. Both dropdowns expose 7 of 17.
- **5 commands are handled and TS-built but absent from `commands.schema.json`** (`add_sketch_arc_length_dimension`, `merge_coincident_sketch_points`, `get_sketch_profiles`, `select_sketch_profiles`, `select_sketch_profiles_rect`). The schema is the stale artefact.
- `app.cpp`'s dispatch has **no fallback branch**, so a drifted command produces no event at all and the UI's awaited promise never settles.

### 1.3 Legibility — the sketch is usable but not readable

No "look at sketch", no slice, no constraint-glyph toggle, no per-snap-type cursor pictogram, no object-snap tracking guides, no dimension auto-layout, no conflict list, and **no answer to "why won't this profile close?"** — the model knows which endpoints are unshared, it never says so.

### 1.4 Creation-mode parity is closer than it looks

The circle tangent modes are **implemented** (`pointerUpActiveSketch.ts:589-654`, `draftCommit.ts:523`, `circle_modes_test.cpp` with 9 tests) — but a stale comment in `types/geometry/sketch.ts:640` says "selecting them has no effect yet", and their hint strings are hardcoded English. So the real work is finishing and de-staling, not building.

The modes that are genuinely absent: rounded rectangle, tangent arc, four of five slot modes, fit-point splines, a standalone point entity, chamfer distance-angle, and any auto-constrain pass.

### 1.5 Recommendation

Three phases, **~8.5 weeks of item effort** for one developer (≈ 2.3 + 3.6 + 2.5), plus ~1 week of continuous chores:

| Phase | Theme | Why first |
|---|---|---|
| **P0 · ~2.5 weeks** | Reach and trust: surface the unreachable commands, one DOF source, named conflicts, loose-ends, schema/dispatch integrity | Highest value per day in the whole plan; most of it is wiring, not new geometry |
| **P1 · ~3.5 weeks** | Creation modes and the solver contract | Closes the Fusion-parity gap users hit while drawing |
| **P2 · ~2.5 weeks** | Legibility: look-at/slice, dimension layout, snap pictograms, conflict panel, theme tokens | Turns a working sketch into a pleasant one |
| **continuous** | `ViewportPanel.tsx` split (6 223 lines), logging hygiene, docs | Pays for itself in every later sketch change |

The plan is deliberately larger than one sprint. If only one branch gets done, make it **P0** — it is ~11 days of work that makes most of what already exists reachable and honest.

---

## 2. Measured inventory

### 2.1 PolySmith

| Dimension | Value | Where |
|---|---:|---|
| Toolbar tools | **19** | `layout/header/SketchToolbar.tsx:69-102` |
| Tools with a hotkey | 7 (line, rectangle, circle, dimension, extend, trim, project); `move` has a binding but no button | same + `config/types.ts` |
| Constraint buttons on the toolbar | **8** of 14 kinds | `SketchConstraintControls.tsx:5-17` |
| Sketch IPC ids | 76 in the generated reference; **94** sketch-relevant dispatched types | `app/*.cpp`, `commands.schema.json` |
| Storable constraint kinds | **14** — H, V, parallel, perpendicular, equal_length, collinear, tangent(line-line), tangent(line-circle), coincident, concentric, midpoint, tangent(circle-circle), tangent(arc-arc), symmetric | `constraint_solver_geometric_constraints.inc` |
| …mapped to planegcs | **12** (collinear, midpoint and tangents as compound constraints) | same |
| …driven-sync only | **2** — `symmetric` (planegcs has no mirror primitive, stated in-source) and the parametric `t` of point-line anchors | `constraint_completion_enforcement.inc`, `state_and_create.inc:177-190` |
| Dimension kinds | **12** | `sketch_dimension_types.h` + kind literals |
| …mapped to planegcs | **7** — line_length, line_angle, circle_radius, angle, point_distance, circle_center_distance, circle_line_distance | `constraint_solver_dimension_constraints.inc` |
| …driven-sync only | **5** — arc_radius, arc_angle, arc_length, polygon_radius, line_line_distance | `private_dimension_relation_sync.inc` |
| Dimension tool modes | **17** declared, **7** exposed, **10** no-ops, `aligned` unimplemented | `types/geometry/sketch.ts:655-672`, `dimensionToolPicking.ts:507-552` |
| C++ suites / test functions | 74 / 810 | `native/cad-core/tests/` |
| Sketch test files / assertions | 23 / ~668 `expect()` | same |
| Sketch IPC-layer test coverage | **none** — IPC tests exist only for CAM | — |
| Largest UI file with sketch logic | `layout/ViewportPanel.tsx` **6 223 lines** | — |
| Next largest | `snapResolution.ts` 2 049 · `draftCommit.ts` 995 · `dimensionToolPicking.ts` 900 · `dimensionLabelDrag.ts` 832 · `dimensionRelationPreviewGeometry.ts` 728 · `sketchMoveTool.ts` 707 · `pointerUpActiveSketch.ts` 696 | — |
| Second solver | planegcs compiled to WASM in the UI (`planegcsBridge.ts` 531, `speculativeSolve.ts` 391), used by snaps and drag preview | — |

Dimension geometry is emitted **core-side**: `sketch_angle_dimension_primitive.inc` 384 lines, `sketch_radial_dimension_primitives.inc` 362, `sketch_primitives.inc` 322 — and re-implemented in TypeScript for previews (§F11).

### 2.2 SolidCraft (authoritative, from its own registry)

**106 `sketch.*` commands**, 32 `ConstraintKind` variants, a two-stage solver, and a palette that is a much better model of *sketch legibility* than PolySmith's.

| SolidCraft behaviour | What it buys | PolySmith today |
|---|---|---|
| `sketch.dimension` **refuses** a dimension the sketch already determines, naming it (`determined_by: ["k3 (Vertical)", …]`), with `if_redundant: "driven"` / `"refuse"` | Over-constraint becomes a message, not a mystery | counted only |
| `sketch.dimension` `scale_sketch` — first length dimension scales the sketch instead of distorting it | "I drew it at 10, I meant 12" | absent |
| `sketch.loose_ends` + `loose_ends.tie`, and the palette shows `open_ends` / `untied_ends` **with a Tie button** | Answers "why won't my profile close?" at the moment it matters | absent |
| `sketch.auto_constrain` / `finish_auto_constrain` — each trial kept only if DOF drops | Rough sketch → fully constrained | absent |
| `sketch.constrainer` — applies the constraint the geometry *nearly* has | One click for H/V, tangent, parallel | inference at commit only |
| `sketch.glyphs` returns per-constraint `failing` / `redundant` / `determined_by` | A conflict UI is a rendering job, not an analysis job | ids not surfaced |
| Palette: 10 toggles, line type (normal/construction/centerline/infinite), per-family variant buttons, DOF caption, Look At, Slice | Everything about the sketch in one panel | modes scattered in the floating tool panel |
| Over-constraint is **refused at the edit**, so there is no "over-constrained colour" to invent | A simpler visual contract | red glyphs/badges, no explanation |
| `sketch.snap {at, from, radius}` — snapping as a queryable command | Snapping is testable and scriptable | UI-side only |
| `sketch.conic`, `sketch.blend_curve` (G1–G4), G2 `Smooth` constraint, `Polygon` constraint | Class-A sketching | absent — correctly out of scope here |

Solver: constructive decomposition (Owen / Fudos–Hoffmann) to place points and rigid clusters on the branch the sketch already has, **then** LM, with pinned-variable elimination and Jacobian rank for DOF; over-constraining edits refused; drags re-solve per frame with continuation steps. Its remaining sketch debt (ROADMAP M1) is three named workflow repairs: mixed offset junctions, legacy natural-spline seams, damaged legacy wire links lacking frame provenance.

### 2.3 Parity map — where PolySmith is short

Everything not listed has a working PolySmith equivalent.

| Fusion / SolidCraft capability | PolySmith status | Cost |
|---|---|---|
| Circle 2-tangent / 3-tangent | **implemented**; stale doc + English-only hints + no UI test | 0.5–1 d |
| Tangent arc | missing | 1–2 d |
| Rounded rectangle | missing | 2 d |
| Slot: center-point, overall, 3-point arc, center arc | missing (1 of 5) | 4 d |
| Fit-point spline, degree-5 spline | missing (control-point, degree ≤ 3) | 2–3 d |
| Standalone point entity | missing | 2 d |
| Chamfer distance-angle | missing (equal, two-distance present) | 1 d |
| Line types: centerline, infinite | missing (construction toggle only) | 1 d |
| Tangent / concentric / symmetric / midpoint / collinear / tangent-pair **buttons** | **no UI affordance** | 1 d |
| AutoConstrain / fully-constrain | missing | 3 d |
| Constrainer (apply the near constraint) | missing | 1.5 d |
| Loose-ends diagnostic | missing | 1.5 d |
| Dimension mode filtering (linear/aligned/angular/radius/diameter/arc_length) | 10 modes no-op, `aligned` unimplemented | 3 d |
| Constraint-glyph show/hide | badges always on | 0.5 d |
| Look At / Slice | missing | 1.5 d |
| Insert SVG as sketch geometry | missing (DXF import exists) | 1 d |
| Conic, blend curves, G2, polygon constraint | missing | defer |
| 3D sketch curves (project-to-surface, intersection, isoparametric, spun) | missing | defer — needs surfaces |
| Fit curves to mesh section | out of scope | — |

**PolySmith-only strengths to protect** (nothing in this plan should regress them; `pnpm test:core` is the guard): core-resolved marquee (`select_sketch_rect`), trim-stroke painting, corner trim, merge-coincident-points heal, parametric projection links, split with constraint transfer, the draft-dimension system, text on path, the AI command bar, and the exact-curve profile arrangement.

---

## 3. Findings that drive the plan

**F1 · Hybrid dimension enforcement (trust, high).** 5 of 12 dimension kinds have no planegcs branch; `constraint_solver_dimension_constraints.inc` contains no `arc_*`/`polygon`/`line_line` case, while `private_dimension_relation_sync.inc` does the work. Note the misleading comment at `sketch_dimension_create_commands.inc:320` claiming arc length is enforced "via planegcs addConstraintArcLength" — **no such call exists anywhere in the tree**, although the planegcs class does (`third_party/planegcs/planegcs/Constraints.h` → `ConstraintArcLength`).

**F2 · Symmetric cannot be solver-mapped (trust, medium).** planegcs has no mirror-symmetry primitive; the code says so and enforces it directly. Any "put everything in the solver" ambition must exempt this kind, or express it as two `PointOnPerpBisector` + midpoint constraints.

**F3 · Two DOF sources (trust, high).** `dof_counter.cpp` computes per-entity DOF from hardcoded costs (line 4, circle 3, arc 6, point 2) with an O(n²) shared-endpoint pass; the solver computes rank. `SketchDofBadge.tsx:20-25` silently prefers one and falls back to the other.

**F4 · Over-constraint counted, not named (trust, high).** `solver_conflicting_count` / `solver_redundant_count` are shipped; the constraint ids are not. Per-entity over-constrained is effectively never painted because `paintDofStatusColors()` is empty while its four call sites remain.

**F5 · Features with no reachable UI (reach, high, cheap).** Four constraint commands are AI-channel-only; ten dimension modes are `break` no-ops; five commands are missing from the JSON schema; `app.cpp`'s dispatch chain has no fallback, so a drifted command hangs the UI promise.

**F6 · Silent destruction (trust, high).** When the solver yields a zero-length line, `state_and_create.inc` deletes the line **and every constraint, dimension, relation, fillet and anchor referencing it**, with only an `fprintf`-level trace. A user can lose a dimension without being told.

**F7 · Diagnostics bypass the logging rule (hygiene, medium).** Eight `fprintf(stderr, …)` sites in the sketch core violate the repo's own "all diagnostics go through the structured logger" rule, so solver failures and enforcement failures never reach the in-app Logs panel.

**F8 · Swallowed errors in the UI (trust, medium).** `updateDimensionExpression` ends in `.catch(() => {})`; a rejected drive is invisible.

**F9 · "Profile not found" has no diagnostic (UX, high value-per-day).** `CLAUDE.md` names profile detection the most regression-prone subsystem and prescribes `PS_TRACE_FACES`; there is no user-facing equivalent of SolidCraft's open/untied ends.

**F10 · Duplicated layout math (architecture, medium).** Dimension frames, extension lines, arrows and label placement exist in C++ (five emitters, 1 000+ lines) **and** in TS (`dimensionRelationPreviewGeometry` 728, `dimensionRelationPreviewSearch` 592, `dimensionLabelDrag` 832, `linearDimensionPlacement` 181), with no parity test.

**F11 · Snap has two owners (architecture, medium).** Core emits `snap_candidates`; `snapResolution.ts` (2 049 lines) also computes them and merges. Four different snap radii are in play (`tolerance_px` 10 px, `SKETCH_SNAP_DISTANCE` 2.5 mm, `SKETCH_SNAP_DISTANCE_PX` 20, `GRID_SNAP_SCREEN_DISTANCE_PX`).

**F12 · `ViewportPanel.tsx` is 6 223 lines (architecture, high friction)** — the same failure the refactor just fixed in `app.cpp` (282 commands → 19 units).

**F13 · Model duplication inside the sketch (architecture, medium).** Entities cache coordinates that also live in `SketchVertex`, reconciled by three ordered passes; this is the structural home of "ghost movement" bugs.

**F14 · No IPC-level test for sketch (process, high).** Which is exactly why F5's schema drift survives a 810-test suite.

**F15 · Docs drift (process, medium).** Two wiki pages describe a sketch that no longer exists, and one code comment contradicts the code in the opposite direction (says tangent circles do nothing; they work).

**F16 · Editing a sketch silently corrupts its linked extrude (trust, high — confirmed 2026-10-23).** Two symptoms, one family: (a) a diameter edit rebuilds the extrude with `side1.distance` reset to the 10 mm sentinel and `operation` reset from `join` to `new_body`, so the body halves in thickness and the three profiles split into three bodies; (b) deleting a circle takes the whole body with it. Root cause of (a) is `private_linked_extrude_refresh.inc:96` replacing the entire parameter struct and restoring only `mode` and `target_body_id`. Full evidence, measurements and fix shape in **[Sketch-Defect-Log](Sketch-Defect-Log)** D-001. This outranks most of P0: it damages a model during an ordinary edit, silently.

---

## 4. Workstreams

### W1 · Reach and trust (P0)

**S1 · Finish the quarter-built surface: constraint buttons, line types, dimension-mode filtering** — *3 d*
Why: F5 — the largest value-per-day item in the plan, and it is wiring, not geometry.
What: (a) add toolbar buttons for tangent, concentric, symmetric, midpoint, collinear, tangent-pair, routed through the existing commands; (b) give the line type a segmented control — Normal / Construction / Centerline / Infinite — as SolidCraft's palette does, with centerline and infinite as real curve flags (`SketchLine` already carries `is_construction` and `constraint`); (c) make the 7 sketch dimension modes real — `radius`/`diameter` accept only circular entities, `angular` only two lines, `arc_length` only arcs, `linear` gets H/V projection, `aligned` = today's auto; (d) delete the 10 drawing-sheet modes from `DimensionToolMode` and from both dropdowns (they belong to the drawing workspace, not the sketch).
Files: `SketchConstraintControls.tsx`, `SketchToolbar.tsx`, `SketchToolPanel.tsx`, `dimensionToolPicking.ts`, `types/geometry/sketch.ts`, new `horizontal_distance`/`vertical_distance` kinds in `sketch_dimension_create_commands.inc` + emit + create handlers.
Acceptance: every command in `commands.schema.json` that names a sketch constraint or dimension is reachable from the UI or explicitly marked AI-only.

**S2 · Schema and dispatch integrity** — *1 d*
Why: F5, F14.
What: add the five missing commands to `commands.schema.json`; add a fallback branch in `app.cpp`'s `handle_command_line` that answers unknown commands with an error event carrying the real id (so promises settle); add a test that every dispatched `command.type` string appears in the schema and vice versa.
Acceptance: the new test fails on today's tree for exactly the five known names, passes after.

**S3 · One DOF source** — *1.5 d*
Why: F3. What: when a constraint system exists, per-entity status derives from the solver's rank/determined-variable analysis; `dof_counter.cpp` survives only for sketches with no constraints. Emit `dof_statuses[].status` and `solver_dofs` from the same pass (`dof_status_emit.inc`). Remove the badge's silent fallback, or label which source it used.
Acceptance: `solver_dofs == 0` ⇒ every entity status is `full`; a test asserts it.

**S4 · Name the conflict** — *2.5 d*
Why: F4 + SolidCraft parity. What: carry conflicting/redundant constraint **ids** into viewport state; refuse (not silently accept) an edit that would over-constrain, naming the determining constraints the way SolidCraft's `determined_by` does; add `if_redundant: "driven"` so the caller can demote instead.
Files: `constraint_solver_*.inc`, `viewport_state.h`, `protocol/impl/viewport_to_payload_*`, `dimension_update.inc`, new `SketchConflictPanel.tsx`, `SketchDofBadge.tsx` (tooltip → list).
Acceptance: editing a dimension on a fully constrained sketch produces "determined by <kind>" text; a test locks that wording.

**S5 · Loose-ends diagnostic** — *1.5 d*
Why: F9. What: `get_sketch_diagnostics` returning `open[]` and `untied[]` from the vertex table the profile walk already builds; render it in the sketch panel as SolidCraft does (warning lines + a "Tie ends" action) and surface it in the extrude panel when no profile is found.
Acceptance: two lines 0.02 mm apart are reported as `untied` before extrude refuses.

**S6 · Report destructive cleanup** — *1 d*
Why: F6. What: when the zero-length-line cleanup deletes referencing constraints/dimensions, emit a structured warning naming the count and kinds ("1 dimension removed with a line that collapsed"), and make it undoable as part of the same step.
Acceptance: a test asserting the warning and that undo restores both the line and the dimension.

**S7 · Logging hygiene in the sketch core** — *0.5 d*
Why: F7. What: replace the eight `fprintf(stderr, …)` sites in `state_and_create.inc` with `log_warn`/`log_debug` under a `sketch` tag; add an architecture grep (`pnpm lint:architecture`) that rejects `fprintf(stderr` under `core/sketch/`.
Acceptance: `grep -r "fprintf(stderr" native/cad-core/src/core/sketch` returns nothing.

**S8 · Stop swallowing drive failures** — *0.5 d*
Why: F8. What: `draftDimensionPostCommit.ts` replaces `.catch(() => {})` with a structured `addMessage` + `log_warn`, and surfaces the failing dimension.

**S30 · Stop the linked-extrude refresh corrupting the feature** — *~2 h + test, now the top P0 item*
Why: F16 / D-001b — confirmed, measured, root-caused. A 0.29 mm diameter edit halves the body and splits it into three, with no warning.
What: in `private_linked_extrude_refresh.inc`, copy the existing `ExtrudeFeatureParameters` and overwrite only the profile-derived fields instead of constructing a fresh struct and restoring `mode`/`target_body_id`. That preserves `side1`, `side2`, `extent_mode`, `operation`, `thin`, tapers, offsets and extents — the whole set that is silently defaulted today. Then revisit the `default_side` sentinel in `extrude_helpers.cpp:184-192`, which exists only to paper over this path.
Test: assert `side1.distance` survives an `update_sketch_circle`, and that mesh volume changes by the hole area only (≈ 0.32 cm³ for the reported case), never by half. The measurement harness already exists (D-001b table).

**S30b · Profile references must degrade, not vanish** — *1–2 d*
Why: F16 / D-001a — deleting one circle takes the whole body with it.
What: when a stored profile id stops resolving, re-derive the region from the surviving boundary (same outer loop, fewer holes) before falling back to `dependency_broken`, and say what changed ("circle A was deleted — the extrusion now has one hole"). The repro is headless and file-free (D-001's 7-command recipe); write the fail-before test next to `sketch_profile_test.cpp` with `profiles_match`.

### W2 · The solver contract (P1, first half)

**S9 · Dimension kinds through planegcs** — *3 d*
Why: F1. What, per kind — **four of the five** (see §6.1; `polygon_radius` deliberately stays):

| kind | mapping (classes available in vendored planegcs) |
|---|---|
| `arc_radius` | `P2PDistance(center, start)` |
| `arc_angle` | `AngleViaPoint(center, start, end, apex)` |
| `arc_length` | `ConstraintArcLength` — **exists, currently unused, and a comment already claims it is used** |
| `line_line_distance` | `P2LDistance(point on A, B)` + existing `Parallel` |

`polygon_radius` remains driven-sync and is listed as the one known exception in the coverage test — a regular polygon's radius would otherwise have to be expressed as centre-to-vertex distance plus equal sides, which is two constraints pretending to be one.
Keep the direct-drive path behind a flag for one release; migrate old documents on load (re-solve to stored values; if residual > 1e-6, keep direct-drive for that dimension and log once).
Acceptance: a **solver-coverage test** enumerates every dimension kind and asserts each maps to a constraint or appears in the exception list. New kinds cannot silently bypass the solver again.

**S10 · Point-on-object for curves** — *2–3 d*
Why: a vertex can be anchored to a **line** (`SketchVertexLineAnchor`) but not to a circle, arc or spline. What: map to `P2PDistance(center, p) = r` for circles/arcs and `ConstraintPointOnBSpline` for splines; add snap + commit inference. Test both epsilon signs of the tangency, per the repo's tangency rule.

**S11 · Auto-constrain / fully-constrain** — *3 d*
Why: F5 + SolidCraft's most impressive sketch command. What: `auto_constrain_sketch {tolerance, angle_tolerance, dimensions}` — infer, then dimension remaining DOF, keeping a candidate only if DOF drops (SolidCraft's `try_add` rule is the right one). One undo step. Entry points, per §6.3: a **sketch-panel button**, the command itself for AI/scripting, and a **single dismissible offer** when the user finishes a sketch with DOF > 20 — never an automatic run.
Acceptance: a hand-drawn rectangle reaches DOF 0 in ≤ 8 dimensions and one undo restores it; finishing an unconstrained sketch never changes it without a click.

### W3 · Creation-mode completion (P1, second half)

**S12 · Finish the tangent circles** — *1 d*
Why: F15 — they already work, the docs and hints do not. What: remove the stale "has no effect yet" comment; move the three hardcoded English hints (`"Tangent circle: click to place the circle"`, `…${n}/${max} lines selected`, `…click on a line`) into `i18n/en.json`; add the missing UI test.
Acceptance: selecting both modes from the split button gives a visible pick flow with translated hints.

**S13 · Rounded rectangle** — *2 d*
What: 2-point rectangle + 4 fillets + one radius dimension as **one undo step**; SolidCraft builds it in a scratch session and commits only on success — do the same via the existing fillet internals rather than duplicating corner math.
Risk: the zero-length cleanup deletes fillet records that reference a consumed line. Keep corners as separate lines (safe), or fix the fillet-consume case first — that is the "Future work" item recorded in `.deepseek/active-task.md`, and it is not scheduled in this plan.

**S14 · Point entity** — *2 d*
What: `add_sketch_point`, cross-rendered, selectable, constrainable, serialized, plus a hole-feature input that accepts a sketch point id. Cheapest route to Fusion's "hole at a sketch point".

**S15 · Chamfer distance-angle** — *1 d*
What: `SketchChamfer.mode: equal | two_distance | distance_angle` (two-distance already exists as `distance_a`/`distance_b`), third split-button mode, schema.

**S16 · Fit-point spline + degree 5** — *2–3 d*
What: `mode: "fit" | "control"` on `add_sketch_spline`; least-squares fit to a clamped B-spline (the profile sampler already exists in `spline_math.h`); allow degree up to 5. Keep sampling identical for both modes so `sketch_profile_exact.inc` is untouched — any change there requires full `pnpm test:core` per CLAUDE.md.

**S17 · Slot family: the two straight modes** — *1 d*
What: `center_point` and `overall` as parameterisations of the existing slot — same geometry, different pick sequence and dimension meaning (span vs centre distance). Keep the existing payload working (default `center_to_center`).
**S17b · Arc slots (deferred)** — *3 d, own branch*: two concentric arcs plus two straight tangent sides needs a new entity, serialization, rendering, trim/split behaviour and profile integration. Deferred per §6.2 because it touches `sketch_profile_exact.inc`, the repo's most fragile file.

### W4 · Legibility (P2)

**S18 · Look At + Slice** — *1.5 d*
What: `look_at_sketch` sets the camera normal to the active plane (reuse the view-cube face-snap path; SolidCraft's second press flips to the other side — worth copying) and `slice` clips at the sketch plane. Both in the sketch panel, both session-persistent.

**S19 · Theme-token the DOF path** — *0.5 d*
Why: F4 + the repo's theme rule. What: add `--cad-sketch-under|full|over` to every theme JSON, consume in `viewportVisualState.ts` and `SketchDofBadge.tsx` (which hardcodes five hexes and builds English strings inline). Delete the empty `paintDofStatusColors()` or give it the job.

**S20 · Constraint glyphs: toggle + density + dimming** — *1.5 d*
What: show/hide (SolidCraft's `sketch.glyphs`), collapse to a count badge below a zoom threshold, and dim redundant glyphs (SolidCraft reports `redundant` per constraint; use it).

**S21 · Snap pictograms + tracking guides** — *3 d*
What: per-type cursor glyphs (square endpoint, triangle midpoint, circle centre, ⟂, ⌒), a snap radius ring, and Fusion-style alignment tracking guides (hover an endpoint, get dashed H/V guides; the draft snaps to their intersection). The guide design is already written in `wiki/Sketch-Selection-Controls.md` §8. Also reconcile F11's four snap radii into one documented constant.

**S22 · Dimension auto-layout** — *5 d*
Why: F10 + real pain on 40-dimension fixtures. What: tier collinear dimensions at consistent offsets, push text off geometry with a small repulsion, prefer the free side (SolidCraft's `chain_centres`/`default_text` is the reference behaviour), and keep text screen-constant. Put it in **one** place — recommend C++ next to the emitters — and delete the TS duplication once parity tests pass (S23).
Risk: touches four 8–17 KB emitters plus label drag. Golden-test emitted primitives before and after.

**S23 · Dimension-geometry parity test** — *1 d*
A fixture sketch, C++ primitives serialized to JSON, snapshot test; plus a TS test that preview geometry for the same input lands within tolerance. Prerequisite for S22.

**S24 · Sketch inspect** — *1 d*
What: `inspect_sketch` → entity counts by kind, constraint counts, dimensions driving/driven, DOF, profile count, and **the ids of dimensions not backed by a solver constraint** (makes S9's gap permanently measurable).

### W5 · Architecture (continuous)

**S25 · Split `ViewportPanel.tsx`** — *3 d*: extract, one PR each, the draft-session controller, dimension-placement controller, sketch pointer routing, context menus, overlays. Target: nothing above ~1 200 lines.
**S26 · Delete the legacy snap fallback** — *~1 h*: the core already owns the static key points; `snapResolution.ts` keeps three small builders (~150 lines) that run only when `snap_candidates` is absent. Confirm every document path emits them (load, undo, new sketch), then delete the fallback. Per-move dynamic snaps stay UI-side by design (latency; CLAUDE.md).
**S27 · Reconcile entity/vertex model** — *design only*: F13 is the structural source of ghost movement; do not start it before S9 lands.
**S28 · ADR-0002 correction** — *0.5 d*: mark Superseded with the measured reason (≈1.4 MB state + 1.6 MB viewport refetch per commit) and open the "send previewed coordinates, core verifies" ticket.
**S29 · Rewrite the stale sketch docs** — *1 d*: `Sketch-Dimension-Implementation-Status.md` → the real 14/12 kind matrix; `V1-Roadmap.md` "What Remains"; `Home.md` link; fix the three misleading code comments (tangent circles, arc length, arc dimensions).

### W6 · SolidCraft (secondary)

Its sketch is in better shape than its ROADMAP implies; M1's remaining items are workflow repairs, not features.

**C1 · Mixed offset junctions + legacy natural-spline seams** — *2–4 d*: the two named M1 defects; correctness, not features.
**C2 · Damaged legacy wire links lacking frame provenance** — *2 d*: a repair/migration path, analogous to PolySmith's merge-coincident-points heal.
**C3 · Conflict explanation in the palette** — *1–2 d*: `sketch.glyphs` already returns `failing`/`redundant`/`determined_by`; make the caption clickable to select the offending entity.
**C4 · Take PolySmith's interaction ideas — re-implement, never copy**: core-resolved marquee semantics, trim-stroke painting, and the draft-dimension badge model. Design note only; AGPL → MIT/Apache is not a legal direction.

> **Licence guardrail.** SolidCraft (Apache-2.0/MIT) may flow into PolySmith (AGPL) with attribution; nothing flows the other way; SolidCraft's own AGENTS.md forbids reading GPL sources. Keep the two checkouts in separate sessions when writing solver or naming code.

---

## 5. Sequencing

### P0 — Reach and trust (≈ 2.5 weeks, 11.5 item-days)
`S2 → S1 → S3 → S5 → S6 → S8 → S4 → S7`

Rationale: S2 makes drift impossible to repeat and unblocks the rest; S1 delivers the most visible capability for three days; S3/S5/S6 are small trust fixes; S4 is the deep change and benefits from S3's single DOF source; S7 is a half-day cleanup that can ride along.

Exit criteria:
- Every sketch command is reachable or explicitly AI-only, and a test enforces the schema↔dispatch match.
- Badge and geometry agree on a fully constrained fixture.
- A conflicting edit names what determines it.
- An unclosed sketch reports its open/untied ends instead of only "profile not found".
- No `fprintf(stderr` under `core/sketch/`.

### P1 — Solver contract and creation modes (≈ 3.5 weeks, 18 item-days)
`S12 → S13 → S15 → S14 → S17 → S16 → S11 → S10 → S9`

Rationale: cheap parity first (S12–S17b is 1 d and completes the slot family's straight half), then the point entity, then auto-constrain (which needs S3's DOF truth), then the constraint/dimension contract (S10, S9). S9 last so it lands on a calm tree with S24's inspect and the coverage test in place.

Exit criteria:
- The solver-coverage test fails when a dimension kind is added without a mapping.
- Auto-constrain reaches DOF 0 on a hand-drawn rectangle, undoable in one step.
- Rounded rectangle extrudes to a valid profile.

### P2 — Legibility (≈ 2.5 weeks)
`S19 → S18 → S20 → S23 → S21 → S22`

Rationale: S19 is half a day and removes a rule violation; S18/S20 are contained; S23 is the safety net before the larger S22; S21 changes the snap surface and wants a calm week.

Exit criteria:
- No hardcoded hex in the sketch render path.
- Snap type is identifiable from the cursor glyph alone (blind test, 5 types).
- A dense fixture sketch shows no overlapping dimension text.

### Continuous
**S25** one extraction per week; **S26** after P1; **S28/S29** early (cheap, and they stop the next session being misled).

### Execution — branch `feature/sketch-refactor`

Branched from `dev` @ `cfcae31` on 2026-10-23. P0 runs on this branch, one commit per item, each gated by §9.

| # | Item | State | Gate |
|---|---|---|---|
| 0 | **S0 fix the red spline-extrude suite** (found while gating S2) | **open — blocks the P0 "all suites green" criterion** | `cad_core_spline_test` passes |
| 1 | S2 schema + dispatch integrity | **done, uncommitted** | `pnpm lint:architecture` check 3 passes; unknown command answered with `UNKNOWN_COMMAND` carrying the real id (verified against the built binary) |
| 2 | S1 constraint buttons, line types, dimension modes | pending | every constraint/dimension reachable or marked AI-only |
| 3 | S3 one DOF source | pending | `solver_dofs == 0` ⇒ all entity statuses `full` |
| 4 | S5 loose-ends diagnostic | pending | unclosed sketch reports open/untied ends |
| 5 | S6 report destructive cleanup | pending | undo restores line + dimension with a warning |
| 6 | S8 stop swallowing drive failures | pending | failed drive appears in the Logs panel |
| 7 | S4 name the conflict | pending | refusal text names the determining constraint |
| 8 | S7 logging hygiene | pending | no `fprintf(stderr` under `core/sketch/` |
| 9 | **S30 linked-extrude refresh corruption** (D-001b) | **done, uncommitted** — refresh copies the existing parameters and overwrites only profile fields; regression test `extrude_refresh_test` (fail-before verified); 74/75 suites green (the red one is S0) | `side1.distance`, `operation` and `sketch_edge_ids` survive a sketch edit; volume/bbox unchanged |
| 10 | **S30b profile references degrade** (D-001a) | **open — repro confirmed headless** | deleting a circle keeps the surviving region and explains the change |
| 11 | D-002 theme alpha → `THREE.Color` (defect log) | open, cosmetic | warning gone, label opacity honoured |
| 12 | **UI error capture + devtools** (no-issue, from D-001c) | **done, uncommitted** — `log_ui_error` / `open_devtools` in the Rust shell, `uiErrorReporting.ts`, boundary writes the real component stack and renders in dev | a render crash leaves `%APPDATA%\polysmith\ui-errors.log` and a readable screen |
| 13 | **Shape guards for degenerate profile loops** (D-001c guard) | **done, uncommitted** — `shapeFromProfileLoops`, profile fill, `polygon_extrude` | a malformed profile can no longer blank the window |

S2 in detail (done):
- `commands.schema.json` gained the **7** commands dispatch accepts but the schema rejected: the five sketch ones (`add_sketch_arc_length_dimension`, `merge_coincident_sketch_points`, `get_sketch_profiles`, `select_sketch_profiles`, `select_sketch_profiles_rect`) plus two CAM ones the check found that the inventory had missed (`cam_capture_edge_reference`, `cam_capture_point`).
- `handle_command_line` now returns `bool`; `run()` answers an unclaimed command with a logged error and an `UNKNOWN_COMMAND` error event carrying the real id, instead of falling through silently.
- `pnpm lint:architecture` grew a third check that diffs dispatch against the schema **in both directions**. It scans globally (not block-split), so `||` chains and `!=` early-return guards such as `trim_preview` are seen — the two blind spots that let this drift happen.
- `IPC-Protocol.md` documented the four commands that had no entry (`get_sketch_profiles`, `merge_coincident_sketch_points`, `select_sketch_profiles`, `select_sketch_profiles_rect`).

**S0 — pre-existing failure found while gating S2 (2026-10-23).** `pnpm test:core` on this branch: **73 of 74 suites pass**; `cad_core_spline_test` fails deterministically at `test_spline_extrude_smoke` with `EXCEPTION: Extrude preview failed. Adjust the extent, taper, or source profile.` The suite has been in the tree since `cf39375` (2026-08-28) and there is no record of it being red, so this is likely a later regression in spline-profile extrude — exactly the fragile area §8 says not to touch casually.

It is **not** caused by S2: the test drives `DocumentManager::add_sketch_spline` / `extrude_profile` / `compile_bodies` directly (no IPC dispatch, no schema), and the S2 diff contains no geometry or sketch file. Worth fixing **before** P1, because P1 adds a fit-point spline mode that shares this profile path, and because "all suites green" is the P0 exit criterion. Diagnosis starts with `PS_TRACE_FACES=1` per CLAUDE.md.

---

## 6. Decisions taken (2026-10-23)

The six open questions were answered with the recommendations below; the workstreams and estimates in §4–§5 were updated accordingly.

| # | Question | Decision | Consequence |
|---|---|---|---|
| 1 | Which dimension kinds move into planegcs | **Four — `arc_radius`, `arc_angle`, `arc_length`, `line_line_distance`. `polygon_radius` stays driven-sync.** | S9 drops to 3 d. Polygon radius is the only indirect mapping (two constraints pretending to be one) and the least common in printed parts; leaving it keeps one known, contained exception, and the coverage test in S9 records it explicitly. |
| 2 | Arc slots now or later | **Two straight slot modes now (`center_point`, `overall`); arc slots deferred.** | S17 drops to 1 d. Arc slots are a new entity plus profile-detection changes — the riskiest file in the repo — and get their own branch when a real part needs one. Recorded as follow-up **S17b**. |
| 3 | Auto-constrain entry point | **Sketch-panel button + command; no automatic run on Finish Sketch; one dismissible offer when DOF > 20 at finish.** | S11 unchanged at 3 d. Finish Sketch stays a safe action; the offer is the only place the feature interrupts, and it happens once per sketch. |
| 4 | Snap dual path | **Delete the ~150-line legacy fallback; require core `snap_candidates` on every path.** | S26 drops from 2 d to **~1 h** — the duplication was already bounded (core owns the static key points; TS falls back only when the payload is absent). Per-move dynamic snaps stay in the UI by design (latency + CLAUDE.md). |
| 5 | Dimension layout owner | **C++ owns the rules; the TS preview is gesture-time approximation that converges on release; a fixture parity test (S23) locks the static case.** | S22 unchanged at 5 d; S23 becomes a hard prerequisite rather than a nice-to-have. |
| 6 | Ten dead drawing-sheet modes | **Delete from `DimensionToolMode` and both dropdowns.** | S1 includes the deletion; the drawing workspace keeps its own kinds and UI path. |

Two corrections this exercise produced, folded back into §4:

- **S26 was over-priced** (2 d → ~1 h). The legacy fallback is three small functions (~150 lines) that only run when `snap_candidates` is absent — not a second snap engine.
- **S1 grew** (2 d → 3 d) once the line-type segmented control and the dead-mode deletion were included.

---

## 7. Estimation basis

Reference points from this repo's own history:

| Shipped work | Comparable to |
|---|---|
| Arc fillet (line-arc + arc-arc): solver, create/enforce/delete, serialization, UI — 3 commits | S10 or S13 |
| Trim redesign (corner trim, split, extend, stroke) — PR #86 | S17 + S16 |
| Dimension draft windows + arc radius field (5 fix rounds) — PR #89 | S1(dimension modes) |
| Refactor: persistent naming + 282-command dispatch split + OCCT relocation — PR #93 | S25 (smaller) |
| Drawing annotation infrastructure (9 kinds, pick→preview→Enter) — 3 rounds | S4 + S24 |

A "new entity kind end to end" (C++ entity → refresh → viewport emit → payload → TS type → renderer → toolbar → pick → tests) costs **2–3 days** consistently in this codebase; that is the unit S13/S14/S16/S17 are priced in. Pure wiring (S2, S12) is a day or less; S1 is three because it also adds two dimension kinds and a line-type control. The two deep items (S9, S22) are the only ones where the estimate could double on an unlucky document-migration or layout-iteration.

---

## 8. Non-goals

- 3D sketch curves (project-to-surface, intersection, isoparametric, spun profile) — needs the surface workspace.
- Conic, blend curves (G1–G4), G2 constraint, regular-polygon constraint — Class-A sketching, not hobbyist 3D printing.
- A constructive (Owen / Fudos–Hoffmann) pre-solve before planegcs. Worth doing eventually; the numeric stage is not today's bottleneck, and it is a solver rewrite.
- Browser/WASM build of the core; a third constraint backend.
- Rewriting profile detection. Every item here treats `sketch_profile_exact.inc` as off-limits unless a defect demands it.

---

## 9. Verification protocol (per item, before commit)

1. `pnpm core:rebuild` — compile.
2. `pnpm test:core` — all suites; anything touching profile detection runs it in full, per CLAUDE.md.
3. `pnpm --filter desktop-ui exec tsc --noEmit`.
4. `pnpm lint:architecture` — already includes the command-dispatch ↔ schema diff (S2); extend with: no `fprintf(stderr` under `core/sketch/` (S7), no hardcoded hex in sketch render files (S19), no dimension-kind `switch` without a solver mapping (S9).
5. `pnpm registry:write` when a command or payload changes; commit the regenerated reference. **Known gap:** the registry's block-split parser misses `trim_preview` (guarded with `!=`) and the second name of a `||` chain — the S2 check scans globally, so the two can disagree. Worth aligning the registry's scan with the check's (about an hour) so the generated reference stops under-reporting.
6. In-app checklist written into the PR body — "compiled" is not "tested" in this repo.
7. `IPC-Protocol.md` + `AI-CAD-Command-Language.md` for every new command (binding rule).

---

## 10. Appendix — SolidCraft's sketch command surface (106)

Captured from the built binary (`solidcraft-cli.exe commands`, 404 total). Useful as the parity target and as a reference for what a complete sketch command set looks like.

**Create (40):** `create`, `line`, `line.midpoint`, `rectangle.two_point|center|three_point|rounded`, `circle.center|two_point|three_point|two_tangent|three_tangent`, `arc.three_point|center_point|tangent`, `polygon.inscribed|circumscribed|edge`, `slot.center_to_center|center_point|overall|arc_three_point|arc_center`, `spline.fit_point|control_point|control_point_5`, `ellipse`, `conic`, `point`, `centerline`, `infinite`, `text`, `project`, `project_to_surface`, `intersection_curve`, `isoparametric_curve`, `spun_profile`, `include_3d`, `intersect`, `fit_curves_to_section`, `insert_dxf`, `insert_svg`, `draw.dimensioned`, `line3d`, `point3d`, `spline3d`, `move3d`, `copy`, `paste`, `constrainer`.

**Modify (22):** `trim`, `trim.path`, `extend`, `break`, `cut`, `fillet`, `chamfer.equal_distance|distance_angle|two_distance`, `offset`, `offset.break`, `mirror`, `pattern.circular|rectangular`, `move`, `scale`, `blend_curve`, `break_link`, `delete`, `drag`, `drag_end`, `move_point`, `redefine`, `export_dxf`, `edit_text`.

**Constraints (16):** `coincident`, `collinear`, `concentric`, `curvature`, `equal`, `fix`, `horizontal_vertical`, `midpoint`, `parallel`, `perpendicular`, `polygon`, `symmetry`, `tangent`, `auto_constrain`, `auto_constrain_from_datum`, `finish_auto_constrain`.

**Dimensions and diagnostics (12):** `dimension`, `dimension_text`, `toggle_driven`, `glyphs`, `inspect`, `solve`, `snap`, `loose_ends`, `loose_ends.tie`, `chain`, `options`, `finish`.

**PolySmith-only, absent from SolidCraft:** corner trim, trim-stroke painting, split with constraint transfer, merge-coincident-points heal, core-resolved marquee (`select_sketch_rect`), live parametric projection links, text-on-path, the AI command bar, and the exact-curve profile arrangement.
