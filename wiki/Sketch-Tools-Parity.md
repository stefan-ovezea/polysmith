# Sketch Tool Parity — SolidCraft vs PolySmith, and the implementation plan

**Date:** 2026-10-23 · **Branch:** `feature/sketch-refactor`
**Sources:** SolidCraft's own built registry (`solidcraft-cli.exe commands` → 106 `sketch.*` entries, with each command's own parameter documentation) and PolySmith's toolbar (`layout/header/SketchToolbar.tsx`) plus its command reference.

PolySmith's **line tool is better than SolidCraft's** (chained segments, double-click to break the chain, H/V inference, draft dimensions, endpoint drag with solver preview). It is explicitly out of scope here — the goal is to reach parity on the *other* families without touching it.

---

## 1. SolidCraft's sketch catalogue (grouped)

**Create — geometry (22):** `line`, `line.midpoint`, `point`, `rectangle.two_point`, `rectangle.center`, `rectangle.three_point`, `rectangle.rounded`, `circle.center`, `circle.two_point`, `circle.three_point`, `circle.two_tangent`, `circle.three_tangent`, `arc.three_point`, `arc.center_point`, `arc.tangent`, `polygon.inscribed`, `polygon.circumscribed`, `polygon.edge`, `slot.center_to_center`, `slot.center_point`, `slot.overall`, `slot.arc_three_point`, `slot.arc_center`, `spline.fit_point`, `spline.control_point`, `spline.control_point_5`, `ellipse`, `conic`, `centerline`, `infinite`, `text`.

**Create — reference/projection (10):** `project`, `project_to_surface`, `intersection_curve`, `isoparametric_curve`, `spun_profile`, `include_3d`, `intersect`, `fit_curves_to_section`, `insert_dxf`, `insert_svg`.

**Modify (22):** `trim`, `trim.path`, `extend`, `break`, `cut`, `fillet`, `chamfer.equal_distance`, `chamfer.distance_angle`, `chamfer.two_distance`, `offset`, `offset.break`, `mirror`, `pattern.circular`, `pattern.rectangular`, `move`, `scale`, `blend_curve`, `break_link`, `delete`, `drag`, `drag_end`, `move_point`, `redefine`, `export_dxf`, `edit_text`.

**Constraints (16):** `coincident`, `collinear`, `concentric`, `curvature`, `equal`, `fix`, `horizontal_vertical`, `midpoint`, `parallel`, `perpendicular`, `polygon`, `symmetry`, `tangent`, `auto_constrain`, `auto_constrain_from_datum`, `finish_auto_constrain`, `constrainer`.

**Dimensions & diagnostics (12):** `dimension`, `dimension_text`, `toggle_driven`, `glyphs`, `inspect`, `solve`, `snap`, `loose_ends`, `loose_ends.tie`, `chain`, `options`, `finish`.

Plus lifecycle: `create`, `edit`, `finish`, `copy`, `cut`, `paste`, `delete`, and the 3D-sketch group (`line3d`, `spline3d`, `point3d`, `move3d`).

---

## 2. PolySmith today

**19 toolbar tools:** select, line, dimension, rectangle, circle, polygon, arc, fillet, chamfer, corner, extend, split, offset, ellipse, slot, spline, text, trim, project (7 with hotkeys).

**Creation modes already implemented** (split-button menus in `SketchToolbar.tsx` / `SketchToolPanel.tsx`):

| Family | PolySmith modes |
|---|---|
| Rectangle | `corner_corner`, `center_point`, `three_point` |
| Circle | `center_radius`, `two_point`, `three_point`, **`tangent_two_lines`**, **`three_tangent`** (both implemented — a stale comment says otherwise) |
| Arc | `three_point`, `center_start_end` |
| Polygon | `inscribed`, `circumscribed`, `edge` |
| Slot | `center_to_center` (one) |
| Spline | control-point only, degree ≤ 3 |
| Dimension | `auto` functional; 6 more sketch modes fall through |

---

## 3. The matrix

Verdicts: **=** parity · **+** PolySmith better · **−** missing · **~** partial · **×** out of scope (3D/surface).

### 3.1 Create

| SolidCraft variant | PolySmith | Verdict |
|---|---|---|
| `line` (+ midpoint variant) | line | **+** better; untouched by request |
| `rectangle.two_point` / `.center` / `.three_point` | same three | **=** |
| **`rectangle.rounded`** | — | **−** |
| `circle.center` / `.two_point` / `.three_point` | same | **=** |
| `circle.two_tangent` / `.three_tangent` | implemented | **=** |
| `arc.three_point` / `.center_point` | same | **=** |
| **`arc.tangent`** | — | **−** |
| `polygon.inscribed` / `.circumscribed` / `.edge` | same | **=** |
| `slot.center_to_center` | ✅ | **=** |
| **`slot.center_point`**, **`slot.overall`** | — | **−** |
| **`slot.arc_three_point`**, **`slot.arc_center`** | — | **−** |
| **`spline.fit_point`** | — (control points only) | **−** |
| `spline.control_point` | ✅ | **=** |
| **`spline.control_point_5`** (degree 5) | degree capped at 3 | **−** |
| `ellipse` | ✅ | **=** |
| **`conic`** | — | **−** (defer) |
| **`point`** (standalone) | — (points exist only as endpoints) | **−** |
| **`centerline`**, **`infinite`** | construction toggle only | **−** |
| `text` | ✅ (+ text-on-path parameter) | **+** |
| `project` / `include_3d` | project edge/face/vertex/body/profile | **=** |
| **`project_to_surface`**, `intersection_curve`, `isoparametric_curve`, `spun_profile`, `intersect` | — | **×** needs surfaces |
| `insert_dxf` | DXF import (File menu) | **=** |
| **`insert_svg`** | — | **−** |
| `draw.dimensioned` | draft-dimension system (geometry + dimension in one commit) | **=** |
| `line3d` / `point3d` / `spline3d` / `move3d` | — | **×** |

### 3.2 Modify

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| `trim`, `trim.path` | trim + trim-stroke painting | **+** |
| `extend`, `break`, `cut` | extend, split | **=** |
| `fillet` (line, line-arc, arc-arc) | same | **=** |
| `chamfer.equal_distance`, `.two_distance` | ✅ | **=** |
| **`chamfer.distance_angle`** | — | **−** |
| `offset` | offset (non-parametric copy) | **=** |
| **`offset.break`** | n/a — offsets are not associative here | **=** equivalent |
| `mirror` | ✅ | **=** |
| `pattern.circular`, `.rectangular` | circular + linear arrays | **=** |
| `move`, `scale` | transform (move/rotate/scale/copy) | **=** |
| **`blend_curve` (G1–G4)** | — | **−** (defer: Class-A) |
| `break_link` | `remove_sketch_projections` / unlink | **=** |
| `delete`, `drag`, `move_point`, `redefine`, `export_dxf`, `edit_text` | all present | **=** |

### 3.3 Constraints

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| coincident, collinear, concentric, equal, fix, H/V, midpoint, parallel, perpendicular, symmetry, tangent | all present (concentric by inference only — no command) | **=** / **~** |
| **`curvature` (G2)** | — | **−** (defer) |
| **`polygon`** (regular polygon) | — | **−** (defer) |
| **`auto_constrain`**, `finish_auto_constrain` | — | **−** |
| **`constrainer`** (apply the near-miss constraint) | — | **−** |

### 3.4 Dimensions & diagnostics

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| `dimension` (12 kinds, refusal naming the determiner) | 12 kinds; conflict is counted, not named | **~** (S4 in the plan) |
| `dimension_text`, `toggle_driven` | ✅ | **=** |
| **`glyphs`** (show/hide constraint glyphs) | always on | **−** |
| **`inspect`**, **`solve`** | — (solve is automatic) | **−** |
| `snap` (queryable) | core emits candidates; UI resolves | **=** |
| **`loose_ends`** + tie | — | **−** |
| `chain` (double-click select) | marquee/click select | **=** |
| `options` | selection-filter panel | **=** |

---

## 4. The plan

Three phases. Effort is per item, one developer who knows the codebase. Nothing here touches the line tool.

### T1 — Cheap parity (≈ 7 days, no profile-detection risk)

| # | Item | Approach | Effort |
|---|---|---|---|
| T1.1 | **Slot: `center_point` + `overall`** | Same `add_sketch_slot` payload; only the pick sequence and the length meaning differ (span vs centre distance). Mode in the toolbar, arithmetic in the draft commit. | 1 d |
| T1.2 | **Chamfer `distance_angle`** | `SketchChamfer` gains `mode: equal \| two_distance \| distance_angle`; geometry is a line trimmed by an angle from one edge; third split-button mode. | 1 d |
| T1.3 | **Rounded rectangle** | New core command `add_sketch_rounded_rectangle { start, end, radius }` that creates the rectangle then four parametric fillets **atomically** (one undo step, one command). Avoids five round-trips and gives the UI a single call like `rectangle`. Needs the internal no-undo fillet entry point (`sketch_fillet_create_command.inc` computes trim/arc indices from `next_trim_entity_index`). | 2 d || T1.4 | **Point entity** | New `add_sketch_point`; standalone points participate in holes/patterns/constraints but not in profile loops. Touches the vertex model — the only T1 item with model risk. | 2 d |
| T1.5 | **Centerline / infinite line** | Line flags rather than new tools: `centerline`, `infinite` alongside `is_construction`; dashed/dash-dot styles already exist in SolidCraft as a reference; PolySmith has the `constraint` field to extend. | 1 d |

### T2 — Geometry work (≈ 15 days)

| # | Item | Approach | Effort |
|---|---|---|---|
| T2.1 | **Fit-point spline (+ degree 5)** | `mode: "fit" \| "control"` on `add_sketch_spline`; least-squares fit to a clamped B-spline (`spline_math.h` has the sampler); allow degree up to 5. **This is the item that also addresses S0** — the red `cad_core_spline_test` fails in `test_spline_extrude_smoke`, i.e. the spline→profile→extrude path this work rewrites. Treat S0 as the first sub-step: fix the failing case, then add fit points. | 3 d |
| T2.2 | **Tangent arc** | Third arc mode: start on an existing curve end, tangent there, through the cursor. Reuses the tangent-circle solve from the fillet work. | 2 d |
| T2.3 | **Arc slots (2 modes)** | New entity: two concentric arcs + two straight tangent sides; needs serialisation, rendering, trim/split behaviour and profile integration. Highest risk in the plan — `sketch_profile_exact.inc` is the repo's most fragile file. | 3 d |
| T2.4 | **Insert SVG** | Parse paths/polylines into sketch lines/beziers; the DXF importer is the template. | 1 d |
| T2.5 | **Auto-constrain + constrainer** | Infer, then dimension until DOF 0; keep a candidate only when DOF drops. Needs S2/S3 from the completion plan (one DOF source) first. | 4 d |
| T2.6 | **Loose ends + inspect + glyph toggle** | Loose ends from the vertex table the profile walk already builds; inspect reports counts/DOF; glyph toggle is a render flag. | 3 d |

### T3 — Deferred

Conic, blend curves (G1–G4), G2 curvature constraint, regular-polygon constraint, all 3D-sketch/surface commands, fit-curves-to-section. Each is either Class-A or needs the surface workspace.

---

## 8. Progress log

**2026-10-24 — S2 and S7 (plan items audit).** Two P0/P1 items were already satisfied and were verified rather than redone: **S2** (schema and dispatch integrity) — the unknown-command fallback in pp.cpp answers with UNKNOWN_COMMAND and the request id, and pnpm lint:architecture keeps the dispatch table and commands.schema.json in step in both directions; the missing regression test was added (unknownCommand.core.test.ts: the error carries the id, the loop keeps serving commands afterwards, and a handler-level failure also settles instead of hanging). **S30** (linked-extrude refresh corruption) — private_linked_extrude_refresh.inc already copies the existing parameters and overwrites only the profile-derived fields, and 	ests/extrude_refresh_test.cpp pins it.

**S7 done — the sketch core no longer writes to stderr.** Fifteen printf(stderr, ...) / std::cerr sites across six files became polysmith::core::log_error/log_warn/log_info("sketch", ...), so they now reach the in-app Logs panel with level and source instead of the shell's metadata-less stderr bridge (the app has no terminal). The messages were rewritten in ASCII: the existing literals carried mojibake from an earlier encoding round-trip, which the logger would have shown verbatim. pnpm lint:architecture gained a fifth check, 
o raw stdio in the sketch core, proven to fire by planting a probe printf and watching it fail. Core suite 79/79.


**2026-10-24 — S1, i18n-proof layout.** A first attempt at the line-type control as a one-row segmented picker put "Infinite" outside the 288 px panel; it was reverted to a dropdown (the four-way mapping stays, now unit-tested in lineTypeSelection.ts). The audit that followed hardened every remaining row of choices: the arc-mode pair, the text-panel alignment rows, the CAM post import/export pair, and the path+browse rows all wrap now (lex flex-wrap on the container, min-w-0 flex-1 basis-* + wrapping text on each button) instead of relying on English labels fitting. pnpm lint:architecture gained a fourth check, layout option rows are wrap-safe, which fails a plain lex gap-2 row of choices — verified by removing lex-wrap from the arc row and watching the check fire. The rule is written down in CLAUDE.md so the next mode selector starts from a dropdown.


**2026-10-24 — S1, constraint buttons.** Five constraint kinds that existed in the core but had no toolbar button are now reachable: **collinear** (line pair, same flow as parallel), **midpoint** (point then line), **tangent** (line then circle/arc), **tangent pair** (two curves, either order), **symmetric** (two points then the axis line). Each has a glyph, a translatable label and an armed-status hint that changes with the pick phase. The line-pick handler was already receiving circle and arc ids (every sketch-entity click funnels through it), so it now takes an isLineEntity predicate instead of assuming a line.

Two legibility gaps came out of this: **collinear relations and midpoint anchors were never emitted as viewport badges** — they constrained the sketch invisibly — and neither were the new tangent/symmetric kinds. All now emit (L, M, T, S). A third gap became **D-006**: the document schema's line_relations[].kind enum lacked collinear (and 	angent_line_line), so the first reply containing one failed validation and the UI silently lost the whole document state. Fixed in the document schema, the TS types and the viewport enum.

Tests: 12 pure pick-order cases in `sketchConstraintPick.test.ts`; end-to-end in `sketchCollinearMidpoint.core.test.ts` (2) and `sketchTangentSymmetric.core.test.ts` (3 — the tangent badge, the curve-pair tangency, and the symmetric solve that mirrors the pair about the axis). Core suite 79/79.


**2026-10-23 — T2.3 arc slots (core).** `SketchSlot` gained `mode: "arc"` with `arc_radius`, `start_angle`, `sweep_angle`; the record's `center_x/center_y` becomes the ARC centre in that mode, so the existing movable centre vertex and its drag path move the whole slot with no new vertex machinery. `refresh_sketch_slots` branches: an arc slot expands to an outer arc (path + half width, counter-clockwise), the end cap (a straight **radial** segment), the inner arc walked back clockwise, and the start cap — four deterministic ids (`arc-…-outer`, `line-…-end`, `arc-…-inner`, `line-…-start`) under the same `generated_by = "slot:<id>"` contract, so the profile walk sees ordinary lines and arcs and needed no change at all. Validation rejects a centreline radius that would make the inner boundary non-positive, a zero sweep, and a full turn (the caps would coincide); an unknown mode is rejected by name. `update_sketch_slot` keeps the stored mode and, for an arc slot, keeps omitted arc fields — a plain centre/width edit cannot silently collapse the arc. Serialization round-trips the new fields (older saves default to straight, which is all they can contain). Tests: three new cases in `tests/slot_test.cpp` (expansion + the complete region id set + winding, validation, and centre-drag); `pnpm test:core` 79/79. UI half: a Slot type selector in the sketch tool panel (Straight / Arc), a Sweep (deg) field for arc slots, and a draft preview that draws exactly what the core expands — outer arc, far radial cap, inner arc, start cap — using the same derived geometry the commit sends (`slotArcDraftGeometry`), so the preview cannot disagree with the created entity. Click 1 is the arc centre, click 2 a point on the centreline; the half width is derived and clamped below the centreline radius. Tests: five new cases in `slotDraftPreview.test.ts`, an arc case in `draftCommit.test.ts` (the payload carries mode/arc radius/start angle/sweep and no stadium fields), and three end-to-end cases in `sketchArcSlot.core.test.ts` (expansion + complete region id set, inverted-inner-boundary refusal, centre drag).

**2026-10-23 — T2.1 fit-point spline + degree 5 (core and creation UI).** `add_sketch_spline` gained `mode: "control" | "fit"` and `degree`, and the degree cap went 3 → 5 (the record always carried a degree; every consumer — wire builder, profile walk, viewport — reads it from there, so creation was the only place that had to change). Fit mode interpolates: the poles are solved at the **Greville parameters of the same open-uniform knot vector** the rest of the pipeline evaluates with (`spline_fit_poles`: span-based basis rows + a dense partial-pivot solve, ≤256 points). Solving at chord-length parameters would have produced poles for a *different* knot vector and the curve would have missed its points — the knot vector is the reason this is not the textbook A9.1 parameterisation. Fit points are not stored: the entity is a control-point spline through them (Fusion makes the same trade for an unrefined fit spline). UI: a mode selector in the sketch tool panel (Control Point / Fit Point) with a one-line explanation, the fit preview solves the *same* interpolation in TS (`lib/splineCurve.ts::fitSplinePoles` — cross-checked against the core's exact poles in `splineCurve.test.ts`), and all three commit paths (first-point click, tool switch, panel) pass the mode.

**A real defect fell out of the fit test: the De Boor evaluator iterated UP from the span**, blending values it had already overwritten — so both the C++ (`spline_math.h`) and TS (`lib/splineCurve.ts`) evaluators drew/sampled a curve that did not pass through its own poles at interior parameters. A degree-3 spline through (0,0) (3,4) (7,5) (10,0) evaluated to (1.89, 2.37) instead of (3,4) at u = 1/3. The downward loop fixes both; a direct Bezier-value test now pins it on each side. Logged as D-005.

**2026-10-23 — T1.4 standalone point.** `add_sketch_point { x, y }` places a point that is *not* derived from any curve. Model: a `SketchPoint { point_id, x, y }` record in `sketch_parameters.points[]`, re-emitted into the vertex table by `rebuild_sketch_vertices` with kind `point`. The id **is** the vertex id (same convention as projected points), so selection, snapping, `update_sketch_vertex`, vertex-distance dimensions and constraints work on it without a second id namespace. Three details that needed care: the rebuild takes the *previous* vertex table as authoritative for these points and writes the coordinates back into the record (a drag edits the vertex, and without the write-back the next refresh would revert it); `update_sketch_vertex` needed an explicit `kind == "point"` branch because the generic path only propagates through connected curves; and `delete_sketch_selection` needed a clause in `will_delete` (a point owns no curve, so a lone point hit the "nothing to delete" early return) plus a `point_ids` record removal. A repeat click within 1e-3 mm selects the existing point instead of stacking a coincident duplicate. Nothing about a point reaches the profile walk, so the region set is provably unchanged. UI: a Point tool in the toolbar (one click; a click on an existing vertex reuses that vertex's exact coordinates), rendering through the ordinary vertex path. Tests: `tests/add_sketch_point_test.cpp` (movable vertex + no profile; complete region set with a point inside a rectangle; move persists through the record; dedup; delete removes only the point; save/load round trip) and `sketchPoint.core.test.ts` (the same four flows over real IPC). Two schema enums had to learn the new kind — `vertices[].kind` in both the document and viewport zod schemas — otherwise the whole `document_state` reply is rejected by the UI parser (the trap recorded in the handoff). Initial limitation of this slice, fixed the same day: the click committed the **raw ray position**, so the placed point ignored the snap marker the pointer move was showing (reported as "there is no snap for point"). `cursorSnappedLocal` — the click after `resolveSnappedSketchPoint` ran — now feeds the commit, covering grid, endpoint/centre/quadrant vertex snaps, on-line, midpoint, intersection, tangent, perpendicular/parallel and H/V alignment, with an exact vertex hit winning first. The Text tool had the same raw-position bug and now uses the same rule. Regression test: `pointerUpActiveSketch.test.ts` (snapped wins over raw, vertex hit wins over both, null-safe, stale vertex id ignored).

The bigger half of the same report: the core's viewport snap candidates were built from **lines and arcs only**, so a standalone point was a dot nothing could snap to — dragging a line endpoint onto it silently ignored it. `snap_candidate_emit.inc` now emits an `endpoint` candidate for every `sketch.points` entry and every `projected_points` entry (vertex id set, entity id empty), which makes a placed point a first-class drag target for line/arc endpoints, circle/ellipse/spline drags and the move tool. Regression tests: `test_point_is_a_snap_target_in_viewport_state` in the C++ suite and the `snap_candidates` assertion in `sketchPoint.core.test.ts`.

**Bind on drop (option A).** The remaining objection was that a snapped point stayed "hidden geometry": the drop left two coincident vertices and nothing bound them, while every other vertex merges into the geometry it meets. `update_sketch_vertex` gained an optional `bind_to_vertex_id`; when the drag ends on a Point-tool point the core makes the point coincident with the dropped vertex (which merges the point's id into the endpoint for every line that shared it), records the `coincident` constraint the viewport badges, and drops the now-orphaned `SketchPoint` record — in the SAME undo step as the move. A target that is not a Point-tool point is ignored, so ordinary endpoint drops behave exactly as before. Tests: `test_dropping_a_line_on_a_point_absorbs_the_point` (record gone, exactly four corner vertices, coincident constraint, region set unchanged, one undo restores the point and the pre-drag position) and the IPC twin in `sketchPoint.core.test.ts`, which also asserts the badge reaches the viewport. UI: `pointIdAtPosition` finds the target at the snapped coordinates, `makeUpdateSketchVertexCommand` sends it, `endpointDragPointerUp.test.ts` pins the self-bind and off-target cases. Remaining (option B, parked): hole centres / pattern anchors — the real reason to place points daily.

The Point button also exposed a toolbar defect: in the sketch workspace the toolbar was one non-wrapping line, so on a 1453 px window everything past Text was clipped and unreachable — including the split buttons (Rectangle, Arc, Circle, Polygon, Dimension, Slot, Chamfer) and the whole constraint strip. The toolbar is now a fixed two-row block (geometry tools on row 1; constraints + crosshair on row 2) that wraps within each row, and the header gives it a definite width (`flex-1`) to wrap against. That is why the Point tool "could not be found": it was visible, but the row's overflow made every other new control invisible, so the toolbar could not be read as a whole.

**2026-10-23 — "rounded rectangle creates a polygon" checked.** The committed result is not a polygon: the rounded rectangle is one region whose boundary is 4 lines + 4 circular arcs (each exactly `r` and a quarter turn), and the extruded body comes out with 10 faces and 8 circular edges. The **region kind** reported by `get_sketch_profiles` is `"polygon"` — that is the region taxonomy (`polygon | circle | ellipse | spline`), not a statement about the boundary, so it is correct and unchanged. One genuine faceting artefact was found and fixed: the drag preview tessellated every corner with a fixed 8 chords (0.15 mm sagitta at r = 8, 0.77 mm at r = 40, visible when zoomed), so it now scales with the radius from a 0.01 mm sagitta bound. `sketchRoundedRectangle.core.test.ts` now also asserts each boundary arc's radius and 90° sweep plus the region area against the analytic rounded-rectangle area, so a future polygonisation of the outline fails the suite.

**2026-10-23 — S0 fixed (T2.1 first sub-step).** `cad_core_spline_test` now passes — the first green run since the suite landed on 2026-08-28. The failing leg was not the face walk (the trace shows a clean closed 3-edge region) and not the prism: it was the **save/load** leg. `load_document_from_path` refreshed each sketch's profiles but never re-derived the sketch-derived features that cache their source profile geometry, so an extrude whose opaque profile snapshot was absent compiled with an empty profile and threw `BRep_API: command not done`. The fix re-derives every sketch-derived feature after the profile refresh (`refresh_linked_features_after_load`, wrapping the same `refresh_linked_extrudes` every sketch edit runs). That is the documented contract — profiles are derived and never persisted — so saves without the opaque cache, older saves, and churned profile ids now reload into compilable bodies. Diagnostics added while chasing it were removed. **`pnpm test:core`: all 77 suites pass.**

**2026-10-23 — T1.1 slots, T1.2 chamfer, T1.5 reference lines (this session).**

- **T1.1 — slot pick modes.** Three modes in the slot split button and the tool panel: `center_point` (slot centre → arc centre; the behaviour the tool always had), `center_to_center` (two arc centres), `overall` (the two cap tips, so `length = span - 2r`). The mode's arithmetic lives in one `slotDraftGeometry()` used by BOTH the live preview and the commit, so the two can no longer disagree. The overall mode's default radius is capped at `span/6`, which keeps the core's `length >= 2*radius` rule for every span. Tests: `slotDraftPreview.test.ts` (mode arithmetic + the rule across a span sweep) and commit-path assertions in `draftCommit.test.ts`.
- **T1.2 — chamfer `distance_angle` (+ `equal`).** `SketchChamfer` gained `mode` (`equal` / `two_distance` / `distance_angle`) and `angle_degrees`; for `distance_angle` the distance along line B is **derived** from the angle on every recompute (not frozen at creation), so dragging a line keeps the angle. The angle must be inside `(0, corner angle)`. Update keeps the record's mode when `mode` is absent, so a plain distance edit never converts a distance+angle chamfer. The chamfer split button offers all three modes and the floating panel shows the fields the mode needs (1 distance / 2 distances / distance + angle). Tests: four new cases in `tests/sketch_chamfer_test.cpp` (derivation, line drive, equal, out-of-range refusal), `sketchChamfer.core.test.ts` (real core through the real builders, including the refusal), and schema coverage for the new fields.
- **T1.5 — centerline / infinite line.** A line flag, not a new tool: `SketchLine.line_type` (`""` / `"centerline"` / `"infinite"`) is persisted, parsed and carried through the viewport payload. Both flavours force the construction flag in the core, so profiles and automatic dimensions are handled by the existing rules — no special cases in the profile walk. Presentation is UI-side: centerlines get a long-dash style; infinite lines are drawn 100 000 mm past their endpoints, which is safe because camera framing uses the core's scene bounds and the document keeps the clicked endpoints. The line tool's panel replaces the Construction checkbox with a four-way Line type dropdown. Tests: `tests/line_type_test.cpp` (flavour + forced construction + no dimensions/profiles, centerline inside a rectangle still one region), `sketchLineType.core.test.ts` (both payloads the UI consumes), `sketchObjects.test.ts` (dash + extension rendering).

---

**2026-10-23 — T1.3 core, first slice (committed).**
`add_sketch_rounded_rectangle { start_x, start_y, end_x, end_y, radius }` exists and works, but the composition is simpler than planned: instead of an internal no-undo fillet entry point, it opens a **named undo group** (`begin_undo_group("Rounded Rectangle")`), calls the ordinary `add_sketch_rectangle` and four times the ordinary `add_sketch_fillet`, then collapses them. `undo_group_commands.inc` already folds nested pushes into one entry, and an abort restores the start snapshot, so a failed fillet leaves nothing behind. The result keeps real `SketchFillet` records, so the radius stays editable and deleting a fillet restores its corner — four arcs welded on would not.

Validated by `tests/sketch_rounded_rectangle_test.cpp` (registered in CMake, passing): four lines, four arcs, four fillet records at the requested radius, one closed profile; the radius is editable through `update_sketch_fillet_radius`; a radius of half the shorter side is refused; and a single `undo()` removes the sides, the arcs and the fillet records together.

Remaining for T1.3 (UI, next): the `rounded` rectangle mode in the toolbar split button, a radius field in the sketch tool panel, and the draft commit path calling the new command — plus the two-commit-paths check from `Sketch-Tool-Implementation.md` (Enter and click-click).

**2026-10-23 — T1.3 UI (this session).** The rounded rectangle is usable end to end:

- **Toolbar + panel**: a fourth option in the rectangle split button (`toolbar.rectangleRounded`), the mode shown in the tool panel's subtitle, and a **corner-radius field** that appears only in that mode (default 5 mm, the sketch fillet panel's default).
- **Both commit paths** send the one command. Click-click goes through `draftCommit.ts`, Enter/submit through `ViewportPanel.commitDraftDimensionSession`; both use corner-corner semantics (the two entered points are opposite corners) and the panel's radius. No UI-side composition of rectangle + four fillets.
- **Live preview**: `roundedRectangleOutline2d` draws the rounded rubber band (8 segments per corner) and clamps the radius while the user is still dragging out a rectangle that is too small for it — the core, not the preview, is what refuses the commit, with its message in the toast.
- **Auto dimensions**: a rounded rectangle's side dimension measures the fillet-trimmed side, not the overall size the draft badge shows, so both paths always drop the four auto side dimensions and a typed width/length is not replayed onto them (it already shaped the entered end point). Documented at the call sites.
- **Tests**: `rectangleDraftPreview.test.ts` (outline maths), `draftCommit.test.ts` (the click commit forwards the radius, skips the plain rectangle command, forces the dimension drop), and `sketchRoundedRectangle.core.test.ts`, which drives the **real core** through the **real command builder**: four sides, four fillets at the requested radius, exactly one profile bounded by the four lines + four arcs, one undo removing all of it, and the oversized-radius refusal leaving nothing behind.
- Docs: `IPC-Protocol.md` and `AI-CAD-Command-Language.md` now document the command; `pnpm lint:architecture` and the desktop `tsc --noEmit` are green.

---

## 5. Order and why

**T1.3 (rounded rectangle) → T1.1 (slots) → T1.2 (chamfer) → T1.5 (line types) → T1.4 (point)**, then **T2.1 (fit-point spline, fixing S0 on the way)**.

Rationale: rounded rectangle is the most visible gap in everyday parts and it is *composed from primitives that already work* (rectangle + fillets), so the risk is integration, not geometry. Slots and the chamfer mode are arithmetic. The point entity is last in T1 because it touches the vertex model. Fit-point spline opens T2 both because it is the next most-requested family and because it shares the failing path with S0.

## 6. Testing contract per item

- Every creation variant ships with a C++ suite in `native/cad-core/tests/` asserting the **complete region set** (`profiles_match`, per CLAUDE.md) plus the parametric records the variant creates (fillet ids for the rounded rectangle, slot geometry for the modes).
- Anything touching `sketch_profile_exact.inc` (T2.3 only) runs the full `pnpm test:core`.
- UI variants also need the in-app checklist in the PR body: the tool appears in the split button, both commit paths work (Enter and click-click — see `Sketch-Tool-Implementation.md`), and Escape cancels without leaving geometry.
- IPC additions: `pnpm registry:write`, `IPC-Protocol.md`, `AI-CAD-Command-Language.md`.

## 7. What this branch already fixed, for context

The extrude/sketch interaction defects (D-001a–f in [Sketch-Defect-Log](Sketch-Defect-Log)) came out of the same code these tools sit on. Two of those fixes matter here directly: the profile-id staleness after an edit is why arc slots (T2.3) are priced as high-risk, and the fit-point spline path is the same one S0 fails in.
