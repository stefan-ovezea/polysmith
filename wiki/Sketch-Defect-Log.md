# Sketch Defect Log

Running record of defects found while using the app (as opposed to defects found in the plan's inventory). One entry per report, with the evidence that exists and the evidence still needed. Entries stay **open** until a fix lands and the reporter confirms.

Added 2026-10-23 when the first report arrived during S2 verification on `feature/sketch-refactor`.

---

## D-001 — An extrude built from a sketch's profiles does not survive edits to those profiles

**Reported:** 2026-10-23, in-app, while smoke-testing the sketch branch. Three observations, all from the same construction: *a rectangle with two circles inside it, extruded, then the sketch edited.*

### Setup

Sketch on XY: rectangle `0,0 → 80,60`, circle A `(20,30) r=10`, circle B `(55,30) r=10`. Finished, then extruded. The sketch produces **3 profile regions**: `profile-poly-…-hole-circle-1-hole-circle-2` (the plate with two holes) plus `profile-circle-circle-1` and `profile-circle-circle-2` (the two discs). Selecting all three and extruding `new_body` or `join` gives **1 body** (measured headless).

### D-001a — Deleting a circle destroys the whole body (CONFIRMED, headless)

| | |
|---|---|
| **Steps** | Build the setup, extrude all three profiles, re-enter the sketch, delete circle A |
| **Expected** | The extrude keeps the plate and loses one hole (or refuses the delete and explains why) |
| **Actual** | The extrude flips to `dependency_broken` and the body count drops to **0**. The Logs panel gives the "will break the geometry" warning first; after that there is no repair action — the body is simply gone. |
| **Status** | **Open — confirmed** |

Evidence (headless, `cad_core.exe` driven over JSON-lines, no UI):

```
1. after extrude (3 profiles)  kinds=[root_part, sketch, extrude]  broken=[]  bodies=1  profiles=3
4. deleted circle-1            kinds=[root_part, sketch, extrude]  broken=[extrude]  bodies=0  profiles=2
```

Same result with `mode: "new_body"` and `mode: "join"`.

**Hypothesis (unverified):** the extrude stores `profile_ids[]` and re-resolves them on every recompute. Deleting the circle changes the region set — the outer profile's id literally encodes the holes (`…-hole-circle-1-hole-circle-2`) — so no stored id resolves, and the feature degrades to `dependency_broken` wholesale instead of degrading to the regions that still exist. The id being a *name of the boundary* rather than a stable identity is the likely root cause; this is the profile-level twin of the topological-naming problem the refactor just addressed for faces and edges.

### D-001b — Changing a circle's diameter silently re-shapes the extrusion (CONFIRMED — root cause found)

**Symptom.** Change one circle's diameter, and the extrusion silently becomes something else: two separate cylinders plus a thin-walled rectangle instead of one plate. No warning.

**Reproduced on the reporter's own file** (`projects/test1.polysmith`), headless, with a **0.287 mm** edit (circle r 8.713 → 9.0):

| | triangles | mesh volume | bbox (mm) |
|---|---|---|---|
| before | 12 | 101.87 cm³ | 82.87 × 61.47 × **20** |
| after r → 9.0 | 284 | **44.72 cm³** | 82.87 × 61.47 × **10** |

and the feature parameters visibly change:

| field | before | after |
|---|---|---|
| `side1.distance` | 20 | **10** |
| `operation` | `join` | **`new_body`** |
| `depth` | 20 | 20 (unchanged) |
| `mode` | `new_body` | `new_body` |

The halved `side1.distance` is the "thin wall"; `operation` flipping `join → new_body` is why the three selected profiles become three separate bodies.

**Root cause.** `core/document/impl/private_linked_extrude_refresh.inc` — the sketch-edit path that re-resolves a linked extrude. It builds a **fresh** `ExtrudeFeatureParameters` with a designated-initializer list (line 61-78) that only carries the *profile-derived* fields, then:

```cpp
feature.extrude_parameters = next_parameters.value();   // line 96 — whole struct replaced
feature.extrude_parameters->mode = mode;                // line 97
feature.extrude_parameters->target_body_id = target_body_id;  // line 98
```

Only `mode` and `target_body_id` are carried over. Everything else reverts to its struct default:

- `side1` → default `distance = 10.0` (the "unset" sentinel) — hence the body halves;
- `operation` → empty, which `normalize_extrude_parameters` then turns into `new_body` — hence the split;
- `extent_mode`, `side2`, `thin`, tapers, `start_offset`, `through_all` / `to_object` / `to_next` extents, `intersect_result`, `merge_result` — all silently dropped too. Any extrude that is not a plain one-sided distance loses its settings on the first sketch edit.

The two `extrude_helpers.cpp` sentinels confirm this was known in a partial way: `default_side` rewrites a `distance == 10.0` back to `abs(depth)` — but **`normalize_extrude_parameters` is only called on create/update, never on load** (call sites: `extrude_face_commands.inc`, `extrude_open_entity_commands.inc`, `extrude_profile_commands.inc`, three in `extrude_update_commands.inc` — none in the session/load path). So the damage is **persistent**:

| file | mesh volume | bbox height | stored `side1.distance` |
|---|---|---|---|
| test1 (healthy) | 101.87 cm³ | 20 mm | 20 |
| **test2 (after one diameter edit)** | **44.72 cm³** | **10 mm** | **10** |
| test3 (healthy) | 131.53 cm³ | 20 mm | 20 |

Loading test2 reproduces the thin body exactly — reloading does *not* repair it, and nothing warns. The only repair found is to re-assert the depth (`update_extrude_depth` → `side1.distance = abs(depth)`), which is why the panel's depth field "fixes" it by accident.

That makes D-001b worse than first assessed: an ordinary sketch edit writes a permanently wrong solid, and saving commits it.

**Fix shape (not applied).** Stop rebuilding the struct. Copy the existing parameters and overwrite only the profile-derived fields:

```cpp
ExtrudeFeatureParameters next = *feature.extrude_parameters;  // keep extents, operation, thin…
next.profile_id = …; next.profile_ids = …; next.plane_id = …; next.plane_frame = …;
next.profile_kind = …; next.profile_points = …; next.inner_loops = …;
next.additional_* = …; next.depth = depth;
feature.extrude_parameters = std::move(next);
```

That removes the whole class of "a field added later is silently dropped here", which is what bit this. The `default_side` sentinel in `extrude_helpers.cpp` should then be revisited — it exists to paper over this path.

**Regression test.** The headless recipe below already measures volume and bbox; assert `side1.distance` survives an `update_sketch_circle` and that the mesh volume changes only by the hole area (≈ π·(9²−8.713²)·20 ≈ 0.32 cm³), never by half.

**Fix applied (2026-10-23, branch `feature/sketch-refactor`, uncommitted).** `refresh_linked_extrude_features` no longer rebuilds the struct. It now calls the same builder the creation path uses (`make_extrude_parameters_for_profiles`), copies the existing parameters, and lets `preserve_extrude_source_geometry` overwrite **only** the profile-derived fields:

```cpp
ExtrudeFeatureParameters next_parameters = *feature.extrude_parameters;
preserve_extrude_source_geometry(next_parameters, *built_parameters);
next_parameters.depth = depth;
feature.extrude_parameters = next_parameters;
```

A field added later is now preserved by the copy instead of silently lost. A profile that the builder rejects (e.g. two holes now touching) marks the feature `dependency_broken` with the builder's message instead of failing the whole sketch edit — `make_extrude_parameters_for_profiles` calls `validate_hole_separation`, which throws.

**Verified on the reporter's own file** (`projects/test1.polysmith`, r 8.713 → 9.0):

| | side1.distance | operation | sketch_edge_ids | volume | bbox |
|---|---|---|---|---|---|
| before fix | 20 → **10** | join → **new_body** | 16 → **0** | 101.87 → **44.72** | 20 → **10** |
| after fix | 20 → **20** | join → **join** | 16 → **16** | 101.87 → 101.87 | 20 → 20 |

Regression test: `native/cad-core/tests/extrude_refresh_test.cpp` (registered in CMake). Two cases — the exact fields that were reset, and the geometry (one body, full depth, slab volume). **Fail-before verified**: reintroducing the two resets makes the test fail on `"after edit: side1.distance preserved"`; removing them passes.

Note the slab volume is *unchanged* by the hole radius by construction — the three profiles' union is the full slab — so the volume assertion is what catches the halving, not the circle size.

**Status:** **Fixed, awaiting the reporter's in-app verification.**

The user-facing silence is a separate defect worth fixing at the same time: the refresh sets `status = "healthy"` and clears `dependency_warning` unconditionally after the rebuild, so a geometry-changing refresh reports nothing even when the result differs from what the user last saw.

### D-001c — Black screen when opening the extrude panel (ROOT CAUSE FOUND)

Reported: after the body had split into the cylinder/frame shape, opening the extrude to edit it **crashed the app**, repeatedly — "the screen goes black".

**Root cause.** The UI error capture added on this branch produced it on the first crash:

```
[ui-error] render error: TypeError: Cannot read properties of undefined (reading 'getPoint')
```

`getPoint` is a Three.js `Curve` method, and the only place this app can reach it on an *undefined* object is `Path.closePath()`:

```js
// three/src/extras/core/Path.js
closePath() {
  const startPoint = this.curves[ 0 ].getPoint( 0 );   // ← undefined.getPoint(0)
```

`curves[0]` does not exist when a `THREE.Shape` was built from a loop with **fewer than three points** — `moveTo` alone never pushes a curve. The shape is built by `shapeFromProfileLoops` ([primitiveObjects.ts:24](C:\SRC\polysmith\apps\desktop-ui\src\utils\viewport\primitiveObjects.ts)) from `exactProfileContour(profile)` ([viewportScene.ts:883](C:\SRC\polysmith\apps\desktop-ui\src\lib\viewportScene.ts)), which prefers the exact `boundary_edges` when the profile carries any — so a degenerate edge list (empty after the linked-extrude refresh) yields a 0-point contour **while the chord sample still looks fine**, defeating the old `profilePoints.length < 3` guard.

Why the panel: opening the extrude from the timeline re-enters the sketch's timeline state and the scene rebuilds the profile fills. The render throws, `DiagnosticErrorBoundary` catches it and (before the fix) returned `<div />` — the black window — destroying the Logs panel that held the message. Two faults stacked: one crash, one that hid it.

**Same root cause as D-001b.** The refresh in `private_linked_extrude_refresh.inc` drops the profile-derived fields, so both the extent (thin body) and the geometry that the renderer needs (empty contour) go missing together. Fixing D-001b removes the trigger; the guards below remove the ability to blank the window.

**Guards applied (uncommitted, `tsc` clean):**
- `shapeFromProfileLoops` returns an empty shape for a loop under three points — no caller can reach `closePath()` with no curves.
- the sketch profile fill bails when `exactProfileContour` yields fewer than three points (previously only the chord sample was checked).
- the `polygon_extrude` primitive skips `ExtrudeGeometry` for a degenerate loop and emits an empty buffer.

**Observability (this is what found it):** `log_ui_error` in the Rust shell appends to `%APPDATA%\polysmith\ui-errors.log`, and the boundary now writes the real `componentStack` there and renders the message in dev instead of a blank div. It took one reproduction.

**Status: fixed at both ends.** The trigger is gone — with D-001b fixed, the refresh keeps `sketch_edge_ids` (16 → 16 instead of 16 → 0), so the contour is no longer empty. The guards above mean a degenerate profile from any other source still cannot blank the window. Verified against the reporter's file; needs their in-app confirmation.

**Follow-up: the guard introduced a second crash, caught by the same capture.**

```
[ui-error] render error: TypeError: Cannot read properties of undefined (reading 'count')
```

The `polygon_extrude` guard substituted `new THREE.BufferGeometry()` for the failed `ExtrudeGeometry` — a geometry with **no attributes at all**. Three's non-indexed draw path reads `attributes.position.count`, so rendering it threw on `undefined.count`. Fixed by giving the substitute an explicit zero-length position attribute:

```ts
geometry.setAttribute("position", new THREE.BufferAttribute(new Float32Array(0), 3));
```

Worth keeping in mind as a rule: an empty `BufferGeometry` is not a safe placeholder for a mesh — it must carry an (empty) `position` attribute. Every other construction site in the app builds from points or sets attributes explicitly; this was the only bare one.

Two crashes, two single-cause fixes, each identified within one reproduction — that is the practical value of `ui-errors.log`.

### D-001d — A stripped payload was treated as authoritative geometry

Reported right after the two fixes above: opening an extrude for editing made the body disappear ("0 faces selected"), and cancelling logged

```
[cad_core] Polygon extrude requires at least three profile points
```

**Cause.** The document-state payload deliberately omits derived geometry — extrude parameters arrive without `profile_points`, `boundary_edges` or the `sketch_*` caches (confirmed by measurement: a file stores 64 profile points, the reloaded document state carries 0). The panel snapshots what it was given, so on cancel it sends those empty arrays back through `update_extrude_parameters`, which merged them over the stored parameters and then failed validation on a field that was never meant to be authoritative (`extrude_feature.cpp:92`).

The same class of problem as D-001b, one level up: **the ids are the durable reference, the geometry is a cache** — and three different code paths each treated the cache as truth.

**Fix.** `refresh_extrude_profile_geometry(document, parameters)` in `private_extrude_profile_parameter_helpers.inc` re-derives the profile-derived fields from `profile_ids` and the owning sketch (same builder as creation), leaving extents, operation, thin settings and targets untouched. `update_extrude_parameters` calls it before validating. When the sketch or a profile no longer resolves — or the profile is now invalid — it returns false and the stored geometry is kept, so the error the user sees is about the profile, not about an empty cache.

Covered by `test_update_tolerates_stripped_profile_geometry` in `extrude_refresh_test.cpp`: the parameters are stripped to exactly what the wire carries, the update must re-derive rather than throw, and the body must still compile.

**Status:** fixed, awaiting in-app verification.

### D-001e — Editing an extrude: body vanishes, Confirm does nothing

Reported after D-001b/c/d were fixed: *"the body disappears again. I re-select the surfaces and press cancel and the body comes back. confirm button does nothing or sometimes is disabled."*

What is known:

- Cancel now works (the body returns), so D-001d's re-derivation is doing its job on the restore path.
- `beginTimelineEditSession` (`app/timelineEditSession.ts:32`) does **not** roll the timeline back for non-sketch features — it only clears edit visibility — so the disappearing body is *not* the standard "edit in context" rollback. Something in the open path removes or invalidates the feature.
- `ActiveExtrudePreview.onConfirm` → `confirmActiveExtrude` only clears the action, clears a cut selection and restores the timeline cursor. It does not commit geometry — the preview *is* the feature. If nothing re-creates it, Confirm legitimately does nothing visible.
- `PendingExtrudePreview.onConfirm` silently returns when nothing is selected (`selectedSketchProfileIds.length === 0` and no face and no thin entities) — a Confirm that "does nothing" is exactly what that path looks like.

**Instrumented (uncommitted).** Failures in `createExtrudeFromProfiles` and in `ActiveExtrudePreview.onConfirm` are now reported to `%APPDATA%\polysmith\ui-errors.log` as well as the Logs panel, so the next reproduction names the failing call instead of leaving an inert panel.

**Needed to finish it:** the Logs-panel lines from opening the panel to pressing Confirm (especially any `extrude action error:`), plus the tail of `ui-errors.log`. With those, either the create is failing (and the message says why) or the panel is being driven from the pending path with an empty selection — and each has a different fix.

**Cause confirmed by the reporter (2026-10-23):** *"the panel on re edit starts with 0 surfaces. One presumption is that the sketch is closed and made invisible. Making it visible and reselect the surfaces restores the body."* That is exactly it — the panel is the selection-driven create preview, and it opened against an empty selection.

Confirming an extrude runs `hideSourceSketchAfterConfirm` (`ActiveExtrudePreview.tsx:264`), which adds the source sketch to `hiddenFeatureIds`. Re-editing from the timeline then set `extrudeAction` with the feature's `profile_ids` but never restored either half of the input state:

- the sketch stayed hidden, so its regions were not pickable;
- nothing selected the profiles, so `selectedSketchProfileIds` was `[]`.

With `0 faces selected`, `PendingExtrudePreview.onConfirm` hits its early return and does nothing — the "Confirm does nothing" — and any operation/mode change re-issues an extrude update against an empty selection, which is what leaves the viewport empty until the user re-picks by hand.

**Fix (uncommitted, UI only).** `editExtrudeFeature` (`app/timelineFeatureEdit.ts`) now calls a new `revealSourceProfiles(sketchFeatureId, profileIds)` context callback before opening the panel. It:

- un-hides the source sketch and **records whether it was hidden**, so the panel closing puts it back (`App.tsx` effect on `extrudeAction === null`) — an edit no longer leaves the sketch shown for good, which was the first follow-up report;
- filters the feature's stored profile ids against the live regions before selecting them, and says *"N of M source faces still exist — the others were deleted from the sketch"* when some are gone (deleting a circle rewrites the plate's id, so the stored set is partly stale — this is why a re-edit after a deletion arrived with one of three selected);
- selects what survives (chained `selectSketchProfile`, additive after the first; the batch `select_sketch_profiles` command exists in the core but has no TS builder yet).

**Second follow-up — Confirm inhibited on a broken feature.** The panel disables Confirm on `hasPreviewError`, and `previewErrorFor` returns the `dependency_warning` for any `dependency_broken` feature. So after deleting a face the feature could never be confirmed again, even with the surviving faces re-picked — the one action that would repair it was the one blocked. Now a non-empty selection clears the block (`previewErrorBlocksConfirm`), and confirming re-binds the feature to the new selection through `updateExtrudeProfiles` before closing.

**Third follow-up — Confirm pressable but still doing nothing.** Two causes, both in the re-bind path, plus one in the panel:

1. **`update_extrude_profiles` never cleared the failure state.** It rewrote `extrude_parameters` and `parameters_summary` and stopped there, so a successfully re-bound feature kept `dependency_broken = true` — the body stayed suppressed and Confirm looked inert. It now sets `status = "healthy"` and clears `dependency_broken` / `dependency_warning`, and copies the existing parameters while overwriting only the profile fields (it used to force `side1.distance` back to `depth`, discarding a two-sided or symmetric extent).
2. **The panel displayed a stale count.** Confirm acted on the live selection while the header showed `extrudeAction.profileCount` — the snapshot from when the panel opened — so the two could disagree. An edit session now displays the live selection count, i.e. what Confirm will use.
3. **Confirming with nothing selected now says so** instead of closing silently ("Select the sketch faces this extrusion should use, then confirm.").

Confirm also writes one line to `ui-errors.log` recording `{edit, broken, liveProfiles, actionProfiles, repairing}` per press, so a future "it did nothing" is data rather than a guess.

Covered by `test_rebinding_profiles_heals_a_broken_extrude` (delete a circle → feature flagged → re-bind → flag cleared, `side1` preserved, body compiles). The old code never wrote those flags, so the assertions cannot pass against it.

Same class of fix is needed for loft/revolve/sweep; they are not touched here.

**Parity pass (2026-10-23).** Checked every profile-consuming feature for the same two faults:

| feature | core repair path clears the flag | UI edit session reveals inputs |
|---|---|---|
| extrude | was broken — fixed here | was missing — fixed here |
| loft | already correct (`loft_commands.inc:58-60`) | **was missing — now fixed** |
| revolve | already correct (`revolve_commands.inc:77-79`) | **was missing — now fixed** |
| sweep | already correct (`sweep_commands.inc`) | **was missing — now fixed** |

So only the extrude path had the sticky-flag bug; the other three already cleared `status`/`dependency_broken`/`dependency_warning` on a successful re-bind. Their *edit sessions*, however, all opened with the same empty selection, so they got the reveal treatment too.

`revealSourceProfiles` now takes **profile ids** instead of a sketch id, because the caller may not know what owns them: a loft's sections can span several sketches, and revolve/sweep reference one profile plus a sketch-level axis or path. It resolves the owning sketches from the feature history, un-hides only those that were hidden, and the visibility restore covers all four session types (extrude/loft/revolve/sweep).

Their panels have no `previewError` gate, so the inhibited-Confirm fault was extrude-only.

### D-001f — Clicking a timeline item clears the sketch-profile selection

Reported alongside D-001e, marked "not extremely important for now": *"clicking with the mouse on any item in the timeline deselects all the surface in the panel."*

Relevant because the extrude edit panel's input *is* the profile selection (D-001e): a timeline click during a panel session silently empties its input.

**Fixed in the core (2026-10-24).** `DocumentManager::select_feature` cleared `selected_sketch_profile_ids` for every feature kind. It now carries the profiles a profile-consuming feature *consumes* into the selection — `extrude` (its `profile_ids`, falling back to `profile_id`), `loft` (every section's profile), `revolve` and `sweep`. That is the honest meaning of "select this extrude": its input surfaces light up with it, and the edit panel can never be emptied by a timeline click. Every other feature kind (sketch, box, cylinder, …) keeps the old clearing behaviour, so a timeline click still reads as a plain selection action.

Test: `tests/select_feature_profiles_test.cpp` — selecting the sketch clears, selecting the extrude selects exactly its consumed profiles (primary id included), and selecting the sketch again clears, i.e. the general path is unchanged.

---

## Behaviour confirmed as intended (2026-10-23)

Observations from use after the D-001 fixes, recorded so the contract is explicit rather than rediscovered:

**1. The body flags while the sketch is still open, and recovers by itself.** Enlarging a circle until it overlaps its neighbour flags the extrude immediately (before Finish Sketch); shrinking it back clears the flag and the body returns, with no user action. This is `refresh_linked_extrude_features` running on every sketch edit and setting/clearing `status`/`dependency_broken` accordingly. Live feedback plus automatic recovery is the intended contract: a warning that only appears after Finish Sketch would describe a body the user is no longer looking at.

**2. Re-drawn geometry does not heal the feature, and should not.** Deleting a rectangle line flags the body; drawing the line again leaves it flagged, and Confirm's re-bind is what fixes it. This is correct, not a regression: a re-drawn line is a *new entity id*, so the profile region's identity genuinely changed and the stored reference cannot re-resolve. D-001a's "degrade, don't vanish" applies — the feature degrades, says so, and is repairable. Undo (which restores the original ids) is the path expected to recover on its own; if a **Ctrl+Z** of that deletion leaves the body flagged, that *is* a defect and should be filed here.

**3. Flag-on-edit is a deliberate trade.** Some CAD tools only report broken references on finish, to avoid warnings flashing while sketching. This one reports immediately. Acceptable as-is; if it becomes noisy on complex sketches, gate the *message* (not the state) on the sketch being finished.

The earlier evidence stands as context (core survives all four edit commands; test3 vs test4 differ only in selection and undo history).

**The core is not the culprit — probed and it survives.** On the exact broken state (extrude `dependency_broken`, 0 bodies), all four commands the extrude panel sends were driven headlessly and every one returned normally:

| command | reply |
|---|---|
| `update_extrude_depth` | `feature_changed` |
| `update_extrude_mode` | `feature_changed` |
| `update_extrude_profiles` (re-sending the deleted profile id) | `error: Sketch profile not found: …` — a clean structured error, no crash |
| `update_extrude_parameters` | `feature_changed` |

Core process alive afterwards (`dead=false`), and a following `get_document_state` answered. So the crash is **UI-side**: an exception during render of the extrude panel or the viewport sync, i.e. the React error boundary or a blank/frozen window — not a dead core.

**Reporter's own hypothesis (2026-10-23), worth taking seriously:** the crash is in the **extrude panel**, not the rebuild — the panel was visible for a frame before the screen went black. Related history they supplied: an earlier branch fixed surface select/deselect, where the last surface changed colour but the panel kept reporting "1 surface selected" and the preview body was never deleted; they are unsure whether re-editing an extrude was retested after that fix. If so, the crash is a regression from the selection rework (PR #92/#93 era) interacting with the preview-body teardown, and the panel's `selectedFaceTargetId` / preview cleanup is the place to look.

**How to capture the stack next time — DevTools first, then reproduce.** In a Tauri dev build, DevTools is a *separate* WebView2 window, so it usually survives the page going black. Open it before reproducing (F12, or right-click → Inspect in dev), keep the Console tab focused, then trigger the crash: the exception and its component stack stay in that window even when the app's viewport is dead. That single stack would end the guesswork — it names either a component in `ExtrudePreviewPanel` / `ActiveExtrudePreview` or a function in the scene sync.

If DevTools is unavailable, a temporary hook is the fallback: a `window.onerror` + `unhandledrejection` listener that appends the stack to the in-app Logs panel *and* writes it to a file (the panel dies with the screen, the file does not).

Still needed to pin it:

1. **The terminal output from the `pnpm dev` window at the moment the screen goes black.** A Tauri dev WebView is a Rust window: a render-time panel crash and a WebView/process failure leave different text there, and that text is the only place the distinction survives a black screen. This is now the highest-value artefact.
2. **`projects/test1.polysmith` does not reproduce it on its own** — it loads to 1 healthy body, and the extrude panel's commands all answer cleanly (table above). So the crash needs the *edited* state; a save taken **after** the split-but-before-the-edit would replay it exactly.
3. If the window survives long enough: devtools → Console (Tauri dev builds allow F12 / right-click → Inspect); a React stack names the component, a WebGL context-loss message points elsewhere entirely.

Note: `update_extrude_profiles` returning "Sketch profile not found" is the *correct* behaviour for the deleted id — the interesting part is that the other three commands accept the broken feature silently and re-emit it as `feature_changed` with `dependency_broken` still set. If the panel keys off "changed" to rebuild its local state, that silent acceptance is the likeliest place for the UI to trip.

**Session-state evidence (2026-10-23, test3 vs test4).** The reporter saved the crashing session (`test3`) and then the same document after a restart + reload (`test4`). Byte hashes differ; the structural diff is **7 paths**, and only two carry meaning:

| path | test3 (crashes) | test4 (reloaded, survives) |
|---|---|---|
| `selected_sketch_profile_ids` | 3 ids | `[]` |
| `selected_sketch_profile_id` | one profile id | `null` |
| `undo_step_names` | `["Extrude Profiles","Sketch Edit","Sketch Edit"]` | `["Dimension","Dimension","Dimension","Start Sketch On Plane"]` |
| `metadata.thumbnailDataUrl` | — | — (screenshot, irrelevant) |

Everything else — geometry, `extrude_parameters`, feature ids, broken flags — is identical. So **the crash is not in the saved data**: the same bytes that crash in-session open and edit cleanly after a restart. It depends on live state that the file does not carry, and the two candidates the diff exposes are (a) the multi-profile selection still holding three ids from *before* the sketch edits, and (b) the undo/redo stack from the editing session.

**Two mechanisms fit "screen goes black and the terminal prints nothing", and one question separates them:**

1. **Renderer/React death** — the window frame is still there (title bar, can drag it) with black content. A render-time throw unmounts the tree; the Tauri host and the core are alive, so `pnpm dev` has nothing to print, and the *absence* of the usual `ELIFECYCLE … 4294967295` line is exactly what is observed. A `window.onerror` hook would capture this.
2. **WebView2 renderer crash** — the window closes or its content goes black while `polysmith.exe` and `msedgewebview2.exe` may still be listed. Nothing reaches the node terminal either.

**Question that settles it:** when it goes black, is the window still open with its title bar (→ 1), or does it disappear (→ 2)? Checking `Get-Process polysmith, msedgewebview2` right after answers it too.

---

## D-002 — Theme colour with alpha is fed to `THREE.Color`, so the alpha is dropped

**Reported:** 2026-10-23, in-app. Only console/log line seen during the D-001 session:

```
warn  react_warning  THREE.Color: Alpha component of rgba(186, 194, 222, 0.82) will be ignored.
```

**Cause (confirmed):** the token `--cad-sketch-dimension-label` is defined as `rgba(186, 194, 222, 0.82)` in `config/themes/catppuccin-mocha.json:140`. `themeColor()` returns the raw CSS string, and the sketch renderer assigns it to a material colour:

- `layout/viewport/draftDimensionPreview.ts:290`, `:492`, `:510`
- `utils/viewport/sketchObjects.ts:650`

Three.js ignores the alpha channel of a CSS `rgba()` passed to `Color.set`/`new Color`, so the intended 82 % opacity is **silently discarded** — dimension labels draw fully opaque under that theme, and the warning fires once per distinct string per session.

**Where it is fine:** `ToolSchematicView.tsx:135,144,151` uses the same token as an SVG `stroke`/`fill`, where CSS applies the alpha correctly.

**Fix shape (not applied):** keep the token as-is for CSS consumers and split it at the Three.js boundary — either parse the alpha into `material.opacity` (and set `transparent`), or add a second solid token (`--cad-sketch-dimension-label-solid`) for the material path. The first is truer to intent; it also removes the warning.

**Severity:** cosmetic (wrong opacity, log noise), but it is a real instance of the theme rule's blind spot — "no hardcoded colours" is enforced, while "no alpha in material colours" is not. Worth an architecture grep later: `themeColor(...)` results must not reach `THREE.Color` with an `rgba` string.

**Fixed:** the split now happens at the boundary. `utils/viewport/themeColor.ts` gained `splitCssColorAlpha`, `themeColorSolid` and `themeColorWithAlpha`; the three material sites in `draftDimensionPreview.ts` use `themeColorSolid`, so the material receives an alpha-free colour (no Three warning) and keeps its explicit `opacity`/`transparent`. The canvas-2D consumers in `sketchObjects.ts` deliberately keep `themeColor`: CSS applies alpha there correctly, so stripping it would *introduce* a bug. A token-driven sweep confirmed those are the only material consumers of the two alpha-bearing sketch tokens (`--cad-sketch-dimension-label`, `--cad-sketch-label`), and the other sixty alpha tokens in the theme files are never fed to a `THREE.Color`. Five tests in `themeColor.test.ts` pin the parser (rgba, rgb, eight/six-digit hex, plain names, clamped and non-numeric alpha). The rule is written into CLAUDE.md's UI Theme Rules.

---

## D-003 — "sketch object vertex-32 not found": the partial sketch reply left the UI's arcs stale

**Reported:** 2026-10-23, in-app, during normal sketching:

```
07:05:10 error desktop_ui  uncaught error: sketch object vertex-32 not found
07:05:11 error react_warning [cad_core] Sketch lines must have non-zero length (moving connected points)
07:05:11 error cad_core      Sketch lines must have non-zero length (moving connected points)
07:05:11 error desktop_ui  uncaught error: sketch object vertex-32 not found
```

**Root cause (confirmed by reading the reply path and reproducing the reply shape headless).** `emit_sketch_entities_changed` (`native/cad-core/src/app/command_context.h`) answers a sketch edit with the **partial** `sketch_entities_changed` event whenever the sketch's dimension list is unchanged. That event only transports **line endpoints and vertex coordinates** — yet it was being emitted for every command that happened not to touch a dimension, including:

- `add_sketch_fillet` / `delete_sketch_fillet` (arcs + fillet records change),
- `add_sketch_chamfer` / `delete_sketch_chamfer` (chamfer record),
- `add_sketch_circle` / `add_sketch_polygon` / `add_sketch_ellipse` / `add_sketch_spline` / `add_sketch_slot` (non-construction ones add dimensions and were safe by luck; construction ones were not),
- `update_sketch_vertex` / `transform_sketch_entities` moving a circle, arc, ellipse, spline or slot,
- `delete_sketch_text`.

After such an edit the UI's document held **stale arcs / circles / fillet records**. The planegcs WASM bridge builds its solver sketch from that document, so a later endpoint drag pushed a stale arc whose endpoint vertex the patch had removed → `sketch object vertex-N not found`, thrown inside a pointer-move handler and reported as an **uncaught UI error**. The rounded rectangle (four fillets in one command) made this easy to hit, but every fillet/chamfer in the app shared the cause.

The second log line, `Sketch lines must have non-zero length (moving connected points)`, is the core **correctly refusing** a different thing: an endpoint drop that lands on a directly connected neighbour, collapsing a segment. The UI was committing such drops, so the user saw the refusal as an error.

### Fixes applied

1. **Reply guard** — `sketch_partial_event_safe` compares the serialized non-line sketch state (everything the event cannot carry: arcs, circles, polygons, ellipses, splines, slots, texts, fillets, chamfers, constraints, relations, anchors, dimension values, next vertex index). The partial event is only used when all of it is byte-identical; otherwise the command answers with the full `document_state`. Comparing serialized state also removes the old O(n²) id scans on a dense sketch, and covers entity kinds added later.
2. **Solver hardening** — `PlanegcsBridge.solve` catches a primitive-lookup failure and returns `status: 2 (Failed)`; every caller already falls back to the unconstrained preview. A stale snapshot can no longer throw through a pointer handler.
3. **Degenerate-drop guard** — `endpointDropWouldCollapse` refuses a commit that would land on a connected line/arc endpoint (within 0.01 mm) and restores the preview, so the core no longer has to reject it.

**Tests.** `tests/sketch_partial_event_test.cpp` (guard: vertex-only construction edits stay partial; fillet, circle-centre moves and dimension-value changes force a full state), `lib/ipc/sketchPartialEvent.core.test.ts` (reply shape end-to-end: fillet → `document_state`, construction vertex move → `sketch_entities_changed`), `layout/viewport/endpointDragPointerUp.test.ts` (degenerate-drop guard).

**Status:** Fixed in the working tree on `feature/sketch-refactor` — awaiting a re-test in the app (the running binary must be restarted: the app holds `native/cad-core/build/cad_core.exe`, so the new core only lands after the app is closed and rebuilt).

**Note for future commands.** A new sketch command must not assume the partial event is available: either keep its change set to line endpoints + vertex coordinates, or let the guard fall back to the full state. The guard is conservative by design.

---

## D-004 — "Unsupported sketch tool: split": two toolbar tools were missing from the core whitelist

**Reported:** 2026-10-23, in-app, clicking the Split tool (repeatedly):

```
07:31:16 error react_warning [cad_core] Unsupported sketch tool: split
07:31:16 error cad_core      Unsupported sketch tool: split
... again at 07:31:27, 07:31:40, 07:31:55
```

**Cause (confirmed).** `kSupportedSketchTools()` in `core/sketch/sketch_tool_ids.h` — the single canonical list both `set_sketch_tool` gates delegate to — listed 18 ids but the toolbar can arm 20: **`corner` and `split` were missing**. Arming either tool sends `set_sketch_tool`, the core rejects it, and the user gets an error toast per click. The tool commands themselves (`split_sketch_entity`, `corner_trim_preview`, `corner_trim_sketch_entities`) were fully implemented; only the session's active-tool record was refused, so the tool still functioned while the core's `active_sketch_tool` stayed on the previous tool — a latent inconsistency for anything that keys off it.

**Fix.** Added `corner` and `split` to the canonical list, and pinned the whole vocabulary:
- `tests/tool_whitelist_test.cpp` gained `test_ui_tool_vocabulary_is_covered`, asserting the canonical list contains every id the UI's `SketchTool` union can arm (Shape2D + select/project/dimension/move/text).
- `lib/ipc/sketchToolVocabulary.core.test.ts` drives `set_sketch_tool` over the real IPC path for all 20 ids and checks an unknown id is still refused.

**Status:** Fixed in the working tree; core rebuilt. Any future tool added to the UI must be added to the canonical list or the C++ suite fails by name.

---

## D-005 — The B-spline evaluator iterated upward, so curves missed their own poles

**Found:** 2026-10-23, while building the fit-point spline (T2.1). Not reported by a user; the fit test (`test_spline_fit_mode_interpolates_points`) failed with a mismatch at u = 1/3 and the printed poles turned out to be *correct*, which pointed at the evaluator.

**Cause (confirmed).** `spline_eval` in `native/cad-core/src/core/sketch/spline_math.h` — and its byte-for-byte TS twin in `apps/desktop-ui/src/lib/splineCurve.ts` — implemented De Boor with an **upward** inner loop:

```cpp
for (int r = 1; r <= degree; ++r)
  for (int i = span - degree + r; i <= span; ++i)   // wrong direction
    d[i] = (1 - alpha) * d[i - 1] + alpha * d[i];
```

De Boor must walk **down** from the span (`i = span → span - degree + r`) so `d[i-1]` is still the previous round's value. Going up blends a value the same round has already overwritten, which evaluates a different curve: a degree-3 spline with poles (0,0) (3,4) (7,5) (10,0) — a single clamped segment, i.e. a plain Bézier — returned (1.89, 2.37) at u = 1/3 where the curve is (3, 4).

**Impact.** Every sampled spline point (profile walk guards, viewport/pick sampling, draft preview, trim-engine probes) was computed from that wrong evaluator, so a spline's interior could sit away from both its poles and its exact OCCT edge. Endpoints were unaffected (both sides short-circuit at u = 0/1) and the wire builder uses `Geom_BSplineCurve`, which is why extrudes still looked plausible.

**Fix.** Downward loop on both sides, with a comment naming the failure mode. Regression tests: `test_spline_eval_matches_the_bezier_form` style assertions live in `tests/spline_test.cpp` (fit interpolation at Greville parameters) and `apps/desktop-ui/src/lib/splineCurve.test.ts` (direct Bézier value at u = 1/3, endpoints, degree 5 finiteness, plus the fit-solver cross-check against the core's exact poles).

**Status:** Fixed. `pnpm test:core` 79/79 and the offline UI suites green afterwards; no test had encoded the wrong values.

---

## D-006 — A new constraint kind silently wiped the whole document state from the UI

**Found:** 2026-10-24, while wiring the collinear constraint button (S1). The new IPC test timed out after 15 s instead of failing with an assertion.

**Cause (confirmed).** `line_relations[].kind` in `apps/desktop-ui/src/lib/schemas/ipc/documentStateSchema.ts` was a closed zod enum (`equal_length`, `perpendicular`, `parallel`, `tangent_*`). The core's `set_sketch_collinear_constraint` stores a relation with `kind: "collinear"`, so the very first `document_state` reply containing that relation **failed validation**. The core was fine (the direct NDJSON probe showed the relation stored and the command replying in milliseconds); the UI's message parser rejected the line, and the harness's `onLine` swallowed the parse error — hence the timeout rather than an error.

If this had shipped, creating a collinear constraint would have made the document state stop updating in the app: every subsequent reply carrying that relation would be dropped, with no visible error. Same class of trap as the `chamfer mode` and `line_type` wounds already recorded — a closed enum is a *silent* filter, not a check.

**Fix.** Added `collinear` to the document schema's relation enum (and to `SketchLineRelationEntry` and the viewport constraint enum, which was missing it too). Two related gaps found in the same pass, both fixed: collinear relations and `SketchConstraint{kind:"midpoint"}` were never emitted as viewport **badges** at all, so a constraint that constrained the sketch was invisible — the badge emitters now handle `collinear` (label "L") and `midpoint` (label "M", on the host line's midpoint). Tests: `sketchConstraintPick.test.ts` (5 pure pick-order cases) and `sketchCollinearMidpoint.core.test.ts` (2 end-to-end cases: the relation exists in viewport state, and the midpoint constraint pulls the bound endpoint onto the host midpoint).

**Status:** Fixed. `pnpm test:core` 79/79, UI suites green.

---

## D-007 — A `warning` status silently deleted the body (S30b, fixed)

**Found:** 2026-10-24, while making S30b work. **Status:** fixed, with three tests.

**The reported defect.** Deleting one circle from a sketch took the whole extrude with it: the stored profile id of the deleted circle stopped resolving, `refresh_linked_extrude_features` set `dependency_broken` and skipped the rebuild, so a plate with one hole vanished instead of becoming a plate with no hole.

**Two separate faults, and the second is the general one.**

1. *The refresh treated "one profile gone" as "everything gone".* It now keeps the profiles that still resolve, rebuilds from them, and reports the shortfall ("1 of 2 profiles no longer exist after the sketch edit; extruding the 1 that remain.") with `status = "warning"` and `dependency_broken = false`.

2. *`status == "warning"` was overloaded to mean "do not build".* Three places skipped any feature whose status was `warning`: `compile_bodies_modifier_replay.inc` (body building), `compile_bodies_mesh_requirement.inc` (whether meshes are needed) and `viewport/impl/extrude_feature_emit.inc` (the legacy primitive emitter). So the moment the refresh reported the degrade the honest way, the body disappeared anyway — with no error, no warning in the viewport, and a "warning" feature in the timeline. The first attempt at S30b produced exactly that, which is why it was reverted the same day and investigated instead of shipped.

**Fix (the general rule).** `dependency_broken` is the flag that means "there is nothing to build"; `status == "warning"` is advisory and must not suppress geometry. All three sites now skip on `dependency_broken` alone, and the body-build loop wraps `build_feature_shape` in a `try/catch` so a feature that genuinely cannot produce geometry (a degenerate profile, an OCCT failure) is skipped without aborting every other body in the scene — previously the warning skip was also what kept those throws from escaping.

**Tests.** `tests/extrude_profile_degrade_test.cpp`: (a) deleting the hole circle keeps the plate — not broken, warning set, warning text explains the shortfall, the body is the full slab, undo restores the holed plate; (b) a feature whose status is `warning` but whose dependency is intact still produces a body (the general rule, independent of S30b); (c) a broken dependency still suppresses the body (the rule's other half). `tests/extrude_refresh_test.cpp`'s repair-path fixture was updated: deleting a circle no longer breaks the extrude, so that test now asserts the degrade and then marks the feature broken directly, because it is about re-binding profile ids, not about how a feature got broken.

---
## D-008 — A constraint on a standalone point is stored but does not move it (S10 blocked on this)

**Investigated:** 2026-10-24. **Status:** open; the S10 first slice was implemented, measured and reverted.

**What was built.** `point_on_circle` — a vertex anchored to a circle or an arc — mapped to `P2PDistance(center, point) == radius`, with a `set_sketch_point_on_circle_constraint { vertex_id, curve_id }` command, a manager entry point and a four-case test (point inside the circle, point outside, point on an arc, a line refused by name).

**What the test showed.** With a circle at the origin (r = 10) and a standalone point at (2, 0), the command stores the constraint — the probe prints `point_on_circle:vertex-2:circle-1`, `solver_ok=1` — but the point stays at (2, 0) and `solver_dofs` reads 2, i.e. the constraint contributed nothing to the system. So the failure is upstream of the mapping branch: either the standalone point is not a free solver parameter (so the constraint has nothing to act on) or the solve result is written back through the `SketchPoint` record in the wrong order — T1.4 recorded that `rebuild_sketch_vertices` treats the previous vertex table as authoritative for kind `point` and writes it back into the record, so a solved vertex position can be overwritten by the record that was never refreshed.

**Why it was reverted.** A constraint kind that silently does nothing is worse than no constraint: it would appear in the panel, count as a DOF, and not move the geometry. The command, the solver mapping, the schema entry and the test were removed together; the tree is back to the committed state with the core suite green (82/82).

**Next step.** Compare the parameter vector a *line endpoint* and a *standalone point* produce (`constraint_solver_entity_mapping.inc` fills `gcs_points_` from `params.vertices`, so both should be there), then check `num_points` / the appended-focus solve path for kind `point`, and only then re-add the constraint kind. The rule to keep: a new constraint kind ships with a test that proves the geometry MOVES, not just that the record exists.

---


D-001a is a **trust** defect of exactly the kind P0 exists for: the user did something ordinary, the app warned once, and the model was destroyed with no way back. If D-001b confirms, the pair becomes the strongest argument for making profile identity provenance-based rather than boundary-name-based — which is the same fix shape as `dependency_ambiguous` in S4.

### Reducing it to a test (when someone picks this up)

The headless recipe is reproducible with no fixture file — 7 commands, about a second:

```js
create_document
start_sketch_on_plane  { reference_id: "ref-plane-xy" }
add_sketch_rectangle   { start_x: 0, start_y: 0, end_x: 80, end_y: 60 }
add_sketch_circle      { center_x: 20, center_y: 30, radius: 10 }
add_sketch_circle      { center_x: 55, center_y: 30, radius: 10 }
finish_sketch
get_viewport_state     → sketch_profiles[].profile_id  (3 regions)
extrude_profile        { profile_id: ids[0], profile_ids: ids, depth: 10, mode: "new_body" }
get_viewport_state     → 1 body
reenter_sketch         { feature_id: <sketch feature id> }
delete_sketch_selection { entity_ids: ["circle-1"] }
get_viewport_state     → bodies 0, extrude dependency_broken   ← the defect
```

This belongs as a C++ suite (`tests/` alongside `sketch_profile_test.cpp`, asserting the region set with `profiles_match`), so the delete path can never regress to "silently drops the body" again.

---

## How to add an entry

Copy the D-001 shape: **Reported** (what the user did and saw), **Setup** (the exact construction, geometry numbers), **Expected / Actual / Status**, **Evidence** (command transcript, log lines, or "in-app only"), and **Hypothesis** clearly marked as unverified. Never promote a hypothesis to a finding without a reproduction — the point of this file is that it can be trusted.

## D-009 — A Horizontal constraint on a loose line is demoted to `driven` as "over-constrained" (fixed)

**Found:** 2026-10-24, while building S11's candidate detection. Its dedupe test applied a proposal the way the real setter does, then looked for it again — and found the constraint had been demoted instead of applied.

**Symptoms.** One loose line, 0.4 deg off horizontal, no other constraints, endpoints free. `set_sketch_line_constraint("line-1", "horizontal")` logged:

```
"constraint on line-1 marked driven (solver reported over-constrained)"
```

and stored the constraint `driven`, so it never drove geometry: a user who applies Horizontal to a slightly tilted line gets an inert constraint and the line stays tilted. A line has four DOF and one axis constraint removes one, so the sketch could not have been over-constrained.

**Root cause.** The demotion test in `sketch_axis_constraint_commands.inc` was `conflicting_count > 0 || redundant_count > 0 || solver_dofs < 0`. After `refresh_sketch_with_append_focus`, `solver_dofs` was still **-1** — its DEFAULT, meaning "the solver has not run" — so `-1 < 0` read as over-constrained. `finalize_new_dimension` in `dimensions.inc` documents exactly this trap and guards it with `const bool solver_ran = solver_dofs != -1;`; the axis path was missing that guard.

**Fix.** The same guard: -1 means "unknown, nothing to demote". A real conflict (conflicting or redundant counts, or a negative count from a failed solve) still demotes to `driven`.

**Test.** `tests/axis_constraint_driven_test.cpp` — Horizontal on a line 0.4 deg off straight: not demoted, and the line comes out horizontal (dy < 1e-6); Vertical likewise; re-applying the same axis constraint is not a new conflict. The first assertion failed before the fix and passes after.

**Separate observation, not fixed.** On this path the solver genuinely never runs (`solver_dofs` stays -1, conflicting/redundant stay -1): a lone line is straightened by the deterministic enforcement, not by planegcs. Defensible for one unconnected line, but the DOF badge has nothing to show for it, and every reader of `solver_dofs` must keep handling -1. S3 (one DOF source) is the right place to decide whether the solver should run here at all.

**Why it mattered beyond the bug.** S11's acceptance rule is "add a candidate, keep it only if the solver's DOF drops". With this defect, `try_add` would have rejected every good candidate and auto-constrain would have looked broken for an unrelated reason.

---

## D-010 — A rectangle was not constrained as a rectangle (fixed)

**Reported:** 2026-10-24, in-app: *"there are no constraints shown on the rectangle and the rectangle constraints were not correct in the first place. PolySmith does not follow the same philosophy as Fusion. It constrains the parallel of the sides instead of angle."*

### What the tool actually did

`add_sketch_rectangle` drew four axis-aligned lines and let `add_sketch_line` infer **H on both horizontal sides and V on both vertical sides**. Consequences:

- **Nothing said "90 degrees".** The shape was axis-*locked*, not angle-constrained. Move a side and the constraint keeps it horizontal rather than keeping the corner square.
- **Nothing said "these two are parallel".** Opposite sides were parallel only because both were axis-locked — an accident of the inference, not a stated relation.
- **No relations existed at all**, so `line_relations` was empty and the solver never ran for a rectangle-only sketch (`constraints_exist` gates on it). `solver_dofs` stayed `-1` and the DOF badge had nothing to show.
- **The viewport showed H and V badges only.** The relation-glyph path (`sketch_line_relation_badges_emit.inc`) was never reached, so a rectangle read as four disconnected axis statements.

### What Fusion 360 does (user screenshot + SolidCraft source)

Fusion shows an **H** on one side, **∥** on both opposite pairs and **⊥** at the corners — no **V** — and the set fully determines x, y, width, height. `solidcraft/crates/engine/src/cmd/sketch.rs` confirms the same family: its 2-point rectangle emits H×2 + V×2 (a different, axis-locked choice), its 3-point rectangle emits `Perpendicular(l0,l1) + Parallel(l0,l2) + Parallel(l1,l3)`. **Neither uses equal-length**, which is the wrong statement for a non-square rectangle.

### Fix

The rectangle now stores exactly the Fusion set, and nothing else:

```
horizontal(bottom)            ← one axis statement, so the shape keeps its orientation
parallel(top, bottom)
parallel(left, right)
perpendicular(bottom, right)  ← the 90° that makes it a rect-angle
```

Four independent constraints over eight coordinates ⇒ **dofs = 4** (x, y, width, height), no conflicts, no redundants. The inferred H/V on the other three sides is cleared. Relation ids follow the existing command scheme (`rel-<kind>-<a>-<b>`, ids sorted) so the delete path finds them. Glyph labels became `∥` and `⊥` (previously `//` and the letter `P`).

### Three defects it uncovered

**(a) Enforcing a relation rewrote geometry that already satisfied it.** `drive_line_parallel_to_reference` / `drive_line_perpendicular_to_reference` recomputed an endpoint from a normalised direction every time, even when the relation already held — an ulp of drift on a *shared* corner, on every recompute. They now return early when the relation holds within `1e-12` relative. This is what makes "refresh an exact rectangle" bit-identical (`rectangle_constraints_test`, `test_refresh_leaves_an_exact_rectangle_untouched`).

**(b) Driving one side of a closed profile rotated its neighbours instead of translating them.** `propagate_connected_point_move` carried the far endpoint only for H/V-constrained lines (a rigid translation along the axis). A relation-constrained line has the same property — its direction is pinned — but fell through to "move the shared corner only", leaving the neighbour diagonal. The relation pass then rotated it back *keeping the stretched length*: widening a 40×20 rectangle to 60 left the right side 28.28 mm long, and the solver reconciled that corrupted input by shrinking the height to **19.77 mm**. The fix generalises the existing rule: a line whose direction is pinned (H/V **or** a parallel/perpendicular relation) is translated by the perpendicular component of the corner's movement, so its length and angle both survive. Measured after: the drive produces an exact 60×20 rectangle.

**(c) Anchored points were only enforced *after* the solve.** A midpoint-anchored point is one of the solver's unknowns, so the solver expects it to already be on its host. An ad-hoc drive moves the host and leaves the anchored point behind; the solver then absorbs that residual by moving things nobody asked to move — measured: halving a rectangle's height moved the **whole rectangle up 5.76 mm**. `refresh_sketch_derived_state` now runs the anchor passes *before* `solver.build` as a consistency pass (the post-solve pass remains the authority). Same scenario after: exact 40×10 rectangle at y = 0, anchored line from `(20,0)` to `(20,10)`.

### Two false alarms removed

- **"sketch over-constrained" was logged for healthy sketches.** The condition was `conflicting > 0 || redundant > 0 || dofs < 0`, and a negative DOF count is not evidence of over-constraint: planegcs returns `-1` when it cannot diagnose the system at all (no unknowns / empty diagnosis matrix) and `params − nonredundant` otherwise. A red error line appeared for a plain rectangle with an anchored line, with `conflicting=0 redundant=0`. Only a reported conflicting/redundant constraint is an error now; a negative count with neither is a `debug` line ("could not count the remaining degrees of freedom").
- **Dimension edits were silently demoted to `driven` on such sketches.** `fallback_to_driven` in `dimension_update.inc` tested `solver_dofs < 0` (plus the D-009 `solver_ran` guard); it now tests the solver's own diagnosis (`conflicting_count > 0 || redundant_count > 0 || !solver_ok`), which is the signal that actually means "this new constraint cannot be satisfied".

### Pre-existing solver-diagnosis noise (NOT fixed here)

`diagnose()` on a sketch containing midpoint anchors returns implausible counts even with the *old* H/V rectangle: `0` where 7 is correct, `-4`, `-5`, and a different number again after one dimension change. Confirmed pre-existing by rebuilding the old constraint set in the same harness. It is a planegcs quality issue (redundancy analysis over ad-hoc anchor constraints), tracked with S3 — the badge never shows these because it only renders `>= 0`.

### Evidence

`native/cad-core/tests/rectangle_constraints_test.cpp` (new suite, registered in CMake) pins the contract: the constraint set and the shape it implies, `dofs == 4` with no conflict/redundancy, the bit-exact refresh, a width drive that keeps the rectangle rectangular, and an anchored line following a height drive. `pnpm test:core` — **87/87 suites pass**, including the three that encoded the old contract (`sketch_profile_test`'s two midpoint-anchor scenarios, `add_sketch_point_test`, and `auto_constrain_apply_test`, whose "axis-only sketch does not solve" guard now uses a single line instead of a rectangle — a rectangle legitimately runs the solver now).

### New diagnostic

`PS_TRACE_SKETCH_SOLVE=1` logs the sketch geometry before the solve, immediately after it, and at the end of the refresh (tag `sketch_trace`). This is what found (b) and (c): the ad-hoc enforcement and the solver both move geometry, and without the three snapshots it is impossible to tell which one made the rectangle jump.

### Follow-up report (same day): "before constraint is dof 4 and after is 2"

Applying a constraint to a rectangle side collapsed the DOF badge. Two more causes, both in the constraint commands, both fixed:

**(d) The append-focus solve published a FROZEN degree count.** `set_sketch_line_constraint` and the relation commands solve through `refresh_sketch_with_append_focus`, which pins every point outside the focus entity so a new constraint cannot drag unrelated geometry. The count that solve reports is the DOF of the *frozen* system, and it was published as the sketch's: a rectangle read **1** after any constraint command, where the sketch really has 4. `refresh_sketch_derived_state` now measures the unfrozen count with a throwaway solver after restoring the flags — it only reads a diagnosis and its solution is never applied, so the freeze still protects the geometry. Measured: applying an axis constraint now leaves the rectangle at 4, and a width dimension at 3.

**(e) Applying an axis constraint stored a duplicate the relations already implied.** Horizontal on the top side of a rectangle is implied by `horizontal(bottom) + parallel(top,bottom)`; planegcs flags the duplicate redundant and its redundancy repair is what produces the absurd counts (and it may move geometry). `set_sketch_line_constraint` now walks the relation graph — the axis propagates unchanged across `parallel` and swaps across `perpendicular`, stopping at the first line that declares an axis — and skips a constraint that is already implied, logging why. Same for the mirrored case below.

**(f) The parallel/perpendicular commands deleted the driven line's axis constraint unconditionally.** `line.constraint = std::nullopt` was justified as "an axis constraint is incompatible with a relation", which is only true when the relation has to *move* the line. Re-applying `perpendicular(bottom, right)` to a rectangle wiped the bottom side's H — a real constraint gone, and the count went *up* from 4 to 5. Both commands now clear the axis only when the relation does not already hold (`lines_are_parallel` / `lines_are_perpendicular`).

Tests: `rectangle_constraints_test` grew `test_applying_a_constraint_keeps_the_degree_count` (four cases: H on the top side, V on the right side, perpendicular on the square corner, parallel on the opposite sides — each must leave exactly one axis constraint, all three relations, DOF 4, no conflict/redundancy). `sketch_move_test` was updated to the new contract: a translate keeps the one stored H and the three relations rather than four axis constraints, because three of those four are now recognised as duplicates.

### Still open: the glyphs are not visible in the app

The data path is proven correct end to end — the core emits all four badges for a rectangle (`H` on the bottom, `∥` twice, `⊥` at a corner) with world positions, the zod schema accepts them, `createViewportScene` keeps all four with the right labels, and `buildSketchConstraintObject` returns a visible, textured, opaque sprite at the right position. What has not been observed is the last hop: the sprite inside the live Three scene. `ViewportPanel` now logs `sketch constraint badges: N [kind:label, …]` (level `info`, only when the set changes) so the next report can say whether the core sent them.

---

## D-011 — Constraint glyphs shipped HIDDEN by default, so no sketch ever showed its constraints (fixed)

**Reported** during the D-010 work, and it is the reason "there are no constraints shown on the rectangle" survived every attempt to fix the constraint *set*: *"I cannot see any constraints"*, *"during preview for the line I have all but after nothing. for the rectangle I have horizontal some time but also disappears after commit."*

**Root cause — a config default, not a renderer bug.** `apps/desktop-ui/src/config/config.json` shipped `viewport.showConstraints: false` ("they sit on top of dense sketch geometry and read as noise"), and the render loop enforces it per frame:

```ts
// ViewportPanel.tsx, every frame
for (const obj of sketchConstraintObjectsRef.current) {
  obj.visible = showConstraintsRef.current;
}
```

So the core emitted the badges, the schema accepted them, the scene built the sprites — and the renderer then set `visible = false` on all of them. Every install that had ever been saved carried `false` in `%APPDATA%\polysmith\config.json`, which is why it looked like a permanent regression: the data was always right, the display was switched off by preference. The one thing that *did* work was the HTML snap badge during a draft — hence "during preview I have all, after nothing". Anything that moved a glyph (a rebuild, a hover) looked like an intermittent bug.

The evidence chain that finally located it, all of it necessary:

| stage | how it was checked | result |
|---|---|---|
| core emits | headless rectangle → `sketch_constraints` | 4 badges, correct labels + world positions |
| IPC schema | `viewportStateSchema.safeParse(payload)` | accepted, 4 entries |
| scene data | `createViewportScene(payload)` | 4 constraints kept |
| object build | `buildSketchConstraintObject(...)` | visible sprite, `opacity 1`, textured, `depthTest false` |
| live render | per-frame `visible = showConstraints` | **`false` — the answer** |

**Fix.** `showConstraints` now defaults to **true** (a sketch that reports a degree-of-freedom badge while hiding the constraints it counts is unreadable; Fusion draws them too), and the config schema gained a version so the change reaches installs that already saved the old default:

- `config.json`: `configVersion: 2`, `showConstraints: true`.
- `appConfig.ts`: `migrateConfig()` runs before the merge — anything below v2 is treated as carrying the OLD default and gets `showConstraints: true`; a deliberate "off" saved afterwards keeps its value because the file then says `configVersion: 2`.
- The `ViewportPanel` render-loop comment now states the policy instead of the opposite.
- The viewport toolbar still has the "Show constraints" toggle for dense geometry.

**Tests.** New `apps/desktop-ui/src/config/appConfig.test.ts`: a pre-v2 config with `showConstraints: false` loads with glyphs ON; a v2 config with `false` keeps it OFF; a fresh install is ON; and the migration does not disturb the rest of the stored config (theme, units, crosshair).

**Lesson.** "The thing is not drawn" and "the thing is drawn but hidden by a preference" look identical from a bug report, and the second one survives every fix to the first. Before instrumenting a render path, grep the render loop for an override that can set `visible`/`opacity` from user config.

The diagnostics added while chasing this (`sketch constraint badges: N`, `constraint glyph sprites: …` in `ViewportPanel`) were removed once the cause was known; the env-gated core trace `PS_TRACE_SKETCH_SOLVE` stays.

---

## D-012 — The line draft's inference guide flickers and is almost invisible (fixed)

**Reported** right after D-011 was fixed: *"for the line preview: after first point of the line there is a dotted line with hints of verticality of projected other points. It is still there but fires rapidly and is almost invisible."*

**Two independent causes.**

1. **Rebuilt on every pointer event.** `renderDraftPointerPreview` opened with `clearPreviewInference()`, so every pointermove disposed the dashed guides and built new ones. A pointer stream arrives far more often than a frame (100+ Hz on a high-polling mouse), so the dash phase restarted continuously — the hint strobed instead of reading as a dotted line. The guides are now **kept across moves and updated in place** (positions written into the existing `BufferAttribute`, then `computeLineDistances()`); they are only disposed when the *number* of aligned vertices changes, or when the tool is not a line draft.

2. **Too faint, and z-fighting with the plane.** The material was `opacity: 0.42` with `depthTest` left at its default — and a guide lies exactly ON the sketch plane, so depth testing it against the plane/grid is a z-fight that wins or loses per frame. It is a hint overlay: now `opacity 0.9`, `dashSize 1.1 / gapSize 0.7` (coarser dashes read at typical sketch zoom), `depthTest: false`, `depthWrite: false`, `renderOrder 8` (sketch lines draw at 7). One material per batch instead of one per line, so the cleanup no longer disposes the same material once per guide.

**Tests.** `apps/desktop-ui/src/layout/viewport/inferenceGuideLines.test.ts`: one line per guide sharing one material; dashed with the `lineDistance` attribute present (without it dashes render solid) and opacity ≥ 0.8; `depthTest === false` with `renderOrder > 7`.

**Third cause, reported after the first two were fixed:** *"It fires only on mouse move and not visible if not move."* The guides were owned entirely by the pointer-move path — built there, and re-built only there. Anything that clears the preview objects (a pointer-up, a commit, a scene rebuild, a tool-state effect) removed them, and with the mouse still no new move arrived to put them back, so the hint existed only while the mouse was moving. They are now re-created from the render loop: `activeSketchPointerMove` records the last resolved alignment in `draftInferenceLinesRef`, and `render()` calls `restoreDraftInferenceGuides()`, which returns immediately when the lines are still in the sketch group and otherwise rebuilds them — only while the line tool has a live draft start, so a commit or a tool switch still clears them for good.

The lesson generalises to every preview overlay in the viewport: a scene object that only a pointer handler can recreate is one clear away from disappearing, and the symptom ("it only shows while I move") points at the lifecycle, not at the drawing.

---

## D-013 — "One dimension and the solver distorted the whole sketch" (guard added, exact repro still open)

**Reported:** *"The most worrying thing is that I made a dimension and the solver distorted completely the sketch. I fight with this a lot before and it is back now. Honestly it was solved by not allowing the solver to recalculate lots of things."*

**What was verified.** Two synthetic sketches were driven through the real core and compared **by vertex id** (comparing the vertex table by index is misleading — `rebuild_sketch_vertices` may emit the same vertices in a different order, which produced a false "the sketch was smashed" reading first time round):

| sketch | action | vertices moved | max displacement |
|---|---|---|---|
| rectangle + 2 loose lines | width dimension 40 → 60 | 1 (the rectangle's own corner) | 20 mm = the requested change |
| rectangle + 10 loose lines | width dimension 40 → 60 | 2 (both corners of the driven edge) | 20 mm = the requested change |

So on a fresh sketch the dimension drive is clean: the planegcs solve leaves loose geometry alone, and the ad-hoc drive translates the perpendicular sides (D-010 (b)). The user's distortion therefore needs their document or the exact command sequence (the report mentions the AI drafting a dimension) to reproduce; it is **not** reproduced by the two shapes above.

**Guard added meanwhile** (`solver_touch_guard.inc`, runs on every solve). A vertex may only move when it is **connected, through shared vertices, to an entity that carries a solver-registered constraint** (axis constraint, relation, driving dimension, constraint record, anchor or fixed point). Everything outside that closure has no say in the solve, so its post-solve value is noise: `restore_unreachable_solver_geometry()` puts the pre-solve values back and logs the largest restored displacement when it exceeds 1 µm. This is the mechanical version of the user's own diagnosis — "do not let the solver recalculate lots of things" — and it is inert on a healthy solve: all 87 C++ suites pass with it in.

**Mechanism found for the "random" part.** planegcs is not a black box, and the way we used it was unsound: `System::initSolution()` calls `diagnose(alg)` on every solve (that is the first thing it does after storing the reference), and `diagnose()` not only *analyses* redundancy — it **removes the redundant constraints from the system that is then solved** (`GCS.cpp` builds `clistR` without them, then solves that subset). Whatever we apply therefore satisfies a subset of what the sketch asks for; the dropped constraints are free to drift, and which ones get dropped depends on the redundancy analysis of the current state. On a rectangle with dimensions, relations and auto dimensions there is plenty to drop — hence "the deformations are random".

**Fix: the solution is now verified before it is trusted.** `solver_constraint_violation()` checks the solved geometry against every constraint we can evaluate — H/V, parallel / perpendicular / equal length, driving line length, line angle, circle radius and point distance — with real tolerances (1 µm, 1e-4 rad). If any is violated the solution is **discarded**: `restore_all_solver_geometry()` puts the pre-solve geometry back and the Logs panel gets `solver solution rejected (<constraint detail>); kept the geometry from before the solve`. The sketch then does not move rather than moving wrongly, and the next occurrence leaves the evidence needed to reproduce it. Verified inert on all 87 suites.

**Known limit.** A verifier can only check the kinds it knows; arcs, ellipses, slots, splines, tangency and anchors are not yet checked, so a violation confined to those still passes. Extending the verifier is the follow-on, and it is the cheaper half of the real fix (the expensive half is integrating planegcs properly: diagnose once, solve the reduced system, verify, and only then apply).

**Still to do:** reproduce the real case from the user's file and add the failing-then-passing test the regression rules require. Until then this entry stays open; the guards are a net, not the fix.

---

## D-014 — The inference guide showed an alignment the commit did not honour (fixed)

**Reported:** *"The snap did not make the line horizontal. I got the helper dotted line and made the end of the line assuming that the next segment will be on the same level, hence horizontal."* … and the requirement behind it: *"It should be a snap at the guide point. I need to be able to create a rectangle just by lines and the guide."*

**Cause — two halves, both needed.**

1. `snapResolution.ts` treated the alignment as a *visual* hint: it set `best = inferenceSnap` only when no axis-lock existed, and the later generic stages (`line-body`, `circle-body`, `arc-body`) could then replace it through `closerDynamicSnap`, which ranks purely by distance. A point 0.1 mm off a line body beat an alignment 0.9 mm away — so the guide was drawn and the point went elsewhere.
2. Even when the preview did take the alignment, `commitActiveSketchDraft` re-resolved the point with `inferenceSnapsEnabled: false` ("inference must not pull committed coordinates away from the user's intended click position"), so the commit silently used the raw click position.

**Fix — Fusion semantics.** An alignment guide is now a snap:

- At the end of the dynamic snap chain the guide wins over any *generic body* snap; it loses only to a snap that names a real feature (intersection, tangency, perpendicular/parallel host, axis lock). This is the priority Fusion uses, and it is what makes "draw a rectangle from four loose lines" possible.
- With a **horizontal and a vertical** guide both showing, the point snaps to their **intersection** (again Fusion's rule) — that is how a rectangle corner gets closed against two different reference vertices.
- The commit path resolves with inference enabled, so preview and commit agree: the dotted line is a promise the click keeps.

Alignment is still a *placement*, not an axis constraint: `snapAxisLock` stays `null`, and the H/V constraint comes from the core's ordinary inference on the committed geometry (the two points really are level, so the line is exactly horizontal).

**Tests.** `snapResolution.test.ts` gained an "alignment guides snap the point" block: a cursor 0.1 mm from a line body and 0.9 mm from a level lands on the level; and a cursor within reach of both a vertical and a horizontal reference lands on their intersection. Both fail on the previous ordering.

---






