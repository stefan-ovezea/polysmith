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

Relevant because the extrude edit panel's input *is* the profile selection (D-001e): a timeline click during a panel session silently empties its input. Left as recorded, not investigated — the selection-clearing is presumably deliberate (a timeline click is a selection action elsewhere too), and the profile re-pick is cheap. Worth revisiting when the panel's inputs are made independent of the global selection.

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

**Status:** Open — cause confirmed, fix chosen, not scheduled. Belongs with S19 (theme-token the sketch render path), which touches the same files.

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

## Why this matters for the plan

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
