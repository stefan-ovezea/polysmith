# Active Task: Sketch toolset finalization (feature/sketch)

> **Branch:** `feature/sketch` (from `dev`, after #67)
> **Date:** 2026-08-22
> **Plan:** approved plan at `.claude/plans/rosy-gliding-pizza.md` (milestones
> SK0–SK8; each independently committable; every milestone gated on
> `pnpm test:core` + `tsc` green + user runtime verification)

## Status

**SK0 — committed** (`e00e549`): schema/whitelist alignment + tool_whitelist suite.
**SK1 — committed** (`da7a5ff`): parametric arcs via deterministic
`enforce_arc_dimensions`; GCS::Arc plumbing dormant until arc-referencing
constraints; append-focus latent fix; parametric_arc suite (6 cases).
**SK2 — committed** (`f8dcbbd`): constraint completion (symmetric/collinear/
midpoint/tangent pairs/anchor-t) + constraint_completion suite (8 cases).

**SK3 (new geometry) — committed** (`0b9ceaa`): ellipse + slot + chamfer
(core + tests + UI wiring), the SK2 anchor-mapping revert, the
point_distance center fix, delete-path fixes, ellipse viewport
primitive, AI schema fixes. All 21 suites + tsc green; user-verified
in-app.

### SK3 follow-ups (user-reported, deferred)

- **Slot draft radius is hardwired** (length/4 clamped to [0.5, 2]) —
  the 2-click draft gives no way to specify the radius while drawing;
  only the panel can edit it afterward. Consider a 3-click draft or a
  draft input.
- **Slot/ellipse drag preview doesn't follow** — dragging the slot
  center shows only the vertex moving until release (the client-side
  WASM preview solver doesn't know the new entities; the core lands
  the geometry correctly on pointer-up).

### SK3 detail (for reference)

- **Ellipse — done, tested.** `SketchEllipse` entity (center + 2 axis
  points, axis points fixed at creation, no solver registration v1);
  creation command end-to-end; exact profile engine `kEllipse` (full closed
  curve, region kind "ellipse"); wire builder `GC_MakeEllipse` (XDir
  orthogonalized); extrude routing via boundary edges (incl. the
  wire-path-condition fix `!parameters.boundary_edges.empty()`); move
  support (center movable, axis pinned); save/load serialization; IPC +
  schema + AI schemas. New suite `cad_core_ellipse_test` (6 cases:
  creation, full profiles_match, extrude smoke, move preserves a/b/rotation,
  construction excluded, trim rejected).
- **Slot — done, tested.** `SketchSlot` struct (center/length/radius/
  rotation, mode "straight", length ≥ 2·radius validated); expansion at
  recompute top right after text expansion: 2 lines + 2 arcs
  tangent-by-construction (CCW loop: bottom bl→br, right arc br→tr ccw,
  top tr→tl, left arc tl→bl cw), `generated_by="slot:<id>"`, deterministic
  ids outside user counters. Center is a regular movable "vertex-N"
  vertex (distance dims work — with a new center-owner cache sync in the
  point_distance drive); corner/arc-center vertices carry "vertex-slot-"
  and are re-marked fixed. add/update/delete commands + doc wrappers +
  IPC handlers + schema + AI schemas + save/load. Delete path: slot ids,
  slot-generated selections (like text), slot centers; ALSO fixed while
  here: ellipse deletion (was a silent no-op) and update_sketch_vertex
  on ellipse centers. Tool whitelist gained "ellipse" + "slot".
  New suite `cad_core_slot_test` (8 cases: creation/ownership, rotated
  geometry both signs, full profiles_match exact 4-id set, update
  re-expansion, generated-entity guards, center drag + move, distance
  dim between slots, extrude smoke + delete).
- **Sketch chamfer — done, tested.** `SketchChamfer` struct cloning the
  fillet lifecycle (corner cache, trim vertices, parametric record);
  create/update/delete trio + recompute pass `enforce_sketch_chamfers`
  (virtual-corner intersection, trim = stored distance along each
  outgoing direction — no angle math); chamfer line is a plain line
  owned by the record (mirrors the fillet arc ownership); corner
  re-emitted as "fillet_corner" kind so delete can restore it; mutual
  exclusion with fillets BOTH ways; delete-selection ownership rules;
  doc wrappers + IPC handlers + schema + AI schemas + save/load;
  "chamfer" added to the tool whitelist. New suite
  `cad_core_sketch_chamfer_test` (7 cases: symmetric + asymmetric
  geometry, distance edits both directions, full profiles_match 5-edge
  chamfered rectangle, delete restores shared corner, fillet/chamfer
  conflict both orders, chamfer survives a dimension drive).
- **UI wiring — done, tsc clean.** Full 13-file path for all three
  tools: toolbar entries + icons (EllipseIcon / SlotIcon /
  SketchChamferIcon — named to avoid the 3D chamfer glyph) + i18n
  keys; ellipse = 3-click draft (center → major axis → minor axis)
  with a live preview reusing the circle preview ref; slot = 2-click
  draft (center → axis end, radius defaults to length/4 clamped to
  [0.5, 2]) with a stadium preview group (new previewSlotRef +
  clearPreviewSlot in the preview-actions factory); chamfer = click
  tool cloning the fillet flow (corner picking with fillet/chamfer
  exclusion, session lifecycle with two distances, ActiveSketchChamferPanel
  + SketchChamferPanel with two debounced inputs). Select-mode routing:
  slot-generated lines/arcs open the Slot panel (SketchSlotPanel:
  length/radius/rotation, deg→rad, radius clamped to length/2),
  chamfer lines open the Chamfer panel bound to that chamfer, ellipse
  hits select/move via the generic entity path. AI schemas fixed
  (set_sketch_tool enum + delete_sketch_chamfer payload).
  **Core addition:** `ViewportSketchEllipsePrimitive` — ellipses had
  NO viewport rendering path; added the primitive (center/a/b/rotation
  + plane frame), emission in sketch_curve_polygon_emit.inc, state +
  serialization plumbing, and the full TS render/pick/marquee/move
  chain (SketchEllipseScene → buildSketchEllipseObject →
  updateSketchEllipseObject; exactDistanceToCurve gains a closed-form
  ellipse polar-radius branch).
- **Remaining:** nothing for SK3 — committed. Next: SK4 (extend,
  offset, transform family, arrays).

**SK4 (editing) — committed** (`3334e97`): extend, offset, transform
family, arrays — core + tests + UI (extend/offset tools, offset live
fan-out session, Transform/Array panel with session-scoped Cancel).
All 25 suites + tsc green; user-verified in-app.

### SK4 detail (for reference)

- **Extend** (`extend_sketch_entity`): line (infinite support) and arc
  (full circle) extension from the nearest end to the nearest
  intersection. Dedicated extend-intersection math (target unclipped,
  local wrap/sweep helpers — trim's are TU-local to trim_engine.cpp),
  opening-bounded arc side filter, trim point-rebind reuse, H/V
  preserved, arc angle dims flip to driven. `cad_core_extend_test` (7
  cases incl. profiles_match closed rectangle).
- **Offset** (`offset_sketch_entity`): signed single-entity offset via
  the creation constructors (auto dims off, no inferred constraints):
  line -> parallel (left-normal convention), circle -> concentric
  radius+d, arc -> same sweep at radius+d. Collapse/invert +
  construction/generated/ellipse rejection. `cad_core_offset_test`
  (6 cases, both signs).
- **Transform** (`transform_sketch_entities`): move_sketch_entities
  refactored to a rigid wrapper over transform(dx, dy, center, angle,
  scale, copy). In-place scale keeps H/V, scales circle/arc radii +
  ellipse a/b + slot dims, flips circle/arc radius dims to driven,
  line dims re-measure. Copy mode = exploded raw records with fresh
  ids, a source-vertex->copy-vertex map (copies share corners with
  each other, never with originals), H/V inferred only when not
  rotating. `cad_core_transform_test` (6 cases, both scale sides,
  two-profile copy, single undo).
- **Arrays** (`create_linear_array` / `create_circular_array`):
  direct-commit exploded copies through the transform copy path (one
  undo per array). DEVIATION from plan: the pending_array preview
  workflow is deferred — undo is the adjust path for v1.
  `cad_core_array_test` (3 cases: linear 3x, circular 6x on-circle
  1e-6, unique ids + single undo).
- **Verified:** build green; ALL 25 suites pass (extend/offset/
  transform/array new + 21 existing); tsc clean. Debugging notes:
  the extend test failure was a dangling pointer in the TEST (find_line
  over a temporary snapshot) — fixed with named snapshots, not a core
  bug. Trim's angle helpers are TU-local to trim_engine.cpp — the
  extend file carries local copies (kExtendPi).
- **UI wired so far:** toolbar entries + icons + i18n for extend and
  offset; extend = click tool (click near the end to stretch);
  offset = click tool with an Offset session panel (distance applies
  to each clicked entity; cancel deletes the session's offsets);
  hook wrappers + IPC types + AI schemas for all four commands.
- **UI wired:** extend/offset toolbar tools + icons + i18n; extend =
  click tool; offset = click tool with a live session panel (distance
  fan-out: delete + re-create each source->copy pair on every
  debounced input; Cancel deletes the session's copies; blur-flush +
  ref-based reads so typed values always reach the click); transform/
  array = "Transform / Array" context-menu entry on sketch selection
  (next to Move/Copy) opening SketchTransformPanel (dx/dy/angle/
  scale/copy/center + linear/circular array with count/total-angle,
  selection centroid pre-filled).
- **Offset debug history (user-reported, fixed):** input reset while
  typing (focused-input guard in useDebouncedNumericPreview), double
  command send + post-hoc snapshot (handler rewrite), debounce gap
  between typing and clicking (blur flush + session ref).
**SK5 (circle modes) — committed** (`de2e202` + `f2493d3`): mode
resolution, circle-slave tangent relations, UI picking flow, then the
user-reported follow-ups — bisector absolute-projection fix, face-walk
tangent-node fix (enclosed region between a closed polygon and an
inscribed tangent circle), T badges, schema enum fixes. All 26 suites
green.

### SK5 follow-up (user-reported, deferred)

- **Enclosed surface still missing in-app** despite the regression
  suite detecting it for the same geometry (closed triangle +
  inscribed circle). Suspects: a stale cad_core.exe in the running
  app, or a live-sketch arrangement difference (H/V constraints,
  enforcement-adjusted radius breaking exact tangency). Needs a
  live trace with PS_TRACE_FACES when revisited.

**SK5 detail — core + green:**

- `add_sketch_circle` gains a `mode` field with wrapper-side
  resolution (the arc wrapper pattern): two_point (diameter
  midpoint), three_point (circumcircle), tangent_two_lines (center on
  the corner's angle bisector — the hint point selects the wedge via
  the u1±u2 bisectors and its projection sets the size),
  tangent_three_lines (triangle incenter via side-length-weighted
  vertices). All resolve to (center, radius) in the doc wrapper —
  single source of truth.
- Circle-slave tangent relations: new line_relation kind
  "tangent_circle_line" (first=circle, second=line) +
  `enforce_tangent_circle_line_relations` in the refresh pipeline:
  the radius re-derives as the min distance from the fixed center to
  the defining lines (center-solve onto the new bisector deferred).
- IPC handler reads optional mode fields; AI/TS schemas pending.
- New suite `cad_core_circle_modes_test` (6 cases: two/three-point
  exact circles, tangent-two both wedge sides, radius re-derives
  after a line move, incenter of a 3-4-5 triangle, complete
  profiles_match of the tangent-circle region — kind "polygon" with
  the circle id in the boundary list + has_source_circle_id).
- UI wired: two/three-point drafts send raw points via the mode-aware
  addSketchCircleMode path; tangent modes pick 2-3 lines (entity/point/
  proximity resolution) then place via a hint click; the draft rubber
  band is SUPPRESSED in tangent modes (pointer-down gate + preview
  gate) and the pick feedback uses the floating snap label (the log
  messages were invisible to the user). (two_point/three_point drafts already
  compute geometry in the UI — send mode+points instead of
  center/radius; tangent modes need a line-pick + placement click
  flow), runtime verification, commit.

### SK4 follow-up (user-reported, deferred)

- **Chain/loop offset** — offsetting a rectangle (any connected
  contour) must offset the whole loop together, Fusion-style.
  Current v1 is single-entity; the plan already deferred chain
  offset (corner-join miter/fillet/intersect handling risks the face
  walk). Also noted: distance fan-out delete+recreate makes each
  debounced keystroke an undo step — consider batching into one undo
  step when revisiting.

### SK2 regression found & fixed (folded into SK3 commit)

`sketch_profile_test` failed after the SK2 anchor-t solver mapping
(WLC + host-length pin) went live:

- **Symptom 1:** `test_midpoint_anchor_follows_host_length_change` —
  after driving the host line's dimension to 60, the whole sketch
  translated −6.73 in x (line5 at the host midpoint, but the host slid).
  Root cause: the WLC's pole gradients (±0.5 on host endpoints) let
  DogLeg's minimum-norm step satisfy the anchor by translating the
  under-constrained host instead of moving only the anchored point —
  the null-space trade. The post-pass then found the anchor already
  satisfied and couldn't correct the host.
- **Symptom 2:** `dofs=-6` at anchor creation — the host-length pin
  double-counts against a driving dimension on the host, and planegcs's
  diagnosis reports over-constraint (0 conflicting/redundant — the frozen
  append-focus points explain the arithmetic, the pin explains the
  conflict on subsequent drives).
- **Fix:** reverted the anchor solver mapping to the pre-SK2 design —
  `PointOnLine`-only for midpoint and point-line anchors, no WLC, no
  host-length pin. The deterministic `enforce_midpoint_anchors` /
  `enforce_point_line_anchors` post-passes position the anchored points
  after every solve (they were the proven mechanism all along; PointOnLine's
  perpendicular residual is zero right after host edits, so the solver
  leaves the anchor alone). The SK2 `constraints` kind "midpoint"
  (perp-bisector + pin) is untouched — it has its own suite and anchored
  setups. `dof_counter` unchanged (its anchor counting predates SK2).
- Regression test: the pre-existing `sketch_profile_test` midpoint-anchor
  cases reproduced the failure (fail-before/pass-after). All 19 suites
  green + tsc clean after the fix.

**SK6 (dimension completion) — implemented, UNCOMMITTED**: diameter
display via `display_as` (stored value stays the radius — conversion
at the IPC boundary: payload emits D, parser stores D/2, numeric
update halves), arc_length end-to-end (creation, update branch,
driven re-measure, "L" viewport label), arc_angle update dispatch
branch. UI: dimension-tool dropdown modes radius/diameter/arc-length.
`cad_core_dimension_completion_test` (3 cases, both epsilon sides).
All 27 suites + tsc green. Commit blocked pending explicit user
approval + in-app verification (auto-mode classifier).

**SK7 (spline) — in progress, core + tests written, UI wired, tsc
green, core rebuild in flight**:

- `SketchSpline` entity (poles = regular movable vertices, degree =
  min(3, n-1), clamped open-uniform knots — `spline_math.h` shared by
  the walk/viewport/wire builder). No solver registration (plan).
- Profile engine: `ExactCurve::Kind::kSpline` — de Boor point/tangent,
  shoelace area, OCCT intersections via `spline_profile_occt.cpp`
  (separate TU so the walk stays OCCT-header-free; sketch_profile_test
  now links OCCT), touch records, endpoint pre-union, dangling drop,
  boundary edges carry the poles; region kind "spline".
- Wire builder: exact `Geom_BSplineCurve` edge trimmed to the walked
  sub-span (feature_shape.cpp).
- Lifecycle: rebuild emits poles; vertex sync + solver writeback +
  connected-point move + transform move/copy re-fit the poles; delete
  path (pole click deletes the whole spline); serialization parser +
  emitter; viewport primitive (sampled polyline + poles); IPC handler
  + doc wrapper + schema + AI schemas; whitelist "spline".
- `cad_core_spline_test` (9 cases: creation, pole-count validation,
  pole-drag re-fit, full profiles_match single spline-bound region,
  crossing-line split, extrude smoke, move, trim rejection, save/load
  round-trip).
- UI: SplineIcon + toolbar entry + i18n; click-to-place-pole draft
  with a REAL B-spline preview (TS de Boor mirroring spline_math.h),
  click the first pole to commit, Escape cancels; scene render +
  picking; threading through ViewportPanel/pointerUp/callbackRefs/
  App/useCadCore.
- Deferred: spline mirror (mirror only folds lines/circles today —
  same as arcs/ellipses), spline×ellipse intersections (pre-existing
  ellipse gap), trim/extend/offset on splines (clear rejection).

### SK7 draft-UX rework (user-reported, fixed)

- Commit discoverability: double-click, Enter, tool-switch (commits
  instead of discarding), and click-first-pole all commit; Escape
  still cancels. Snap label hints "Double-click or press Enter to
  finish" after 2+ poles (i18n `viewport.splineFinishHint`).
- Draft now mouse-follows: the move path rebuilds the preview with a
  dashed rubber segment last-pole→cursor (splineDraftPreview
  `cursor`), duplicate consecutive poles deduped.
- FOLLOW-UP (user-reported, still open): the rubber line preview is
  wired through draftPointerPreview but does not render in-app —
  investigate after the SK6/SK7 commit (suspects: the pointer-move
  path for the spline tool not reaching renderDraftPointerPreview, or
  the preview group being cleared right after rebuild).

### SK7 fixes folded in (before first green build)

- OCCT's in-tree build dir (occt8-build/inc) lacks the TColStd/
  TColgp array templates (deprecated headers, install tree only) —
  added `include_directories(occt8-install/inc)` in cad-core CMake.
- The vendored `Geom2dAPI_InterCurveCurve` has NO parameter accessor
  (reduced OCCT 8 API): spline-side params come from point projection
  instead; tangent overlaps processed via `Segment()` endpoints.

## SK8 — COMPLETE (2026-08-24)

Docs sweep landed: Implementation Log entries for SK4-SK7 (editing
tools, circle modes, dimension completion, spline + follow-ups),
AI command language entries for all new commands (ellipse/slot/
chamfer/spline, transform/arrays/extend/offset, arc dims, circle
modes + the diameter convention), IPC protocol bullets, the
set_sketch_tool enum, and the roadmap sketch-system entry.

## Post-SK8 follow-up — radial dimension rendering (2026-08-26)

User-reported: arc dimensions were stuck at a fixed position, drew as an
"ugly line", and only the text moved horizontally. SK6 shipped dimension
*values* but changed no rendering file. Fixed four defects: the core
emitters for `circle_radius` / `arc_radius` / `arc_length` ignored the
stored `label_x/label_y` (drag persisted, refetch reverted it); the TS
radius branch was a bare segment with no arrowhead or leader; the drag
pinned the label to a `radius + 4` ring; and two dropped-event bugs —
`arc_length` missing from the zod viewport enum discarded every
`viewport_state` event, `arc_angle` missing from documentStateSchema
discarded every `document_state` event, and `arc_angle` had no emitter at
all.

New `sketch_radial_dimension_primitives.inc` (arc radius / length / angle
emitters) + rewritten circle emitter, all honoring the stored label. No
new IPC fields: the radial kinds reuse `arc_center`/`arc_radius`/arc
angles, and `anchor_end` carries a quarter-turn rim point as an in-plane
direction reference. The leader landing is derived in the renderer (it is
presentational, and duplicating it in the core would give the preview and
the re-emit something to disagree about). Both mirrored preview
projections collapsed into shared helpers mirroring the C++ constants.
`dimension_completion` suite 3 → 9 cases. All 28 suites + tsc green,
user-verified in-app.

## Extrude thin-wall regression — intersecting arcs (2026-08-26)

User-reported: two intersecting arcs enclosed by two lines (res/part.json)
extruded with part of an arc as a thin wall. Root cause: the face walk
assigned the arrangement's EXTERIOR cycle (area ~14372, larger than the
lobe itself) as the lobe's inner loop, because the exterior's probe point
lies exactly on the lobe's own boundary and the ray-cast rounded it onto
the inside. The bogus hole cut the lobe face down to a sliver.

Fix: `exact_point_on_polygon_boundary` guard in the hole-assignment loop
of `sketch_profile_exact.inc` — a probe ON a candidate's boundary is the
exterior twin of that region, never a hole of it.

Two test-side consequences, both verified with OCCT probes:

- New suite `cad_core_intersecting_arcs_extrude_test` — the exact
  part.json geometry via the direct constructors, asserting the full
  region set (lens ~2479 / lobe ~5105 / big U ~6789) and that the lobe
  carries NO inner loop. Fails on the pre-fix walk (hole area 14372.5).
- `multi_profile_extrude_test`: the corner-touch new_body case now
  expects TWO solids inside the single body entry. The old "one solid"
  was an artifact — the spurious hole destroyed the first prism. Two
  prisms touching along one edge legitimately stay two solids (a
  non-manifold union is not a valid solid); the OCCT probe confirms
  this is the correct geometry, not a regression.

All 29 suites green. Awaiting user in-app verification + commit.

### Radial follow-up round 2 (2026-08-26)

User follow-up after the radial-dimension commit:

- `polygon_radius` joined the free-2D radial leaders
  (`make_polygon_radius_dimension_primitive` over `compute_radial_leader`;
  the old emitter projected the stored offset onto a hardcoded (0,1)
  normal, so only the Y component survived). While testing this, found
  and fixed a pre-existing emitter bug: `find_if` matched an entity's
  AUTO dimension first and the `!is_auto` guard then suppressed the
  explicit dimension entirely — polygons (and circles) carried both, so
  the explicit one never rendered. The lookup now skips auto dimensions
  in the predicate.
- Circle draft-diameter preview was reworked to the new field convention
  (it was still emitting the old left/right-rim anchors with no
  arc_center; under the new renderer that drew the leader across the
  whole circle from the far rim).
- Arc draft preview: the three-point arc now shows a chord-length
  dimension readout while placing the second point — the readout between
  the two end vertices that predated the arc tool rework. Radius draft
  readouts stay out (Fusion doesn't have one either).
- `dimension_completion` suite grew to 10 cases (polygon free-2D label +
  explicit-over-auto lookup). All 28 suites + tsc green.

## Branch state — FINALIZED

`feature/sketch` is 10 commits ahead of `dev`; every milestone was
gated on full suites + tsc and user-verified in-app. Suites total 28
(tool_whitelist, parametric_arc, constraint_completion, ellipse,
slot, sketch_chamfer, extend, offset, transform, array, circle_modes,
dimension_completion, spline + the 15 pre-existing). The user ran the
final verification pass on the spline close gesture, the rubber
preview, and the extrusion surface.

Deferred (tracked): chain/loop offset, slot draft radius input,
slot/ellipse drag previews, spline mirror, spline×ellipse
intersections, the SK5 in-app tangent-circle surface check (suite
green — re-check with PS_TRACE_FACES if it reappears).

After merge of this branch: squash-merge to `dev`, delete
`feature/sketch`, next branch per user.
