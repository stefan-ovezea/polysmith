# Active task: CAM retract WCS fix + generate-result popup + arc lead sweep angle + circle pierce corner-warning fix + mixed-sketch circle kind fix + circle-hole double-rendering fix + circle-hole exact toolpath fix + sketch-circle double-outline UI fix (cam/milling) — implemented, UNCOMMITTED

> **Branch:** `cam/milling` (HEAD 0bc3ad8)
> **Date:** 2026-09-08
> **Plan:** approved plan at `.claude/plans/woolly-crunching-balloon.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

Two user-reported items (2026-09-08), both implemented, both
uncommitted in the working tree:

1. **Generate-result popup**: pressing Generate shows a popup —
   "generated without problem" or "generated with warnings/errors".
   **User verified in-app ("it works well")** — popup gate passed.
2. **Retract-height WCS bug**: origin at world z=20, boss top 30 +
   3 mm stock → stock top 33, retract 25 warned and only 35 passed.
   Root cause: the generators compared the RAW retract height (a
   machine-Z-above-origin value) against WORLD face/stock heights;
   the post subtracts the origin (machine = world − wcs_origin), so
   the exported rapids sat at machine z=5 — inside the stock.  Fixed:
   retract is now WCS-relative everywhere.

## Status — implemented, uncommitted (working tree)

- **Popup (user-verified)**: `make_cam_generation_result_event`
  (protocol/ipc.{h,cpp}) `{op_id, ok, error_message, warnings[]}`;
  emitted by cam_commands.inc after generate (NEVER preview) —
  failure branch and after the document_state reply (so the UI's
  stats lookup sees the fresh toolpath_cache); existing error event +
  log_warn fan-out untouched; events.schema.json enum entry.
  CamGenerationResultEvent type + zod schema + coreMessageSchema
  union; cadCoreStore `generationResult` + `dismissGenerationResult`;
  CamGenerationResultPopup modal (success / amber warnings list /
  danger failure, stats line, Escape/OK/backdrop dismiss) mounted in
  App.tsx; `dialogs.generateResult.*` i18n; export G-code success
  toast (the path previously had NO visible feedback).
- **Retract WCS fix**: `cam_planning::setup_retract_plane_z(setup)` —
  world retract plane = `wcs_origin.position.value_or({0,0,0})[2] +
  retract_height` (doc comment carries the post contract).  All three
  mill generators (contour/pocket/face milling) use it for guards AND
  emission; the warning strings now print the setup value AND the
  resolved plane ("Retract height (25 mm above the setup origin)
  reaches 45 mm — below the stock top (33 mm)…").  Unresolved origin
  → {0,0,0}: legacy behavior unchanged.  **User verified in-app
  ("it works well").**
- **Preview arc smoothness fix**: cam_toolpath_emit.inc viewport chord
  tolerance 0.05 → 0.005 mm (kPreviewChordToleranceMm) — 0.05 mm left
  ~8° facets on a 20 mm circle; the pinned arc_segments_per_circle
  path is untouched (pinned counts must show the real faceting).
  Viewport-only; G-code export unaffected (true arcs / pinned count).
  **User verified in-app ("yes it works now").**
- **Arc lead sweep angle (user request 2026-09-08)**: kerf-inside +
  arc leads curled a full 270° (the spoke-based roll geometry) —
  "excesiv".  New `lead_in_arc_angle_deg` / `lead_out_arc_angle_deg`
  (default 90) on LaserCutParameters + serde both directions; the four
  arc branches in laser_leads.cpp now construct the entry/exit radius
  direction as the pierce radius direction rotated by the requested
  sweep, so the emitted arc sweeps exactly the input value (90° =
  classic quarter roll; 270 reproduces the old interior curl).
  Measurement during the fail-before run also showed the OLD code
  swept 270° on the exterior LEAD-IN too (lead-out was 90°) — the new
  default fixes both.  UI: two number fields in CamLaserCutPanel
  (min 1 / max 360, disabled unless the style is "arc"; CamNumberField
  gained a max prop) + i18n keys.  **In-app verification pending.**
- **Arc sweep parse-error fix (user log 2026-09-08 15:34)**: a cleared
  field commits `Number("") === 0`; the zod `.min(1)` then rejected
  every document_state the core echoed back (parse-error cascade).
  Fixed three ways: zod loosened (core is the source of truth — no
  min/max, schema must accept what the core stores); the panel clamps
  on commit to [1, 360]; the core clamps the sweep at use via
  `arc_sweep_radians` (a stored 0° would emit a zero-length arc that
  some controllers turn into a full circle).  Rebuilt + 41/41 green.
- **Regression tests (fail-before verified)**: contour_2d_test 18,
  pocket_2d_test 10, cam_generators_test 51 — WCS z=20 / retract 25 /
  stock top 33: no retract warnings + rapids at world z=45; genuinely
  too-low retract still warns; default-origin legacy still warns.
  Verified failing against the pre-fix helper (temp revert) and
  passing with the fix.
- **Regression test (fail-before verified)**: cam_generators_test 52
  "arc lead sweep angle is configurable" — exterior roll at 135°
  sweeps 135° (measured sweep=270 on the old geometry); interior
  circle at the 90° DEFAULT sweeps 90° with the entry inside the cut
  line (r<4.9); interior lead-out at 120° sweeps 120° with the exit
  inside.  Sweep measured from the move I/J center + start/end angles
  (kind-aware).  Old geometry: Test 52 FAIL (sweep=270); fix: PASS.
- **Circle pierce corner-warning fix (user 2026-09-08: "we should not
  have polygon approximation anymore")**: the user's circle op logged
  "1 sharp corner(s) were excluded from pierce placement" + "Every
  corner is sharper than the pierce threshold — piercing mid-segment on
  the longest straight edge".  NOT polygonization — the contour was
  already one exact full-circle arc (a 16-gon would report 16 corners;
  the profile id's 16 keys are the circle's SAMPLE points, and the
  generator walks the exact boundary edge).  The corner classifier in
  `select_pierce_vertex` flagged the circle's artificial self-join
  vertex as sharp (interior = π → the |π − interior| pointedness guard
  rejects it), then printed the straight-edge fallback message even
  though the loop has no straight edges.  Fix: a loop lying entirely on
  one circle (same center/radius on every arc segment) has no corners —
  pierce by position rules alone, no corner warnings.  Regression:
  cam_generators_test 53 "circle in a mixed sketch has no corner
  warnings" — circle + separate rectangle (forces the polygon-kind
  region path the user hit), kerf inside + arc leads: no corner/
  tessellation warnings + the contour stays an exact full-circle arc at
  the offset radius.  Fail-before verified (clean pre-fix build: only
  Test 53 FAIL, warnings present); pass-after: all 41 suites green.
  **In-app verification pending.**
- **Mixed-sketch circle kind fix (user 2026-09-08: "Circle profiles in
  sketches that also contain lines/arcs keep kind:'polygon' with a
  16-point sample. it is looking very bad in cam preview. The path is
  circle and the circle is poligons. Why I need that?")**: the CAM
  preview drew the circle PROFILE as a 16-gon because the exact
  profile engine gated kind "circle" (center/radius, no sampled
  points) behind `circles_only` — a sketch-wide "no lines and no arcs"
  test inherited from the OCCT-v8 port (8d845a6).  The toolpath was
  always exact (boundary_edges carry the circle edge); only the
  preview region was sampled.  Fix in sketch_profile_exact.inc: the
  gate is now `(circles_only || circle_curve->kind == kCircle) &&
  holes.empty()` — hole-free full CIRCLE faces become kind "circle"
  even in mixed sketches.  Full ELLIPSES keep the circles_only gate
  (their boundary edges are ellipse-kind, which
  build_base_segments_from_edges cannot consume — the sampled points
  remain their only contour input), and hole-bearing circles stay
  polygon-kind (the UI polygon renderer draws the hole loops; the
  circle renderer has no inner-loop path).  Downstream verified safe:
  detect_sketch_profiles → circle viewport primitive → UI
  EllipseCurve smooth render; extrude kind "circle" branch works
  without source_circle_id; project/deletes find the circle via
  line_ids/boundary_edges; CAM capture/planning read the exact
  boundary edge.  Known degradation (pre-existing pattern): stored
  "profile-poly-circle-…" ids in OLD documents do not re-resolve to
  the new "profile-circle-…" ids (find_equivalent_profile has no
  polygon→circle bridge) — dependency_broken + warning per TNP
  doctrine, same as the mixed→circles-only transition today.
  Test updates: circle_modes (3 expectations polygon→circle),
  extrude_quality (small-selectable lookup, trim-extrude expectation
  set, touching-lines lookup), sketch_profile (2 counting loops),
  cam_commands (circle selection assertion), cam_profile_reference
  (swap by kind, capture without source id, Test 7 lookup),
  cam_generators 53 (lookup by kind), sketch_test_utils doc comment.
  **In-app verification pending.**
- **Circle-hole double-rendering fix (user 2026-09-08: "are you crazy?
  the circle is nou double: one circle one polyline")**: after the
  circle-kind fix the circle region drew smooth, but the CONTAINING
  polygon still drew the circle as its hole outline from the 16-point
  sample in inner_loops — two outlines at the same radius disagreed.
  Fix: the exact detector now records a `SketchProfileCircleHole`
  (loop_index + exact center/radius, same full-circle test the region
  classification uses — hoisted into a shared `full_circle_curve`
  lambda) alongside the sampled hole points.  The sampled points STAY
  (capture/area math and operations created before the descriptor
  existed depend on them — the attestation consistency constraint);
  the descriptor is rendering-only metadata.  Carried end-to-end:
  region → PolygonSketchProfile → ViewportSketchProfilePrimitive →
  viewport wire + document save/load serde (both directions, lenient
  parse) → zod (viewportState + documentState) → SketchProfileScene
  `circleHoles` → renderer draws circle holes from center/radius with
  the same 96-point smooth sampling as circle profiles (fill path AND
  outline), and withDisplayProfileHoles bases its loop checks on the
  smoothed loops.  Ellipse holes keep the sampled outline (a circle
  descriptor cannot express the minor axis).  Regression:
  cam_profile_reference_test Test 1 asserts the exact descriptor
  (center 10/5, radius 2, loop_index 0) + a full
  serialize/deserialize round trip preserves it.  Rebuilt (touched
  sketch_profile.cpp / viewport.cpp / serialization.cpp shells) —
  41/41 suites green; tsc clean.  **In-app verification pending.**
- **Circle-hole exact toolpath fix (user 2026-09-08: "nothing
  changed" after the hole-rendering fix)**: the region outline fix
  worked, but the LASER CUT PATH around a circle hole was still built
  from the 16-point sample (`build_base_segments_from_points` on
  `inner_loops`) — ~16 straight chords at the hole radius, the
  polyline the user saw on top of the smooth circle.  Fix in
  laser_generate.cpp: the hole loop now looks up
  `region.circle_holes` by loop index and, when present, synthesizes
  one exact full-circle BaseSegment (CW walk — hole interior on the
  right — so the auto kerf still offsets inward into the scrap;
  `conventional` reversal unchanged; centroid = the circle center).
  Non-circle holes keep the sampled path; the size gate now accepts a
  single full-circle arc.  Regression: cam_generators Test 2 (rect +
  circle hole) gained two assertions — the hole ring emits ONE exact
  FeedArc at the offset radius (1.9) and ≤2 straight moves may touch
  the ring (the old chord path put ~16 there).  Fail-before verified
  (pre-fix build: Test 2 FAILS at "hole ring emitted as an exact
  full-circle arc"); pass-after: 41/41 suites green.  Side effect:
  exported G-code now carries G2/G3 for circle holes (was straight
  chords).  **In-app verification pending.**
- **Sketch-circle double-outline UI fix (user 2026-09-08: "the path
  was good and circle but the circle of the sketch was double circle
  and polygons")**: pixel forensics on the user's screenshot
  (stdlib-only PNG decoder, PCA ridge separation, kink-angle
  measurement — cross-checked with `bl vision describe`) identified
  the double as TWO coincident UI outlines at the circle's radius,
  not a core-data problem (a fresh core driven on the saved part.json
  emits a fully smooth circle region + exact circle_holes): (A) the
  sketch ENTITY circle — peach `--color-tertiary-plane-fill`,
  64-point EllipseCurve; (B) the circle PROFILE edge loop — lavender
  `--color-tertiary-plane-edge-hover`, 96 points, raised to opacity
  0.98 only while hovered/selected (the CAM region is).  Different
  sample counts + colors at extreme zoom read as "one circle + one
  polygon".  Fix (UI-only, no core rebuild): circle-kind profiles no
  longer draw an edge loop at all — the entity circle draws the same
  boundary; hover/selection feedback stays via the fill; picking is
  unaffected (fill mesh carries the profile id + the analytic circle
  fallback in sketchProfilePicking).  Polygon holes that are exact
  circles (circleHoles) skip their edge loop too — the rectangle's
  hole loop was the third coincident candidate.  Sampling raised for
  smoothness at extreme zoom: entity circle 64→256, circle fill
  CircleGeometry 48→256, smoothProfileHoleLoop 96→256 (feeds the
  fill's hole boundary).  Files: sketchObjects.ts + viewportScene.ts.
  **In-app verification pending.**
- Docs: IPC-Protocol.md + AI-CAD-Command-Language.md bullets (popup);
  wiki/CAM-Development.md retract-semantics paragraph.

## Gates

- `pnpm core:build` — green (app closed to release the exe lock; rebuilt
  again for the circle-hole fix with the three owning shells touched —
  sketch_profile.cpp, viewport.cpp, serialization.cpp).
- Circle-hole fix: `pnpm test:core` 41/41 green again (incl. the new
  cam_profile_reference Test 1 descriptor + round-trip assertions);
  `tsc --noEmit` clean.
- Circle-hole toolpath fix: fail-before verified (Test 2 FAIL on the
  pre-fix build) → pass-after `pnpm test:core` 41/41 green; final
  `pnpm core:build` relinked cad_core.exe + spawn-path copy fresh.
- `pnpm test:core` — 41/41 suites green (incl. cam_generators 53,
  contour 18, pocket 10 — the arc-sweep, WCS-relative, and circle
  pierce cases; the mixed-sketch circle-kind change rebuilt the core
  with the touched sketch_profile.cpp shell and every suite passes
  with the updated expectations).
- `tsc --noEmit` — green (Setup panel retract hint: `cam.setup.retractNote`
  "Above the setup origin (WCS Z)." + i18n key; laser panel arc-angle
  fields).
- Double-outline UI fix: `tsc --noEmit` green (sketchObjects.ts +
  viewportScene.ts; UI-only — no core rebuild).
- **Popup in-app pass — user confirmed ("it works well").**
- **Retract fix in-app pass — user confirmed ("it works well",
  2026-09-08).**
- **Preview arc smoothness in-app pass — user confirmed ("yes it
  works now", 2026-09-08).**
- **Arc lead sweep in-app pass PENDING** — laser op, lead style Arc,
  kerf inside on a circle: the lead arcs must now be 90° quarter
  rolls (not 270° curls); change the arc angle fields and confirm the
  preview/G-code sweep follows.
- **Circle pierce warning fix in-app pass PENDING** — same op: the
  Logs panel must no longer show "sharp corner(s) were excluded" /
  "Every corner is sharper…" for a circle contour.
- **Mixed-sketch circle kind fix in-app pass PENDING** — a sketch
  with a circle PLUS lines/arcs: the circle's filled profile preview
  (sketch + CAM) must now render as a smooth circle, not a 16-gon.

## Next steps

1. In-app: laser cut, kerf inside, lead-in/out style Arc → the arcs
   roll 90° by default; try e.g. 120° in the new "Lead-in/out arc
   angle" fields and confirm the preview follows.  Regression: kerf
   auto (exterior) lead-in should now also be a 90° quarter roll
   (was 270° too).
2. In-app (same run): a circle op (circle + other entities in the
   sketch) must generate WITHOUT the "sharp corner" / "Every corner"
   pierce warnings — and the cut must still be a true circle arc in
   the preview/G-code.
3. In-app (same run): the circle profile in a mixed sketch must
   render as a smooth circle in the sketch and CAM previews (the
   16-gon polygon outline is gone).
4. In-app (same run): a circle inside a rectangle must show ONE
   smooth circle — the rectangle's hole outline must coincide with
   the circle region's outline (no second polyline).  The laser
   TOOLPATH around the circle hole must now also be a smooth circle
   (one exact arc — no chord polygon on the ring), and the exported
   G-code must carry G2/G3 for the hole.  Also verify after a
   save/load that the hole stays smooth.
5. In-app (same run): the circle must now be ONE smooth circle in the
   sketch and CAM views — no second outline when the circle region
   (or the containing rectangle) is hovered/selected; hover feedback
   shows as the fill highlight; the circle stays smooth at extreme
   zoom.
6. Commit on user approval — popup + retract fix + preview smoothness
   + arc sweep angle + circle pierce fix + circle-kind fix + the
   double-outline UI fix together (or split as the user prefers) — no
   Co-Authored-By trailer; message names the suites run (41/41 incl.
   contour 18 / pocket 10 / cam_generators 53).

---

# Active task: 2D Contour CAM operation (cam/milling) — implemented, COMMITTED (2026-09-08)

> **Branch:** `cam/milling` (HEAD fe6dbc9)
> **Date:** 2026-09-08
> **Plan:** approved plan at `.claude/plans/woolly-crunching-balloon.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

Next milestone in wiki/CAM-Development.md §V1: contour a closed wire on
a planar face, offset by the tool radius.  Binding user decisions
(2026-09-08): **both inputs in v1** (face pick AND sketch-profile pick)
and **exact arcs** (circular wires emit true G2/G3; ellipse/spline
edges fall back to chord-tolerance polylines).

## Status — phases 1-6 implemented, uncommitted (working tree)

- **P1** core params + serde: `ContourParameters` {side, depth_mm,
  stock_allowance_mm} as optional `contour` block on
  CamOperationParameters, serialized both directions.
- **P2** hoist + wire builder + generator: laser wire builders hoisted
  to cam_planning (laser suite stays green — the pin); NEW
  `build_base_segments_from_wire` (exact line/circle BaseSegments,
  `false` → polyline fallback); `contour_2d.h/.cpp` +
  `impl/contour_2d_generate.inc` registered in cam_generators.cpp.
  Semantics: face wins over profiles; largest |signed area| wire;
  CCW normalization, `reverse = (side=="inside") XOR
  (direction=="conventional")`, `d = conventional ? −r_eff : +r_eff`;
  on_line skips offset and emits the base loop; allowance read ONLY
  from the contour block; single pass at `planeZ − depth_mm`;
  guards (non-horizontal face, tool-doesn't-fit, self-intersect).
- **P3** command gates (only these two): cam_commands.inc create gate
  `!= "laser_cut" && != "contour_2d"`, update gate `== "laser_cut" ||
  == "contour_2d"` — profile-selected contour ops are created with
  EMPTY geometry_references and the core captures.
- **P4** `cad_core_contour_2d_test` suite (17 cases: all 4
  side×direction combos, on_line ± allowance, exact arcs both sides,
  spline fallback, profile inputs incl. left-handed frame, face-wins,
  depth default, empty/non-horizontal/multi-wire rejections, serde).
- **P5** UI: camContourActions trigger (face witness capture, else
  empty-region profile create); CamContourPanel (side/depth/allowance
  + feedrate/plunge/spindle, scope re-pick for profile input, armed
  face re-pick, CamStatusLine); CamFloatingPanels contour_2d branch
  (contour-block-aware onUpdate); App.tsx armed contour face pick with
  mutual disarm vs origin/WCS/pocket; AppTopBar/AppHeader/
  CamMillingToolbar threading (Contour button enabled on face OR
  profile selection); camPanelShared prefix union, CamToolbar type
  union, documentUiState/CamOperationPanel labels; full `cam.contour.*`
  i18n block.  `tsc --noEmit` green.
- **P6** docs: wiki/CAM-Development.md "## 2D Contour (2026-09-08)"
  section + tracker rows; AI-CAD-Command-Language.md; Implementation-Log.md;
  V1-Roadmap CAM paragraph refreshed; this tracker header.

## Verification (so far)

- `pnpm core:build` + `pnpm test:core` — 41/41 suites green (new
  contour suite 17/17 + all 40 existing incl. laser hoist pin).
- `tsc --noEmit` clean.
- COMMITTED 2026-09-08 on user approval, WITHOUT the in-app pass
  (user restarting — the in-app pass is owed, see next steps).

## Next steps

1. **Deferred by the user (2026-09-08): multi-pass stepdown for 2D
   contour.**  v1 is single-pass only — no stepdown field, no
   `plan_stepdown_levels` in the contour generator.  The user will
   implement it later.
2. In-app pass still owed: face/boss-face create, inside/outside/
   on_line live flips, depth shifts Z, sketch-profile create at
   sketch plane, G2/G3 in exported G-code, re-pick face, scope
   re-pick, delete, generate/export stats, Logs warnings; regression:
   face milling, pocket, laser.
3. **Tauri callback-id warning — investigate (user-reported
   2026-09-08):** Logs panel showed `[TAURI] Couldn't find callback id
   1154042992. This might happen when the app is reloaded while Rust
   is running an asynchronous operation.` — Tauri's invoke-callback
   registry got a response for a dead id: an async invoke command
   (file dialog, export write, …) resolved after the webview reloaded
   (dev-mode HMR / Ctrl+R). Our Rust code has no explicit
   Callback/Channel usage (grep: only `.invoke_handler` in
   src-tauri/src/main.rs:249 + lib.rs:12), so this is Tauri's own
   invoke plumbing — likely dev-mode noise but root it out anyway:
   reproduce by firing a long async command then reloading the view.
4. **Generate-result popup (user-requested 2026-09-08) — DONE,
   uncommitted:** `cam_generation_result` core event + result popup
   modal + export success toast (see the active task header at the
   top of this file).
5. **Retract-height WCS bug (user-reported 2026-09-08) — DONE,
   uncommitted:** retract is machine Z above the WCS origin; the
   guards/emission now use `setup_retract_plane_z` (world plane =
   origin Z + height) in all three mill generators + regression
   tests (contour 18 / pocket 10 / cam_generators 51).  See the
   active task header above.

---

# Previous task: 2D Pocket CAM operation + refinement (cam/milling) — COMMITTED fe6dbc9

> **Branch:** `cam/milling` (HEAD fe6dbc9)
> **Date:** 2026-09-08
> **Plan:** approved plan at `.claude/plans/glittery-coalescing-kernighan.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

User: "No PR. continue with implementation." — the next milestone in
the prioritized V1 CAM list is 2D Pocket.  Binding decisions: islands =
full spec from day one (user-pickable boss faces stored as
`avoidance_regions`); stepdown = reuse the face-milling multi-pass
machinery (stock top → face, last level pinned at face).

In-app feedback 2026-09-08 (their words, binding): a join boss on the
pocket face is avoided "like a hole" (the zigzag leaves a stock plug
that becomes a boss and blocks later drilling); the linear zigzag
around a round boss "will create artifacts and a circular motion will
be more appropriate"; islands added/deleted look identical to the
path.  → implemented the refinement below.

## Status — phases 1-5 implemented + refinement, uncommitted (working tree)

- **P1** shared `plan_stepdown_levels` helper + face-milling level-loop
  refactor (behavior pinned by cam_generators_test 47-50).
- **P2** `pocket_2d` generator (`pocket_2d.cpp` + `impl/pocket_2d_generate.inc`)
  + `cam_generate.cpp`/`cam_refresh.cpp` avoidance-region resolution.
- **P3** `cad_core_pocket_2d_test` suite (12 cases) + cam2d CCW-clip pin.
- **P4** UI: `camPocketActions.ts` trigger; pocket button in
  CamMillingToolbar; `CamPocketPanel`; armed pocket face pick
  (outer/island) in App.tsx with mutual disarm vs origin/WCS picks;
  `cam.pocket.*` i18n.
- **P5** docs: wiki/CAM-Development.md section, AI-CAD-Command-Language.md,
  Implementation-Log.md, tracker (this file).
- **Refinement R1 — inner-wire boss/hole classification**
  (`pocket_2d_generate.inc`): floor-face inner wires probed via their
  adjacent walls (edge→face ancestor map, floor excluded); wall COM
  above the floor (`faceZ + 0.1`) → boss → avoided; all walls
  below/coplanar → open hole → rows mill ACROSS it (clears the stock
  plug); no probe → conservative avoid.  Tests 2/2b/2c.
- **Refinement R2 — finishing contours**: after the rows at every
  level, closed climb contours around each active avoidance (CCW, as
  traced) + the outer inset wall (CW, reversed); each segment clipped
  against the other active avoidances (never itself — tests 3/4
  caught the unclipped version riding into a grown near-wall island),
  surviving pieces chained (1e-6) and rapid-plunge-feed-rapid.
  Unconditional, also in single-pass mode.
- **Refinement R3 — island single-pass hint**: core warns when an
  island lies on a boss already owned by the pocket face (only
  multi-pass flush levels would change anything); UI
  `cam.pocket.islandSinglePassHint` beside the island list when
  islands exist without a stepdown.
- **Laser pierce corner fix** (same working tree): mitered-reflex-corner
  interior detection (`base_corner_interior` three branches in
  laser_leads.cpp) + Test 23 comment / Test 25 fixture corrections.

## Verification (so far)

- `pnpm test:core` — all 40 suites green (pocket suite 12/12;
  cam_generators_test 50/50 incl. the pierce regressions).
- `tsc --noEmit` clean (CamPocketPanel hint + en.json key).
- User verified in-app 2026-09-08 ("OK is working now. make a
  commit") → COMMITTED fe6dbc9 (32 files, +3703/−109).

## Next steps

1. Next milestone per wiki/CAM-Development.md §V1: 2D contour.

---

# Previous task: CAM milling UX + 5-axis scaffolding (cam/milling) — COMMITTED e9d4a64

> **Branch:** `cam/milling` (from `dev` @ c10ceba)
> **Date:** 2026-09-05
> **Plan:** approved plan at `.claude/plans/glittery-coalescing-kernighan.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval, no Co-Authored-By trailer.)

## Context

User feedback on the CAM milling workspace (their words, binding): "I
still do not have -z value. I still do not have how many passes. I do
not have 'snap' on the stoc points. I cannot select face of the stoc
faces. I am having here a very bad implementation". User decisions:
passes run from the STOCK TOP down to the face (multi-pass only when a
Stepdown is set); WCS Z=0 = top of stock (pick the stock top face →
cuts go negative).

## Status — COMMITTED as `e9d4a64` (50 files, +2463/−180)

- **M0 milling foundations** (from the earlier milestones, same commit):
  MachineDefinition mill fields (travel/kinematics/axis limits/
  tool-change position) + 3 mill seeds; ToolpathMove rotary a/b/c +
  modal rotary words + G93 inverse-time feed; per-op tool_axis_mode
  (fixed_z only); linuxcnc post with golden test.
- **Multi-pass face milling**: stepdown plans levels from the stock top
  down to the face (last level pinned at the face); cleared stepdown =
  single pass; 100-level cap + retract-below-stock-top warning.
- **WCS anchor model**: `""`/`face`/`stock_face`/`point`/`stock_origin`;
  `"point"` pins manual X/Y/Z edits against the refresh clobber;
  `cam_wcs_set_face` accepts `"stock:<face>"` ids (no new IPC).
- **cam_stock helper** mirroring the UI stock-box extents (core/UI
  parity contract, documented in wiki/CAM-Development.md).
- **Stock-aware picking**: stock box face-tagged (`userData.stockFaceNames`)
  and pickable; WCS pick snaps to stock corners/midpoints (top+bottom),
  anchors to stock faces, falls back to the bed plane; snap markers
  shared with the origin pick.
- **Stale-closure fix** (the stock-snap root cause): the viewport
  pointer handlers live in an effect keyed on activeSketchPlaneId only,
  so their closures froze mount-time document/viewport/showStock/setup —
  body snap worked (scene refs), stock candidates were silently never
  built. Fixed with render-synced `documentRef`/`viewportRef`/
  `showStockRef`/`activeCamSetupIdRef` at the three pick call sites.
- **Deviation (deliberate)**: stock bottom-FACE CENTER snap candidate
  from the plan omitted (would need a new snap kind + i18n label; top
  face has no center either — corners + edge midpoints cover both).

## Verification

- `pnpm test:core` all 39 suites green — cam_generators_test (1-50:
  multi-pass levels, level cap, retract-below-stock-top), cam_refresh_test
  (1-10: point anchor survives refresh, stock_face resolves/degrades,
  payload round-trip), cam_commands_test (stock_face round-trip),
  cam_machine_library_test, linuxcnc_post_test, cam_save_load_test.
- `tsc --noEmit` clean.
- User-verified in-app: face milling multi-pass looks good; stock snap
  square + label + stock face anchor work after the closure fix; manual
  WCS edits survive refresh. "OK it is working now. commit and continue"
  → commit approved.

## Next steps

1. ~~Push `cam/milling` and open the PR against `dev`~~ — SUPERSEDED by
   the user's "No PR. continue with implementation."; work continues on
   `cam/milling` (see the 2D Pocket task above).
2. Deferred from the plan: stock bottom-face-center snap kind (if wanted).

---

# Previous task: Core build speedup + OCCT deprecation migration (core-build-speedup)

> **Branch:** `core-build-speedup` (from `dev` @ 4d03d7b)
> **Date:** 2026-09-04
> **Plan:** approved plan at `.claude/plans/streamed-hatching-crab.md`
> (every commit gated on build/tests + user verification — CLAUDE.md:
> no untested commits, no git mutations without explicit approval.)

## Context

Task #6: "Speed up full core builds: parallel jobs + stop per-test
recompilation". Two structural problems: all 35 full-core test exes
recompiled the entire CAD_CORE_SOURCES (~57 TUs) each (~2,100 TU
compiles per clean build), and `cmake --build --parallel 2` with
cmake-injected `CL_MPCount=1` meant serial in-project compilation.
`pnpm test:core` also ran the 38 suites strictly sequentially.

## Status — implemented, verified, UNCOMMITTED

- **CMakeLists.txt** — `cad_core_lib` STATIC target (CAD_CORE_SOURCES
  compiled once); cad_core exe = app.cpp + main.cpp + lib; 35 full-core
  suites collapse to `polysmith_add_core_test(name tests/x_test.cpp)`
  (1 TU + project reference each); curated suites (cam2d,
  sketch_profile, text_engine) untouched. `CMAKE_VS_GLOBALS`
  UseMultiToolTask + EnforceProcessCountAcrossBuilds for bounded
  per-TU parallelism (NOT /MP — inert under cmake --build).
- **scripts/build-core.mjs** (new) — `pnpm core:build` wrapper, jobs =
  `CAD_CORE_JOBS` || os.cpus().length, no `--target`.
- **scripts/run-core-tests.mjs** — async pool (`CAD_CORE_TEST_JOBS`,
  default CPU count), per-suite private TMP/TEMP/TMPDIR (fixed-name
  temp subdirs + remove_all would race otherwise), buffered output in
  completion order.
- **Docs** — wiki/Implementation-Log.md entry.

## Verification (all green on this machine, 12 cores)

- clean `pnpm core:rebuild`: 1m25s (was multi-minute at --parallel 2)
- `pnpm test:core`: 38/38 in ~1.7s wall parallel; serial ×2 and
  parallel ×2 all green; no temp leftovers
- incremental: touch protocol `.inc` → owning TU + relinks in ~4s
  (tlog tracking unchanged)

## Part 2: OCCT 8.0 deprecation migration — implemented, verified, UNCOMMITTED

Task #5 (user-added, same branch): replace deprecated OCCT 8.0 array
containers with NCollection types; fix only warnings from OUR code,
leave OCCT-library and planegcs ones. Clean-rebuild triage found 16
deprecation warnings in exactly 5 files:

- **feature_shape.cpp + geometry/impl/sketch_wire_extrude.inc** —
  `TColgp_Array1OfPnt` / `TColStd_Array1OfReal` / `TColStd_Array1OfInteger`
  → `NCollection_Array1<gp_Pnt|double|int>` (BSpline wire edges)
- **sketch/spline_profile_occt.cpp** — same trio (Pnt2d) →
  `NCollection_Array1<gp_Pnt2d|double|int>`
- **export/export.cpp + tests/stl_writer_test.cpp** —
  `TopTools_IndexedDataMapOfShapeListOfShape` → `NCollection_IndexedDataMap
  <TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>`
  (`TopExp::MapShapesAndAncestors` signature verified against OCCT 8.0)
- **tests/cam2d_test.cpp** — spline fixture trio → NCollection_Array1

- **CMakeLists.txt** — removed `include_directories(occt8-install/inc)`
  + its stale comment: it existed only for the deprecated TCol headers
  (install-tree-only), which nothing includes anymore
  (`grep TColStd_|TColgp_|...` → 0 matches in our code).

Verification: incremental rebuild — 0 warnings, `pnpm test:core` 38/38
parallel + serial; full rebuild after the include-dir removal — green,
0 warnings from our code (remaining: dxfrw C4805 + planegcs
`BOOST_ALLOW_DEPRECATED_HEADERS` C4005, both third-party), 38/38 again.

## Next steps

- User smoke: `pnpm dev` (POST_BUILD cad_core.exe copy confirmed) —
  remaining commit gate
- Commits (each needs explicit user approval, no Co-Authored-By):
  1. build: static cad_core_lib + MTT parallel compile
     (CMakeLists, build-core.mjs, package.json, Implementation-Log)
  2. test: parallel suite runner (run-core-tests.mjs, this tracker)
  3. fix: OCCT 8 deprecations → NCollection containers
     (5 sources + CMakeLists include-dir cleanup, Implementation-Log)
- POSIX cross-check untested here (changes are generator-agnostic;
  CMAKE_VS_GLOBALS ignored by Makefiles/Ninja)

## Previous task: CAM workspace interaction pass — MERGED PR #74 (4d03d7b)

> **Branch:** `cam/laser-cut` (from `dev`)
> **Date:** 2026-09-02/04
> **Plan:** approved plan at `.claude/plans/streamed-hatching-crab.md`
> (every commit gated on build/tests + user in-app verification —
> CLAUDE.md: no untested commits, no git mutations without explicit
> approval.)

## Context

The user asked to revisit the laser CAM implementation and rejected a
polish-focused plan: the real gaps were CAM-workspace interaction —
snap to geometries, highlighting, select & deselect, stock/body snap
points for the origin pick, and saveable/reusable machine definitions.
Git archaeology confirmed regressions from `b48b47d` (exact 3D origin
pick → bed-plane click, z forced 0) and two multi-setup bugs (TS origin
pick + core `cam_wcs_set_face` both write `setups[0]`; the WCS triaxis
marker follows `stock.origin` instead of the resolved
`wcs_origin.position`).

## Status

- **M1-core — COMMITTED** `3740663`: `cam_setup_find` resolution +
  optional `setup_id` on `cam_wcs_set_face` (absent = first setup,
  backward compatible) + tests + wiki.
- **M1-ui — COMMITTED** `59bbee3`: origin snap restoration
  (`camOriginSnap.ts` candidates + `resolveCamOriginSnap`, z-rule: snapped
  geometry keeps 3D z, bed-plane fallback stays z=0), SnapCursorOverlay
  label, active-setup fixes for origin pick + `placeWcsFromFacePick`,
  WCS marker positioned at `wcs_origin.position` (pointer offset +
  face-anchored WCS now move it), i18n snap labels.
- **M2 — COMMITTED** `f2a5e77`: re-pick polish (TS only).
  `camProfileSelection.ts` (new): `reportCamProfileSelectionChange`
  runs the selection command then awaits the fresh document —
  `profilePicked` (increased) / `profileRemoved` (decreased), never
  client-side prediction; `laserOperationScopeSketchId`. App.tsx
  overcount sites replaced; "Clear selection" button in
  CamLaserCutPanel when armed; Apply/Cancel dispatch `clear_selection`
  (no stale-selection leakage into later ops); hidden scope sketch
  auto-shows on arm, restored on Apply/Cancel. tsc green.
- **M3-core — COMMITTED** `ccd7f98`: machine library.
  `machine_library.{h,cpp}` (new) mirroring post_processor.cpp: JSON
  file per machine in `POLYSMITH_MACHINES_DIR` (set by cad_core.rs to
  app_data/machines), 3 built-in seeds (grbl-laser, smoothieware-laser,
  generic-3-axis-mill) written if absent, user files override built-ins
  by name, re-read on every list, save = slugged-file write with
  validation. IPC: `cam_machine_list` / `cam_machine_save` →
  `cam_machine_list_result` event; schemas; wiki docs. New suite
  `cam_machine_library_test` (5 cases: seed idempotence + user
  override, save/load round-trip, invalid rejection, dir env override,
  builtin seeds parse). Registered in CMakeLists.txt.
- **M3-ui — COMMITTED** `f2a5e77`: MachineDefinition types (TS,
  cam.ts + camCommands.ts + ipc.ts), zod schema
  (`camMachineListResultEventSchema` in coreMessageSchema — avoid the
  attestation-event class of bug), factories, useCadCore
  camMachineList/camMachineSave, App once-per-CAM-entry fetch
  (armed-ref, NOT keyed on the fresh function identity), CamSetupPanel
  Machine tab (dropdown + inline name input + Save, preselect by
  content match), applyMachine composes camSetupUpdate +
  camPostProcessorSet + camMachineSettingsSet in one runAction in
  CamFloatingPanels (single setup-resolution source). i18n keys.
  tsc green.
- **Laser lead side — COMMITTED** `1fe6d2a` (core) + `3ba2453` (UI):
  the pierce + leads now follow the kerf offset side.  A straight
  tangent line cannot lie inside a closed contour, so interior offsets
  (kerf_side "inside", or auto on holes) turn the lead into a spoke
  along the pierce→centroid ray (mirrored roll for arc style); the
  pierce dwell stays at the lead entry, on the kerf side.  Also
  shipped: `pierce_angle_deg` (centroid ray-cast, overrides
  `pierce_position`), `arc_segments_per_circle` (viewport polyline +
  use_arcs=false post), pierce viewport flag, panel controls
  (lead-side auto/angle, arc segments, debounced auto-preview,
  lead-follows-kerf hint).  38 suites green (cam_generators 45 cases
  incl. hole-lead-from-interior + kerf-inside-lead-inside regressions;
  grbl_post byte-identical at arc_segments 0); user verified in-app.
- **Origin snap on finished sketches — COMMITTED** `e713110`: the core
  only emits vertex sprites for the ACTIVE sketch, so finished
  sketches contributed no origin-pick snap targets.  Candidates now
  derive from the always-emitted sketch primitives (line endpoints,
  circle/arc centers), shared by the snap resolver and the armed-pick
  markers; scene hover suppressed while armed; snap square + kind
  label track the pointer.  tsc green; user verified in-app.

## Next steps

1. Push `cam/laser-cut` and open the PR against `dev` (user-requested).

---

# Active Task: OrcaSlicer per-body "Send to Slicer" — COMMITTED 38119f3

> **Branch:** `feature/Orca`
> **Date:** 2026-09-01
> **Plan:** approved plan at `.claude/plans/nifty-enchanting-fern.md`

## Status — COMMITTED as `38119f3` (36 files, +587/−78)

User runtime-verified in-app ("it is working") and approved the commit.
All verification builds green: `pnpm test:core` (37/37 suites),
`cargo check`, `tsc --noEmit`.

## What shipped

1. **External-launch mode** (prior session, runtime-confirmed by the user):
   OrcaSlicer runs as its own window — export the model to a temp file and
   spawn OrcaSlicer detached with the file as argv (`orca_slicer.rs`
   `prepare_orca_export_path` + `spawn_orca`; TS side in
   `slicerExport.ts` / `slicerWorkspaceActions.ts`).
2. **Header "Export to Slicer" button removed entirely** (AppHeader +
   AppTopBar + App.tsx dead wiring) — replaced by right-click context
   menus, per user request.
3. **"Send to Slicer ▸" submenu** on body right-click in BOTH the document
   tree (`DocumentHierarchyPanel.tsx`) and the viewport
   (`ViewportContextMenu.tsx` + actions/refs/shell plumbing):
   - **As STL (mesh)** — existing `export_body_stl` path.
   - **As STEP (B-rep, more accurate)** — new `export_body_step` IPC →
     new `export_body_as_step` in `core/export/export.cpp` (single
     `STEPControl_Writer` session, one `Transfer(STEPControl_AsIs)`).
   - STEP hidden for mesh-import bodies (no B-rep — core rejects);
     web integration + STEP shows a status message (web upload is
     mesh-only).
4. **"Export as Mesh" untouched** — still saves STL to disk via file
   dialog.
5. **Regression tests**: `stl_writer_test` grew
   `test_body_export_step` (valid ISO-10303-21 file, count 1) +
   `test_body_export_step_unknown_body` (runtime_error, no file).
6. **Docs**: `wiki/IPC-Protocol.md` + `wiki/AI-CAD-Command-Language.md`
   document `export_body_step`.

## Follow-up — mesh-import bodies now show BOTH formats (UNCOMMITTED, 2026-09-01)

User checklist item 6: a mesh-import body showed only "As STL" (STEP
hidden) and the user wants both items listed. Investigation: the gate
was unnecessary — mesh bodies DO compile into `body_shapes`
(`compile_bodies_modifier_replay.inc`: the `include_meshes=false` flag
only skips viewport tessellation), and `export_body_as_step` writes
them as valid faceted/tessellated STEP (the "core rejects" note in
"What shipped" item 3 was wrong). STEP-import bodies are kind
`step_import` and were never gated — matching the user's "I have
imported step files and works".

Fix (UI-only, 4 files): removed the STEP gate in
`DocumentHierarchyPanel.tsx` + `ViewportContextMenu.tsx` and deleted
the dead `isMeshImportBody` plumbing (`viewportContextMenuActions.ts`,
`ViewportPanelShell.tsx`). "Convert to Body" mesh item untouched.

Regression test: `stl_import_test` test 20 `test_mesh_body_export_step`
— import STL box → `export_body_as_step` → format "step", count 1,
file starts with `ISO-10303-21`. All 37 suites + tsc green.

Awaiting user in-app verification + commit approval.

## Next steps

1. Push `feature/Orca` and open a draft PR against `dev` (needs user
   approval — not done yet).

---

# Active Task: Laser CAM rework (cam/laser) — COMMITTED 82c2ffa, ironing out

> **Branch:** `cam/laser` (from `dev`, after `fac4fb5` CAM scaffolding)
> **Date:** 2026-08-29
> **Plan:** approved plan at `.claude/plans/lucky-finding-hoare.md` (10
> milestones; each gated on `pnpm test:core` + `tsc` + user in-app
> verification).

## Status — COMMITTED as one squashed feature commit

`82c2ffa` on `cam/laser`: the full rework (M1–M10 + test patterns + machine
settings + U1–U6 IPC finalization + the in-app-testing UI fixes), 81 files,
+8387/−1935.  Full MSVC build + all 37 suites + tsc green.  The user asked
for a single commit despite remaining rough edges — ironing out continues
on top.

- **M1 — offset correctness + graceful degradation**: right-handed xz
  frame, world-mapped arc i/j + mirror sweep flip, non-horizontal plane
  rejection (~5°, `kMaxCutPlaneTilt`), per-loop degrade (outer failure
  drops the region, hole failure drops the hole, all-fail hard error
  surfaces the first reason), cached loop samples. Tests 9–12
  (rotated/mirrored arcs, yz/xz rejection, hairline slot) verified
  fail-before/pass-after.
- **M2 — shared 2D CAM module**: `cam2d.{h,cpp}` (offset/join/clip/
  containment math, OCCT-free) + `cam_planning.{h,cpp}`
  (chord-tolerance wire sampling, `face_cut_plane`, `map_face_index`);
  the `.inc` preprocessor era ended for the generators; new
  `cad_core_cam2d_test` (10 cases).
- **M3 — parameter model v2 + monolith split**: `LaserCutParameters` v2
  (speed_mm_per_s, kerf_side, lead styles/angles, overcut, pierce
  position, tabs, fill, cut_order, air assist — serde
  backward-compatible, legacy docs load with defaults);
  `laser/laser_generate.{h,cpp}` module replaces the 630-line `.inc`;
  speed/passes/kerf_side/mode validation/conventional direction/
  thickness warning live. Tests 13–17 + save/load v2 round-trip +
  legacy-defaults test.
- **M4 — nesting + cut ordering**: containment tree per domain,
  `laser/laser_order.cpp` (inner_first / nearest_neighbor / by_area),
  duplicate-cut warning via cross-group coincident scan. Fixed the
  synthesized-full-circle area = 0 bug (area now from the sampled
  offset loop). Tests 18–20.
- **M5 — leads + pierce**: `laser/laser_leads.cpp` (corner-filtered
  pierce vertices, line/arc lead styles, lead angles, overcut,
  pierce_position). Tests 21–25. NOTE: lead angle default is 0°
  (tangent continuation) — the plan's 90° default would have changed
  the existing behavior; LightBurn's 0°=tangent convention kept.
- **M6 — tabs/bridges**: `laser/laser_tabs.cpp` (arc-length tab
  distribution, segment splitting, laser-off or tab_power spans,
  outer-loops-only default). Tests 26–28.
- **M7 — engrave fill**: `laser/laser_fill.cpp` (scan-line hatch,
  holes via even-odd, angle, bidirectional, per-line laser-off jumps
  across holes). Tests 29–31.
- **M8 — posts + export (GRBL-first)**: power_change template (B3),
  laser_footer_lines (no Z lift for laser programs), laser_air_on/off,
  smoothieware seed, inch scaling ×1/25.4 (B2), single footer on the
  last exported op (B1), unknown post = real failure. grbl_post tests
  8–13 + end-to-end export test 32.
- **M9 — UI rework**: CamLaserCutPanel v2 (all v2 fields; zod schema =
  single defaults source), merged duplicate generate handlers + i18n
  stats, last-element op id, batched LineSegments toolpath rendering,
  i18n for all hardcoded messages, dead en.json keys removed, WIP
  origin-pick absorbed (camSignature now covers stock.diameter/length;
  close clears the pick). tsc green.
- **M10 — cleanup**: shared `resolve_geometry_reference` (refresh +
  generate one source of truth), laser tool-type validation,
  WcsOrigin.position from stock origin (+ refresh test 5),
  cam_runtime clear() wired into create_document, dead constants +
  both `#if 0` blocks deleted, default laser tool uses ToolEntry
  defaults. Test 33.

## Next steps

1. **Continue in-app ironing out** (user-reported): rotated-sketch
   preview arcs; passes=3; speed mm/s → F600; kerf side inside flips
   the offset; arc lead visible; tabs as gaps; fill hatch renders;
   export has no Z moves + exactly one M2; origin pick on a cylinder
   setup rebuilds the marker; Test Pattern card generates/engraves/
   cuts; Machine Settings fields persist and shift the WCS by the
   pointer offset.
2. Deferred (tracked in the plan): Ruida/DSP export, Clipper2
   adoption, per-shape cut-order priority, material presets, setup-list
   UI management (core multi-setup resolution is done).

---

## Added on top (2026-08-29, in commit 82c2ffa): test patterns + machine settings + IPC sweep

- **`laser_test_pattern` operation** (LightBurn material-test cards):
  `engrave_grid` (filled squares) and `cut_grid` (through-cut squares),
  power columns × speed rows, machine-coordinate cells, work-area
  overflow warning, laser-tool check. Generator tests 34–36.
- **`LaserMachineSettings`** on `document.cam.machine_settings`: bed
  work area + red-pointer offset; the refresh pass shifts the laser WCS
  by −offset (framing under the dot); `cam_machine_settings_set` IPC
  command + schema + TS wiring; Machine fieldset in the setup panel;
  refresh test 6. Save/load round-trip + legacy defaults covered.
- **IPC finalization sweep (U1–U6, all test-gated)**:
  U1 deleted the serialized-but-dead fields (`fallback_strategy`,
  `PostProcessorOptions`, `SimulationData`, `point_locations`/
  `CamPointLocation`) end-to-end (core, serde, TS, schemas, tests).
  U2 `cam_capture_face_reference` → the UI never fabricates face
  witnesses (TS `buildFaceAttestationFromSelection` deleted).
  U3 viewport toolpath points carry `pierce` → pierce-dot markers.
  U4 multi-setup: `CamOperation.setup_id`, `setup_for()` resolution in
  generate/export/refresh, setup panel edits the selected op's setup.
  U5 face-anchored WCS: `cam_wcs_set_face` + refresh resolves the
  machine origin from the live face (mid-UV, TNP-safe).
  U6 test patterns: `kerf_gauge` calibration square (test 37) +
  engraved "P… S…" cell labels via the core text engine (test 38).
- **In-app-testing fixes (post-commit-ready, folded into 82c2ffa)**:
  `cam.operations` i18n key collision → `cam.operationsTitle`;
  `step="any"` on stock-origin, WCS-origin, and laser speed fields
  (native validation bubbles); speed fallback display rounded to 1dp.

---

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

## Trim tool modernization — COMPLETE (2026-08-27/28)

User-reported: the 2026-05 trim tool (line/circle/arc only, patched
three times) got confused on complex geometry, its vertex minting
fought the wire walk, and the UI raced the core. Modernized in five
stages, five commits on `feature/sketch`:

- `d6124bf` — no silent deletes + deterministic point identity
- `fc4e608` — shared exact-curve layer (trim and the walk can no
  longer disagree; ellipse/spline cutting edges)
- `443e2a1` — race elimination (revision-stamped previews, stale
  index fallback, IPC correlation, preview coalescing), flower
  notch-junction fix, FIX-badge cleanup
- `127862f` — ellipse trim targets (partial elliptical arcs) +
  crossing lines split closed profiles (ellipse-chord surfaces)
- `0673378` — spline trim targets (exact knot-insertion split)

Suites total 30 (trim_test 23 cases, ellipse trim-to-arc, spline
trim-split); all green; tsc clean; every stage user-verified in-app.
Dead TS preview math removed from trimHoverPreview.ts;
wiki/Trim-Tool-Implementation-Plan.md rewritten; Implementation-Log
updated.

Deferred (tracked): constraint/dimension re-targeting onto surviving
trim segments; polygon-record preservation across trims; the three
duplicated orphan-coincident sweeps (consolidate only when a future
stage touches them); spline×ellipse intersections (still routed
through OCCT's generic path — fine, but untuned).
