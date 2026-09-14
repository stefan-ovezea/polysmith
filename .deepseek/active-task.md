# Active task: sketch selection fixes + projection heal — CODE COMPLETE, user verification pending (2026-09-14)

> **Branch:** `fix/sketch` (from `dev` @ 69aa509)
> **No commits yet — no commit without user verification + explicit
> approval (CLAUDE.md).**

## User-reported problems (res/part-stefan.json — mesh→body face
projection, 910 projected lines, 55 profiles; the projected outline is
being replaced with simplified geometry, then deleted)

### 1 — "Surface detection" consumed clicks; inner geometry unselectable

- Root cause A (hover): Fusion highlights nothing on hover — surface
  detection belongs to the click.
- Root cause B (click): entity picks took `intersectObjects(...)[0]`.
  Three.js sorts line hits by along-ray t, NOT by distance to the
  click. Coplanar sketch geometry (head-on view) makes every t equal,
  so the stable sort keeps array order and the outer loop's line
  (emitted first by the projection) won over the inner geometry the
  user clicked; clicks that missed every line fell through to the
  profile pick.
- Root cause C (drag): marquee selection only started on empty space
  (`!hit`). Click-drag beginning on a profile or entity did nothing —
  in Fusion any click-drag is a marquee.

**Changes (UI only):**
- `sceneTargetPicking.ts` — new `nearestEntityIntersection()` helper:
  picks the hit with the smallest perpendicular distance
  (`raycaster.ray.distanceToPoint(hit.point)` — three sets `hit.point`
  on the segment). Used in both `pickActiveSketchTarget` entity
  branches and both inactive-scene entity picks.
- `pointerMoveHover.ts` — `applySelectToolHover` RESTORED the profile
  (surface) hover in round 3 (see below): the highlight is needed to
  make the click target visible, otherwise Delete invisibly targets
  the surface boundary.
- `selectPointerDown.ts` — marquee drag now also starts on
  `sketch_profile` / `sketch_entity` / `sketch_point` hits (projected
  vertices are fixed, so drags from them can't be endpoint moves); a
  plain click still falls through to the pointer-up click selection.

### 2 — Deleting projected geometry flagged the sketch "link deleted"

User rejected auto-unlink-on-delete (some projections must stay
linked; unlink would silently change projected styling). Agreed
solution: **keep the live link + the flag on partial delete**, and add
an explicit heal — right-click on sketch geometry →
**"Remove projections"** (deletes ALL projected entities — the user
covers the projection with own geometry and wants the small projected
arcs/lines gone in one shot) and **"Unlink projections"** (keeps
entities, drops only the live links).

**Changes (core):**
- NEW command `remove_sketch_projections { feature_id, keep_geometry }`:
  - `keep_geometry=false` ("Remove"): collects every generated id from
    the projection records, then reuses `delete_sketch_selection`
    (temporarily points active-sketch at the feature, restores after)
    so all cleanup rules apply (fillets/slots/texts, dimensions,
    constraints, anchors, record pruning). Clears `projected_sources`.
  - `keep_geometry=true` ("Unlink"): clears the records +
    `projected_sources`, refreshes derived state, then clears
    `is_projected` + source fields on all vertices (plain styling).
  - Both clear the dependency_broken alarm via the walker on the next
    bump.
  - Files: `project_face_commands.inc` (impl),
    `document_manager_projection_parameter_commands.inc` (decl),
    `sketch_create_projection_command_handlers.inc` (app dispatch),
    `commands.schema.json` (enum). Note: the app side had no
    `read_bool` reader — added to `app/impl/command_reader_basic_helpers.inc`
    (the protocol-side `json_helpers.inc` read_bool is only visible
    inside serialization.cpp).
- `sketch_delete_selection_commands.inc` — restored the original
  per-id pruning (record dropped when empty; partial delete keeps the
  link and may flag — accepted).
- Docs: `wiki/IPC-Protocol.md` + `wiki/AI-CAD-Command-Language.md`
  updated.

**Changes (UI):**
- `types/ipc.ts` `RemoveSketchProjectionsCommand`; `ipcProtocol.ts`
  builder; `useCadCore.ts` `removeSketchProjections(featureId,
  keepGeometry)`; `aiCommandPayloadSchemas.ts` + `aiCadPrompt.ts`.
- Right-click menu: `ViewportContextMenu.tsx` two entries
  ("Remove projections" / "Unlink projections", shown only when the
  active sketch has projections) wired through
  `viewportContextMenuActions.ts` → `ViewportPanelShell.tsx` →
  `ViewportPanel.tsx` (`removeSketchProjectionsRef` +
  `showSketchProjectionActions`) → `App.tsx` (handler resolves the
  active sketch id; visibility from `sketch_parameters.projections`).
- i18n: `sketch.removeProjections` / `sketch.unlinkProjections` in
  en.json (other locales fall back to English).

**Tests (`face_projection_arc_test.cpp`):**
- `ProjectionHealFixture` — rounded-rect extrude, top-face sketch,
  projected face (4 lines + 4 arcs) + one hand-drawn line.
- `test_remove_projections_deletes_projected_geometry` — remove mode:
  records gone, projected lines+arcs gone, hand-drawn line survives,
  no alarm.
- `test_unlink_projections_keeps_geometry_and_clears_alarm` — partial
  delete flags the sketch (pinned as accepted), unlink mode: records
  gone, entities stay, vertex styling cleared, alarm cleared.

## Gates run (all green)

- VS-wrapper build OK (document.cpp + app.cpp touched after .inc
  edits — stale-build trap)
- `pnpm test:core` (CAD_CORE_TEST_JOBS=1) — **46/46 suites**
- `pnpm --filter desktop-ui exec tsc --noEmit` — clean

### 3 — User round-2 findings (2026-09-14, after app testing)

- **"Delete not working / entities still visible":** NOT a code
  regression — the core delete was verified correct (scratch test
  loaded the user's saved file, selected interior polylines, deleted:
  they were removed cleanly). Root cause: the DOUBLED geometry. The
  saved file had two stacked projections (`projection-3` face 906
  lines + `projection-4` body-section 1180 lines). Deleting a line
  removed one of the two coincident copies; the other stayed rendered
  at the same position → "delete does nothing".
- **"Deleting inside geometry deletes the perimeter"** (the confirmed
  bug): a click on a region INTERIOR selects the PROFILE (surface);
  `select_sketch_profile` did not clear prior entity/vertex
  selections, so Delete removed BOTH the previously-selected inner
  geometry and the surface's boundary (the 4 perimeter lines — the
  "highlighted for one second then deleted" ones). With the surface
  hover restored, the user can now SEE what the click selects; the
  fix clears the stale entity selection so Delete only removes the
  surface boundary. (The "undo twice then it works" symptom: undo
  rebuilt state without the stale multi-selection.)
  **Fix:** `profile_selection_commands.inc` — non-additive
  `select_sketch_profile` now clears `selected_sketch_entity_ids` +
  `selected_sketch_vertex_ids`. Test:
  `cad_core_selection_test.cpp:test_profile_click_replaces_entity_selection`
  (verified fail-before/pass-after).
- **Double projection:** `project_body_into_sketch` had no guard
  against existing links — its same-source idempotency only matches
  `body:<id>:<mode>`, so a body-section re-projection stacked over the
  face projection (different source id) doubled the entities.
  **Fix:** after the same-source no-op, `project_body_into_sketch`
  now REFUSES when the sketch has any NON-BODY projection record
  (face/edge/vertex — `source_kind != "body"`). Scope matters: the
  first blanket guard broke `stl_import_test` "both modes coexist" —
  section + silhouette body projections coexist BY DESIGN. Body+body
  stays allowed; the targeted link blocks the whole-body
  re-projection.
- `app/impl/command_reader_basic_helpers.inc` — added app-side
  `read_bool` (the first full build failed with C3861; the
  protocol-side read_bool is only visible in serialization.cpp).
- Test: `test_body_projection_refused_over_targeted_projection` — box
  STL → convert_mesh_to_body → sketch ON the top face → face
  projection (the targeted link) → body projection REFUSED → heal →
  silhouette projection works again on the same sketch.

## Round-4 notes (2026-09-14, after user round-3 testing)

1. **Surface hover removed in sketch mode (Fusion behavior).**
   `applySelectToolHover` no longer sets the profile hover — over a
   dense projection it flickered on/off as the pointer crossed
   projected lines. The CLICK decides surface selection (pick order:
   entity/point first, profile fallback). Scene/3D-mode hover
   (`applySceneHover`) unchanged.

2. **Delete race — perimeter deleted "not all the time".** The hotkey
   delete built its payload from the UI document snapshot, which can
   lag the last marquee while its selection events are in flight. A
   stale snapshot contained a pre-marquee PROFILE selection → the core
   deleted the profile's boundary (the perimeter) while the marquee's
   entities survived. Fixes:
   - Core `delete_sketch_selection`: EMPTY id lists now resolve the
     CURRENT selection at command time (singular+plural for
     entity/vertex/profile). Explicit-id callers (context menu, trim,
     heal) unaffected.
   - UI: `deleteSelectedSketchItems` (sketchHotkeys) always calls the
     no-snapshot form; `confirmAndDeleteSketchSelectionFromContext`
     resolves the selection only for the dependents check and sends
     empty ids; the pending-confirmation dialog carries
     `deleteCurrentOnConfirm`.
   - Test: `cad_core_selection_test.cpp:
     test_delete_empty_ids_resolves_current_selection` — verified
     fail-before (empty delete = no-op) / pass-after.

3. **Edge + vertex projection for converted mesh bodies.** The
   viewport gate blocked edges/vertices for mesh_to_body (only
   full-face projection was available). Now mesh_to_body emits
   edges/vertices filtered to the SEMANTIC ones: new
   `core/viewport/facet_edge_filter.h` (FacetEdgeFilter) drops edges
   shared by coplanar faces (triangulation seams) — small meshes are
   already unified at conversion (ShapeUpgrade_UnifySameDomain), so
   the filter matters for parts above the 50k-face unify limit or
   failed unification, the exact payload-flood case. Vertex emission
   keeps only endpoints of kept edges (outline corners).
   - OCCT 8 gotcha: TopTools_* collection typedefs were removed —
     MapShapesAndAncestors fills
     `NCollection_IndexedDataMap<TopoDS_Shape,
     NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>`.
   - Tests (stl_import_test): test 21 converted body emits semantic
     edges/vertices + end-to-end edge/vertex projection into a sketch;
     test 22 FacetEdgeFilter contract on a hand-built split-top box
     (13 edges → 12 kept, the coplanar A-C seam dropped).
   - **Found + fixed while testing: `project_edge_into_sketch` never
     pushed its projection record** (the record is built, generated
     ids filled, vertices fixed — then never registered; the "record
     moves into the projections list" comment proves the lost
     push_back). Edge projections now create live links (alarm/heal/
     guard coverage).

4. Gates: 46/46 C++ suites + tsc clean (2026-09-14).

## Round 5 — projection performance (2026-09-14)

Edge projection took 10-15s on part-stefan. Measured with a scratch
timing test on the user's saved file: `build_viewport_state` = **8.7s**
per revision bump (cache hits 55ms — why zoom/rotate stay fast). The
new edge/vertex enumeration was the cost, specifically:
- `FacetEdgeFilter` built TWICE (edges + vertices passes) and computed
  a fresh surface adaptor + normal per edge-keep check.
- The per-vertex circle-seam probe (`is_nonsemantic_circle_seam_vertex`)
  rebuilds the WHOLE edge map per vertex — O(V×E), ~1.4s on 2360
  vertices. The per-edge seam-line probe is O(E×F) similarly.
- BRepGProp linear properties per kept edge (3516×).

Fixes: one shared filter instance with a per-face normal cache; seam
probes skipped for faceted (mesh_to_body) bodies (closed-surface seams
cannot exist on chorded STL solids); polyline-based edge length for
facet bodies. Result: **8.7s → ~0.46s** on the user's part (3516
edges / 2360 vertices / 1182 faces). Serialization ~70ms (5.4MB JSON
— dominated by face triangle data, pre-existing). All timing
scaffolding and the scratch test were removed after diagnosis.

5. Gates re-run: 46/46 C++ suites + tsc clean (2026-09-14).

## Round 6 — vertex projection invisible (2026-09-14)

User: face projection fast, edge OK, vertex "does not work or never
comes back". Verified the CORE path end-to-end on the user's saved
part (scratch repro: project_vertex_into_sketch on a viewport vertex
id lands the point + record). Root cause is UI-side: body vertex dots
are fixed WORLD-space spheres (radius 1 × 0.25 units) — sub-pixel on
a fan-panel-sized part, so corner clicks fall through to the adjacent
edge/face picks and the vertex target is effectively unhittable.
Fix: `ViewportPanel.render()` now scales every vertex mesh to a
constant SCREEN size (4px radius, 5px hovered/selected) via
worldUnitsPerPixel — same behavior as sketch points.

While diagnosing: `app.cpp` error events carried an EMPTY id — the UI
correlates responses by id, so a throwing command never settled the
awaited promise ("never comes back"). The run loop now parses the
command BEFORE the try and threads `command.id` into the error event.
(The store also toasts any error event, so failures now surface.)

6. Gates re-run: 46/46 C++ suites + tsc clean (2026-09-14).

## Round 7 — projected vertices invisible + sketch-on-face visibility (2026-09-14)

User: "I have projected 3 vertex but I cannot see them in the sketch
but the info panel says 3P" + the sketch on the part face "looks like
is on top and the sketch under" (grid only visible through the holes,
lines invisible over the body) + "I cannot start the selection square"
over the body, and a marquee started on empty sketch area "does not
select anything" over the body.

1. **Projected points never emitted (core).** The viewport emission
   skip set `projection_vertex_ids` is meant for line/arc/circle
   ENDPOINTS (derived bookkeeping — the line carries their visual).
   It ALSO contained `projection.generated_vertex_id` — for a
   standalone projected POINT that id IS the entity, with no line to
   carry it, so it was skipped → invisible while the info panel still
   counts it ("3P"). Fix: `sketch_feature_emit.inc` no longer puts
   `generated_vertex_id` into the skip set.
   - Test: stl_import_test test 21 extended — asserts the projected
     point's `generated_vertex_id` appears in
     `build_viewport_state().sketch_vertices` (fail-before/pass-after).

2. **Sketch grid invisible over the body (UI, render order).** The
   grid/axes/labels are line renderables; their renderOrder was set on
   the parent GROUP, and three's transparent sort reads renderOrder
   from each renderable — the group value never propagated, so
   children sorted at 0, UNDER the body face fills (4) → grid visible
   only through holes. Fix: `grid.ts` sets renderOrder on the
   renderables themselves (grid 6, axes 5, tick-label sprites 6 —
   above face fills 4, below sketch entities 7). Removed the
   ineffective group-level assignments from `dynamicGridUpdate.ts`.

3. **Contrast (the user's own diagnosis — "the color of the body").**
   Sketch entity resting colors were `--color-tertiary-plane-fill`
   (#fff7c0 dark / #fff3a6 light — the pale profile-FILL token).
   Switched the 6 entity base colors in `sketchObjects.ts` + the
   resting branch of `paintSketchEntityMaterials`
   (`viewportVisualState.ts`) to `--color-tertiary-plane-edge`
   (#ffe784 dark / #d9b600 light — the darker EDGE token). The profile
   fill material (`sketchObjects.ts` ~line 802) deliberately
   unchanged. Light-body themes: light.json grid tokens darkened
   (#c5d8df→#7d9aa6, #94aeb8→#5b7d8a, #5d7b86→#3e6272);
   catppuccin-latte likewise with palette colors
   (#ccd0da→#8c8fa1, #9ca0b0→#7c7f93, #8c8fa1→#6c6f85) — body is
   #bcc0cc there, the old grid was LIGHTER than the body.

4. **Marquee over the body.** With a sketch active, a pointer-down on
   BODY geometry (face/edge/vertex hit) now starts the marquee too
   (`selectPointerDown.ts`) — before, only empty space / sketch
   hits did, so the selection square couldn't start over the body on
   a sketch-on-face. 3D mode (no active sketch) keeps its own rules
   (marquee from empty space only). Note: the pointer-down on the
   body now marquees INSTEAD of the old body-targeting behavior in
   sketch mode — that's Fusion behavior; a plain click (no drag)
   still falls through to the click pick.

5. Gates re-run: 46/46 C++ suites + tsc clean (2026-09-14).

6. **Follow-up (user: "projected vertex does not take same color
   like the projected line").** Projected points were deliberately
   painted `--color-axis-z` blue ("derived from body vertex"
   convention). User wants them to match projected entities. Fix
   (UI-only): `buildSketchPointObject` (creation material) and
   `paintSketchPointMaterials` (interaction repaint) now use
   `--cad-sketch-projected` for `kind === "projected"`; center/
   quadrant points keep axis-blue. tsc clean; awaiting in-app
   confirmation.

## Round 8 — Array toolbar button (2026-09-14)

User: "I remember I have implemented array in the sketch but I
cannot find any button." Investigation: NOT lost — the array was
fully implemented and shipped:
- Core: `create_linear_array` / `create_circular_array` (exploded
  copies, count 2-100, Fusion-style circular spacing) + tests —
  commit cf39375 (#68, "sketch toolset finalization").
- UI: IPC types (`types/ipc/sketchCommands.ts:444`), builders
  (`lib/ipc/sketchCommands.ts:1042`), hooks (`useCadCore.ts:1185`),
  and `SketchTransformPanel` with Linear Array (dx/dy/count) +
  Circular Array (center/total-angle/count) sections wired to the
  core (`App.tsx` onApplyLinearArray/onApplyCircularArray).

The ONLY missing piece was discoverability: the panel opened
exclusively via right-click → Move/Copy. Fix (UI-only, no core
change): an Array toolbar button next to Mirror in the sketch
constraint group, opening the same panel:
- `types/geometry/contraints.ts` — `ConstraintType` gains `"array"`
  (same editing-op comment pattern as mirror).
- `ToolBarIcons.tsx` — `ArrayConstraintIcon` (3×2 dot grid) +
  `case "array"` in `ConstraintIcon`.
- `SketchConstraintControls.tsx` — Array button after Mirror; new
  props `isArrayPanelOpen` + `onStartArrayTool`.
- Prop chain threaded through `SketchToolbar` → `AppHeader` →
  `AppTopBar` (mirrors the `isMirrorToolOpen` pattern).
- `App.tsx` — the inline `onOpenTransformArray` handler extracted
  to a named `openTransformArrayPanel()` shared by the context-menu
  and toolbar entries; passes `isArrayPanelOpen={sketchTransformPanel
  !== null}` + `onStartArrayTool`.
- `i18n/en.json` — `toolbar.array` label.

Gates: tsc clean + en.json parses. Awaiting in-app verification
(button opens the panel, Linear/Circular Array still work from both
entries).

## Next session checklist

1. **User verification in the app (binding per CLAUDE.md):**
   - Sketch mode: NO surface hover flicker; hovering lines/points
     only; a click on region interior selects the surface (boundary
     highlight shows on click); click-drag anywhere = marquee.
   - Window-select inner polylines → Delete immediately: only the
     polylines go, the 4 perimeter lines stay (repeat fast to hit the
     old race).
   - Converted body: hover/pick individual outline EDGES and corner
     VERTICES in Project mode; project an edge → line appears AND the
     link lives (delete it → sketch flags, heal clears); project a
     vertex → fixed point. **Projection should now land in well under
     a second** (was 10-15s; core viewport rebuild on this part went
     8.7s → 0.46s).
   - Vertex dots are now constant SCREEN size (like sketch points) —
     clicking a corner must hit the VERTEX, not the adjacent edge.
   - **Round 7:** projected vertices are VISIBLE as sketch points in
     the active sketch; on a sketch-on-face the grid + axes are
     readable OVER the body (not only through holes); sketch lines
     contrast against the body; the selection square can START on any
     sketch area including over the body, and the marquee over the
     body selects + highlights entities.
   - Full-face projection still available; the 3D-mode face hover
     behaves as before.
   - Body re-projection refused while targeted links exist; Remove/
     Unlink via right-click re-arms it and clears the alarm.
2. Commit with explicit user approval (no Co-Authored-By trailer),
   then PR `fix/sketch` → `dev` (squash), delete the branch.

## Known gaps (not fixed — scope)

- Trim/extend of projected entities still doesn't prune projection
  records (same count-mismatch alarm path); the heal command covers
  it.
- Scene mode (no active sketch): profile pick still runs before model
  pick — entity picking of inactive sketches stays gated behind
  `inactiveSketchEntityPickEnabled` (action modes / CAM).
