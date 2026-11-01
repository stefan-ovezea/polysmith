# Remove quadrant vertices — design and plan

**Branch:** `feature/remove-quadrants` (from `dev` @ 24e2a7a)
**Status:** implemented 2026-11-01 — P0–P7 landed on the branch. Gates: `pnpm
test:core` 89/89, `tsc --noEmit` clean, `pnpm lint:architecture` 6/6. In-app
verification of the trim hover and the snap matrix is still outstanding (the app
must be restarted against the new core).
**Decisions taken (2026-11-01):** D1 remove the quadrant snap entirely; D2 **no
migration and no format-version bump** — there are no documents in the wild that
need to stay compatible, so old files are simply read without their quadrant
records (the vertex parser skips the kind; the curve rebuilds what it needs); D3
drop the persisted `snap_quadrant` flag (an old file's key is ignored, not
migrated).
**Owner:** Stefan. Written 2026-11-01.

---

## 0. Decision

Remove the derived **quadrant vertices** from the sketch model entirely — the four
cardinal points emitted on every circle, and the cardinal points inside an arc's
sweep. They are a second representation of information the curve already owns, they
leak a legacy id format into trim bookkeeping, and they steal picking priority from
the curve itself.

Not in scope: the "quadrant" concept in Gerber parsing (`multi_quadrant_arcs`,
G74/G75) and in angle-dimension orientation — unrelated uses of the word.

---

## 1. What exists today

| Where | What |
|---|---|
| `core/sketch/impl/private_point_profile_helpers.inc:340-428` | Emits 4 vertices per circle (`vertex-<circle>-quadrant-0..3`) and per cardinal angle inside each arc's sweep (`vertex-<arc>-quadrant-N`). `kind = "quadrant"`, `is_fixed = false`, `geometry_owner_ids = [entity]`. |
| same file `:86-94` | `find_coincident_endpoint` **already excludes** quadrants, so a new endpoint no longer adopts the id — the comment records the reason: a shared-id attachment is not a constraint, and `vertex_is_derived` then refuses to bind it, leaving the endpoint behind when the curve moves. |
| same file `:192-196` | **`resolve_shared_point` does NOT exclude them** — it walks `parameters.vertices` and will return a quadrant id as a trim-minted arc endpoint. This is the trim hazard in §3. |
| `core/viewport/impl/snap_candidate_emit.inc:75-125` | Emits `kind="quadrant"` snap candidates with `vertex_id = ""` (circle: 4; arc: cardinals within sweep). Decoupled from the vertices already. |
| `core/sketch/impl/trim_attachment_cleanup.inc:144-215` | `live_sketch_point_positions()` regenerates the four circle quadrant ids by string construction so a constraint referencing one is not pruned as orphaned; `:153` carries `TODO(phase-6-followup)`. |
| `core/sketch/impl/trim_circle_entity_command.inc:43-47` | Comment explains the circle being converted is excluded from the shared-point scan so it cannot resolve against its own centre or quadrant vertices. |
| `core/sketch/sketch_constraint_types.h:67` | `bool snap_quadrant = true` in `SketchSelectionFilter`. |
| `protocol/impl/document_session_to_payload.inc:143`, `document_from_payload.inc:219`, `viewport_to_payload_state_scene_interaction_entries.inc:33`, `app/sketch_utility_command_handlers.cpp:85-87` | The filter flag round-trips through document and viewport payloads and is settable by command. |
| UI types/schemas | `types/geometry/sketch.ts:68`, `types/scene.ts:303`, `types/viewport.ts:263` (`kind` unions), `viewportStateSchema.ts:412,524,804`, `documentStateSchema.ts:827`, `selectionFilter.ts:13,40`, `selectionFilterState.ts:17,59`, `SelectionFilterPanel.tsx:42`, `i18n/en.json:184`. |
| UI snapping | `snapResolution.ts:23,335,558,1236` (`SketchSnapKind`, `isPriorityStaticSnap`, candidate mapping, `snap_quadrant` filter mapping). |
| UI picking | `pointerUpActiveSketch.ts:151,503`, `sketchClickSelection.ts:7,30,519`, `sketchConstraintPointPick.ts:3`, `contextMenuState.ts`, `constraintPreview.ts`, `dimensionRelationPreviewGeometry.ts`. |
| UI badge | `types/geometry/contraints.ts:33-35` — a `"quadrant"` armed-constraint kind rendered as a "Q" badge. No producer found in the pick flows; treat as dead. |
| Trim picking | `sceneTargetPicking.ts:355-362` — the trim tool must special-case entity-first picking because the zoom-aware point radius "swallows a circle's outline around its center and **quadrant vertices**, making the red trim highlight unreachable". |
| Tests | `committed_attach_test.cpp` (23 refs), `extrude_quality_test.cpp` (11 — incl. `test_trim_vertex_owners_and_center_sharing` at ~1370 asserting a quadrant id "appears exactly once"), `sketch_angle_dimension_test.cpp` (4), `sketch_profile_test.cpp` (2), `circle_modes_test.cpp` (2), `snapMatrix.test.ts` (11), `viewportSceneRectangleDims.test.ts` (1). |

**Saved-document survey** (2026-11-01, all four `projects/*.json`): 37, 51, 53 and 72
quadrant vertices respectively — and **zero references to any of those ids** outside
the vertex record itself. No line endpoint, constraint or dimension points at one.
That is the empirical basis for saying removal loses no user data.

---

## 2. Why they go

1. **Derived duplication.** A quadrant point is `(cx + r·cos θ, cy + r·sin θ)` for
   θ ∈ {0, ½π, π, 1½π}. It carries no information the circle/arc does not, and it is
   regenerated on every rebuild — so it is also persisted state that must be migrated
   forever (see `Data-Safety-Design.md` §A.4: derived caches should not be persisted).
2. **It is not an attachment mechanism, but it looks like one.** Endpoint adoption of
   a quadrant id was removed in `find_coincident_endpoint`, precisely because the
   "shared id" tie is invisible to the solver and `vertex_is_derived` then refuses to
   bind — the bug that froze sketches with `point_on_circle vertex-circle-1-quadrant-0
   /circle-1 off by 38.97 mm`.
3. **They break picking.** The trim tool needs a bespoke entity-first branch to reach
   a circle outline through its own vertex markers (`sceneTargetPicking.ts:355`).
4. **They force id archaeology.** `trim_attachment_cleanup.inc` reconstructs the id
   format by hand; any refactor of vertex ids has to keep that string format alive.

---

## 3. The trim coupling — what actually breaks, and why it stops breaking

This is the part worth being precise about, because "removing quadrants breaks trim"
is true only of two mechanisms, both of which exist *because* quadrants exist:

**(a) `resolve_shared_point` can hand a quadrant id to a trim-minted arc endpoint**
(`private_point_profile_helpers.inc:192-196`). When a circle is trimmed into arcs, the
new arc's `start_vertex_id`/`end_vertex_id` are resolved from the shared-point scan,
which includes the vertices table — quadrant records included. The arc then carries
`vertex-arc-1-quadrant-0` as an endpoint id, which is nonsense (a vertex owned by the
circle, reused as an arc endpoint), and `extrude_quality_test.cpp:1406` pins exactly
this string.

After removal there is no such vertex in the table, so the scan can only return a real
entity point or nothing (`mint_vertex_id()`). The hazard disappears rather than being
worked around. The test that encodes it gets rewritten to assert the endpoint is a
minted `vertex-N` bound by `point_on_arc`.

**(b) The live-point map** (`trim_attachment_cleanup.inc:155-215`) lists the four
quadrant ids per circle so that a coincident constraint referencing one is not treated
as orphaned and dropped. With no quadrant vertices, no constraint can reference those
ids, so the four entries are dead weight. Deleting them is the *fix*, not a regression
— but the deletion must be paired with the constraint-transfer tests below, because
that map is also what proves "a severed endpoint pair keeps its ids alive".

**Not affected:** the arc branch of the same map (`:174-178`) only lists start, end and
centre — arcs never had quadrant entries. `trim_circle_entity_command.inc` excludes the
source circle by entity id, so it needs no change beyond the comment at `:43-47`.

**Also not affected:** profile detection already skips them
(`private_point_profile_helpers.inc:94`, `:82-92`), so region sets are unchanged.

---

## 4. Scope decisions — need a call

**D1 — Does the "Quadrant" snap survive?**
*(recommended: remove it)*
Quadrant snap candidates are emitted with `vertex_id = ""`, which is why
`Sketch-Snap-Matrix.md` measured them as un-bindable ("Centre and quadrant snaps carry
no host id at all"). Removing the vertices but keeping the candidate would leave a
snap that lands on a point nothing owns — the same T-junction failure the matrix
records. Recommendation: delete the candidate type too; `On circle`/`On arc` and
`Nearest` already give a bindable point on the curve. Cost: users lose the 4-way
cardinal magnet on circles. Alternative: keep it as a *snap-only* candidate mapped to
`point_on_circle` at commit (work, and duplicates `On circle`).

**D2 — Old documents.**
*(decided 2026-11-01: nothing to keep compatible — no migration)*
There are no documents in the wild that must keep loading, so the format version
stays at 1 and no migration step is written. The vertex parser simply skips
records whose `kind == "quadrant"` (`feature_parameter_sketch_geometry_parser.inc`),
which is what keeps a stray old file from failing the UI's schema while adding no
versioning machinery. Nothing references those ids in the four sample projects, and
the point is always re-derivable from its curve.

**D3 — Keep the persisted `snap_quadrant` flag readable for one release?**
*(recommended: yes, parse-and-ignore)*
`document_from_payload.inc:219` already tolerates its absence. Keeping the read path
for one version costs three lines and makes a downgrade of the app non-destructive.

---

## 5. Work plan

Order matters: **core emission first**, then schemas — a reply carrying `kind:
"quadrant"` while the zod enum has dropped it is rejected wholesale and surfaces as a
15 s command timeout, not an error (`Sketch-Next-Session.md` §5).

### P0 — guard (0.5 d)
- [ ] `scripts/architecture-greps.mjs`: new check that fails when `native/cad-core/src`
      emits `kind == "quadrant"` or constructs an id matching `-quadrant-\d`.
- [ ] Same check asserts no `snap_quadrant` in the C++ filter struct once P2 lands
      (so a revert cannot quietly reintroduce half of it).

### P1 — core: stop emitting (0.5 d)
- [ ] Delete `emit_quadrant` and both loops (`private_point_profile_helpers.inc:340-428`).
- [ ] Delete the four circle entries in `live_sketch_point_positions` and the
      `TODO(phase-6-followup)` (`trim_attachment_cleanup.inc:153,164-173`).
- [ ] Update the stale comments at `:82-94`, `:498-500`, `trim_circle_entity_command.inc:43-47`,
      `attach_committed_vertex.inc:41-46`, `sketch_feature_lifecycle_declarations.inc:21`,
      `sketch_merge_coincident_points_commands.inc:41`, `sketch_delete_selection_commands.inc:473`,
      `sketch_line_vertex_update_commands.inc:124`, `state_and_create.inc:636`.
- [ ] `resolve_shared_point` (`:192-196`): keep walking vertices (still needed for
      projected/fillet/slot vertices) — the quadrant exclusion becomes unnecessary but
      add a comment saying why the table is safe now.

### P2 — core: snap candidates + filter (1 d)
- [ ] Remove the circle and arc quadrant candidates (`snap_candidate_emit.inc:75-126`,
      both loops are inside `if (filter.snap_quadrant)`).
- [ ] Remove `snap_quadrant` from `SketchSelectionFilter` (`sketch_constraint_types.h:67`).
- [ ] Stop writing it in `document_session_to_payload.inc:143` and
      `viewport_to_payload_state_scene_interaction_entries.inc:33`; keep the tolerant
      read in `document_from_payload.inc:219` (D3) and delete the setter branch in
      `sketch_utility_command_handlers.cpp:85-87`.
- [ ] `pnpm registry:write` if any command signature changes (it should not).

### P3 — protocol + TypeScript (1.5 d)
- [ ] Remove `"quadrant"` from the zod enums: `viewportStateSchema.ts:412,524,804`,
      `documentStateSchema.ts:827`; `snap_quadrant` from the selection-filter schema.
- [ ] Remove from TS unions: `types/geometry/sketch.ts:68`, `types/scene.ts:303`,
      `types/viewport.ts:263`; `selectionFilter.ts:13,40`; the dead badge kind in
      `types/geometry/contraints.ts:33-35`.
- [ ] Remove the filter row (`SelectionFilterPanel.tsx:42`, `selectionFilterState.ts:17,59`)
      and `i18n/en.json:184`. (`help-index.ts:68` mentions "quadrant" only in the
      angle-sign sense — leave it.)
- [ ] Snapping: drop `"quadrant"` from `SketchSnapKind` (`snapResolution.ts:23`), from
      `isPriorityStaticSnap` (`:335`), the `case "quadrant"` arm (`:558`) and the
      `snap_quadrant` mapping (`:1236`).
- [ ] Picking: drop `"quadrant"` from `pointerUpActiveSketch.ts:151,503`,
      `sketchClickSelection.ts:7,30,519`, `sketchConstraintPointPick.ts:3`,
      `contextMenuState.ts`, `constraintPreview.ts`, `dimensionRelationPreviewGeometry.ts`,
      `viewportCallbackRefs.ts`, `viewportPanelTypes.ts`, `viewportContextMenuActions.ts`,
      `ViewportOverlays.tsx`, `sketchSceneObjects.ts`, `endpointDrag.ts`,
      `sceneTargetPicking.ts`.
- [ ] Delete the trim-specific entity-first branch if it is no longer needed
      (`sceneTargetPicking.ts:355-362`) — measure first: with no vertex markers on the
      outline, the ordinary order may be enough; if it is, removing the branch is a net
      simplification, if not, leave it with an updated comment.

### P4 — trim verification (1.5 d)
- [ ] `resolve_shared_point` no longer returns quadrant ids: add a C++ test that trims
      a circle into arcs and asserts every new endpoint id matches `vertex-\d+` (not
      `vertex-circle-*`), and that the arc is bound by `point_on_arc`.
- [ ] Constraint survival: a coincident constraint between two line endpoints that a
      trim severs keeps both ids; with quadrants gone, a constraint that *used* to point
      at a quadrant id is dropped with a warning (covered by the migration test).
- [ ] Update `test_trim_vertex_owners_and_center_sharing`
      (`extrude_quality_test.cpp:1370+`): remove the "quadrant id appears exactly once"
      leg, keep the centre-sharing leg, assert the endpoint is a minted vertex.
- [ ] Regression-run the trim suites: corner trim, split, drag-paint stroke, circle→arc
      conversion, arc-arc.
- [ ] In-app: hover a circle with the trim tool and confirm the red highlight is
      reachable all the way round the outline (currently needs the entity-first branch).

### P5 — old files (dropped)
- [x] No migration, no format bump: `format_version` stays 1.
- [x] The vertex parser skips `kind == "quadrant"` records, so a pre-2026-11-01 file
      loads without them and without a schema failure.

### P6 — tests + UI suites (1 d)
- [ ] C++: update `committed_attach_test.cpp` (23 refs — the "line from a quadrant" test
      at :288-306 becomes "line from a cardinal position is bound by point_on_circle"),
      `sketch_angle_dimension_test.cpp` (4), `sketch_profile_test.cpp` (2),
      `circle_modes_test.cpp` (2).
- [ ] UI: `snapMatrix.test.ts` (11) — the "arc quadrant"/"circle perimeter" rows lose
      their Quadrant expectations; `viewportSceneRectangleDims.test.ts` (1) drops
      `snap_quadrant` from the fixture.
- [ ] Add `sketchQuadrantRemoval.core.test.ts` (real binary over IPC): draw a circle,
      assert no vertex with kind `quadrant`; snap to the perimeter, assert the committed
      point binds with `point_on_circle`; trim the circle, assert the arc endpoints are
      real vertices.

### P7 — docs + gates (0.5 d)
- [ ] `wiki/Sketch-Snap-Matrix.md`: re-measure and delete the two un-bindable rows
      (centre/quadrant) or re-label them; the matrix test prints the table.
- [ ] `wiki/Vertex-Unification-Plan.md`: Phase 4's quadrant line is superseded — note it.
- [ ] `wiki/2D-Sketch-System-Architecture.md:83`: delete the Quadrant Snap row.
- [ ] `wiki/Glossary.md:185,216`: drop the quadrant-snap entry.
- [ ] `CLAUDE.md` build troubleshooting: nothing to change.
- [ ] Update `Implementation-Log.md` at the end of the work (project convention).

---

## 6. Gates

Per `Sketch-Next-Session.md` §4, plus the new checks:

```
pnpm core:build
pnpm test:core            # 89/89 today; must stay green (minus deleted tests)
pnpm --filter desktop-ui exec tsc --noEmit
pnpm lint:architecture    # 5 checks + the new quadrant check
pnpm registry:write       # only if command shapes changed
pnpm --filter desktop-ui exec vitest run src/layout/viewport/snapMatrix.test.ts
```

---

## 7. Risks

| Risk | Mitigation |
|---|---|
| **zod rejects the reply** if core still emits `kind: "quadrant"` after the enum is narrowed — presents as a 15 s timeout, not an error | land P1+P2 before P3; the new lint check fails the build if a `quadrant` kind survives in C++ |
| Old documents lose a constraint | migration detaches + warns; survey says no real file references one |
| Trim highlight becomes unreachable (or the entity-first branch hides a real regression) | P4 explicitly re-tests the trim hover path; keep the branch if the plain order is not enough |
| Arc endpoints start sharing ids with the circle again through `resolve_shared_point` | P4 test asserts minted `vertex-N` ids for trim-created endpoints |
| Users miss the cardinal magnet | D1 decision; if it must stay, implement it as a snap-only candidate that binds via `point_on_circle` (adds ~1 d) |
| Persisted selection holds a dropped vertex id | selection resolution must ignore unknown ids — verify in P5 corpus test |

---

## 8. Estimate

| Phase | Days |
|---|---|
| P0 guard | 0.5 |
| P1 core emission | 0.5 |
| P2 snap candidates + filter | 1 |
| P3 protocol + TS | 1.5 |
| P4 trim verification | 1.5 |
| P5 migration + corpus | 1 |
| P6 tests + UI suites | 1 |
| P7 docs + gates | 0.5 |
| **Total** | **~7.5 d** |

Sequencing: P1 → P2 → P3 in one commit series (the build is only consistent at the end
of P3), then P4 and P5 in either order, P6 last, P7 with the final gates. Every phase
ends with `pnpm test:core` green; P4/P6 also need in-app verification before the branch
merges (`.deepseek/notes.md`: never commit untested).
