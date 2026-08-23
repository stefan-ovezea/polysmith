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

## Commit guidance

SK3 (ellipse + slot + chamfer + the SK2 anchor-mapping revert) is one
commit, pending user approval + in-app smoke. SK3 verification checklist:
draw an ellipse (3 clicks), extrude it (smooth, analytic edges); draw a
slot, extrude; chamfer a corner, edit both distances; midpoint-anchor a
line to another line's midpoint, then resize the host via its dimension
(anchored line must follow, sketch must not drift); undo/redo each.
After merge of this branch: squash-merge to `dev`, delete `feature/sketch`,
next branch per user.
