# Active task: CAM TOOL TABLE — full implementation (2026-09-17)

> **Branch:** `cam/tools`, created from `dev` @ `d10cd3f` and pushed.
>
> **User request (verbatim):** "I want again a full implementation of
> the tool table for cam. make a study of the industry also. I want to
> have a default tool list from where to be able to select generic
> tools drils , mils etc. Also I want a propper edditor for the tools.
> A graphic floating window with a tool drawing and input windows for
> various parameter. Also I want to be able to import and export tools
> between linuxcnc and my program and verything you may think to add."
>
> **Working mode (user-approved):** autonomous full implementation
> with local checkpoint commits ("make local commits as you consider
> and if we need to debug we can get back commit by commit"); user
> available ~30 min at the start for plan questions.
>
> Research agents launched (2026-09-17, 3 parallel) — COMPLETE:
> 1. LinuxCNC tool table + FreeCAD/Fusion/ISO13399 formats
> 2. PolySmith CAM internals integration map (core + UI)
> 3. Tool-editor UX industry study (Fusion/FreeCAD/LinuxCNC)
>
> **User decisions (2026-09-17, AskUserQuestion):**
> - Library storage: DOCUMENT library + ON-DISK SHARED library
>   (POLYSMITH_TOOLS_DIR, clone machine_library pattern)
> - Machine scope: MILL + LASER + LATHE data (I/J/Q + angles as data)
> - Posts: EMIT T/M6 + spindle now (GRBL/FluidNC must NOT — gate by post)
> - Formats: PolySmith JSON + LinuxCNC .tbl (no CSV this round)
>
> **Key findings:** ToolEntry/tool_library/cam_tool_* CRUD exist
> end-to-end already; generators consume tool diameter + default
> feeds. Missing: tool numbers, on-disk library, editor UI, .tbl I/O,
> T/M6 post emission, tool-edit invalidation (precedent:
> cam_tool_delete), new types (v_bit/spot_drill), fields (flutes,
> helix/point/tip/taper angles, shoulder/length-below-holder,
> surface-speed/fz, description/vendor/product_id, pocket, guid,
> lathe front/back/orientation).
>
> **Checkpoint commits (each gated: build + suites + tsc):**
> C1 core ToolEntry schema + whitelist + payload/parse + save/load test
> C2 core tool_library.{h,cpp} on-disk lib + POLYSMITH_TOOLS_DIR +
>   cam_tool_library_list/save + generic catalog seeds + test suite
> C3 core .tbl + JSON parse/export text commands + round-trip tests
> C4 core tool-number auto-assign + uniqueness + update invalidation
> C5 core post T/M6 + spindle emission (linuxcnc yes, grbl no) + tests
> C6 TS types/zod/ipc + hooks for new fields/commands
> C7 TS lib: toolSchematic.ts, toolOptions.ts, camToolSelection
>   (replaces 16 scattered type-filter predicates; engrave accepts
>   V-bits, drilling accepts spot drills)
> C8 CamToolEditorPanel (floating, schematic SVG + fields + validation)
>   + --color-warning token (6 themes) + cad-panel-item CSS classes
> C9 CamToolLibraryDialog (3-pane, search/sort, duplicate/copy/delete,
>   editor entry, sidebar Tool Library row)
> C10 import/export (.tbl/JSON): core cam_tool_parse_file /
>   cam_tool_import_file (renumber|overwrite|skip, one undo step) /
>   cam_tool_export_file + UI conflict dialog + wiki docs
> C11 fix: save_tool_entry mints guid when absent
> Final: full gates + tracker + user verification checklist

## STATUS 2026-09-17: FULL IMPLEMENTATION COMPLETE — all local commits on `cam/tools`, gates green, AWAITING USER IN-APP VERIFICATION

**Commits:** df2606b (C1) → edff543 (C2) → ad606be (C3) →
7448396 (C5) → c38216e (C6) → 8af0688 (C7) → 685b14e (C8) →
f415a65 (C9) → 9f04f49 (C10) → 443e4b6 (C11).
NOT pushed (user pushes when asked).

**Gates run:** `pnpm core:build` clean + **54/54 C++ suites pass**
(`pnpm test:core`; new: tool_library 5, tool_table_io 8, import-batch
3b in cam_commands, linuxcnc-post tool-change emission) +
`tsc --noEmit` clean + `cargo check` clean.

**User verification checklist (in-app, `pnpm dev`):**
1. CAM workspace → sidebar **Tool Library** row → dialog opens; both
   Document and Shared Library tabs list tools; shared = 42 seeded
   generic tools (T1–T42).
2. Double-click a tool → floating editor with the schematic; change
   the diameter and watch the drawing rescale; validation errors block
   Save (e.g. clear the name).
3. New Tool → name it, Save → appears in the document library with an
   auto-assigned number.
4. In a mill setup: create an operation with two different tools and
   export G-code — the second operation must emit `T<n> M6` + `G43
   H<n>` (linuxcnc/mach3/mach4/fanuc posts only). NOTE: if the user's
   on-disk `posts/linuxcnc.json` predates this change it shadows the
   seeded post — delete or re-import it to see the tool-change lines.
5. Import: LinuxCNC `tool.tbl` from a real machine → preview lists
   dispositions; switch the conflict mode and watch the labels change;
   Import → Logs panel shows "added X, skipped Y, replaced Z".
6. Export → `tools.tbl` → re-import it in another document → tools
   reconcile by guid (renamed tools come back as replacements).

**Deferred (noted, not blocking):** Fusion-style parameter presets,
tool holders/assemblies, CSV format, per-shared-tool delete command
(dialog hides Delete in the shared tab; save overwrites the file
instead), lathe post/generator work (data + table fields only).

---

# COMPLETED: UNDO/REDO REWORK — merged as PR #87 (2026-09-17)

> **Branch:** `feature/undo-redo`, created from `dev` @ `01c5149`
> (the squash merge of the trim redesign) and pushed to origin.
>
> **User request (verbatim):** "undo and redo is working sporadic.
> this tools was also done very long time ago and is behind not
> knowing about many new tools and stuff. I want again in deep
> analysis and a good plan to bring it to general accepted cad
> standard. right now has a mind of its own and some times work some
> times does not. ... i want a full implementation. Probably need to
> be spitted between 2d and 3d implementation."
>
> **User decisions (approved via AskUserQuestion):** sketch undo
> model = **Session = one step** (Onshape model — per-action undo
> inside the open sketch; after exit the session collapses to ONE
> step; cancel rolls back the session with no trace); rollout =
> **All phases, one go** (implement P0→P5 now, staged test program,
> user verifies the full result in-app at the end).
>
> **Research (2026-09-17, 4 agents) — COMPLETE:** (1) core audit:
> 138 push sites / 56 files; undo swaps snapshots with NO refresh
> (revision goes backwards, CAM cache ambiguity); extrude pushes per
> tick ×N features; construction-plane previews clear redo with no
> push; `remove_sketch_projections`/DXF-import conditional entries.
> (2) UI: stale `can_undo`/`can_redo` (session_state only on
> request), fire-and-forget undo, cancel loops up to 10 undos,
> double-Escape, per-keystroke steps, no Ctrl+Y. (3) Standard:
> one-intent-one-step, named grouped transactions, previews never
> push/clear-redo, sketch session enter/exit/cancel semantics,
> session-scoped history + limits, undo-recomputes + toolpath
> invalidation. (4) History: machinery born 2026-04-16 root commit,
> only 1 real reported bug (round 25), 8/51 suites cover undo.
>
> **Spec:** `wiki/Undo-Redo-Redesign-Requirements.md` — decisions
> D1–D10, phases P0→P5, stage test program
> `cad_core_undo_stages_test`.
>
> ## MERGED as PR #87 → dev @ d10cd3f (2026-09-17). New branch `cam/tools` created from dev (pushed).
>
> ## STATUS 2026-09-17: ALL SIX PHASES IMPLEMENTED — gates green, AWAITING USER IN-APP VERIFICATION
>
> **Gates run:** core rebuild clean + **52/52 C++ suites pass**
> (52nd = `cad_core_undo_stages_test`, 9 stages: empty-stack
> contract, refresh pipeline, extrude session, plane-preview redo,
> projection grouping, scrub, session semantics, named steps + limit
> + undo_many, CAM invalidation) + tsc clean. NOTHING committed —
> user tests first (no untested commits).
>
> **What landed (P0–P5):**
> - Core: full-refresh undo/redo, EMPTY_UNDO_STACK/EMPTY_REDO_STACK
>   error codes, monotonic revision, preview sessions never push/clear
>   redo, timeline scrub + rename no-ops, CAM generate undoable +
>   toolpath invalidation on every restore.
> - Sketch sessions: enter opens a group (begin AFTER the opening
>   bump); per-action undo inside; finish = ONE named step; NEW
>   `undo_abort_all_groups` command + toolbar **Cancel Sketch** button
>   (confirm dialog) = roll back with no trace.
> - Groups/history: `undo_begin_group`/`undo_end_group`/
>   `undo_abort_group`/`undo_many`/`set_undo_limit` (default 30),
>   named steps via `std::source_location`, Edit menu `Undo <step>` +
>   Undo History dropdown with multi-step confirm, dimension drafts +
>   projection removes grouped.
> - UI: can_undo/can_redo + step names on every document_state, awaited
>   correlated undo, Ctrl+Y, global Ctrl+Z in focused inputs, ONE
>   Escape = one cancel (double-Escape fixed), extrude cancel = single
>   undo, extrude panel closes when its feature leaves the history,
>   revision-pinned pointer-up commits (M24), empty-stack no toast.
> - Docs: IPC-Protocol, AI-CAD-Command-Language, Glossary,
>   Implementation-Log, V1-Roadmap all updated.
>
> **In-app verification checklist (user):**
> 1. Sketch: draw several entities → Ctrl+Z steps back ONE entity at a
>    time inside the sketch → Finish Sketch → the whole session is ONE
>    undo step in the Edit menu (named "Sketch Edit") → Ctrl+Z removes
>    it all at once; Ctrl+Y restores it.
> 2. Cancel Sketch button (toolbar, next to Finish): confirm → the
>    sketch returns to its pre-edit state; NO trace in the history.
> 3. Re-enter a sketch and edit: each edit inside = one step; finish
>    collapses again.
> 4. Extrude panel: change depth/mode (live preview) → press Escape —
>    ONE undo step removes the whole preview; redo branch survives
>    preview edits (undo something before the extrude, change a preview
>    setting, redo still available).
> 5. Ctrl+Z with the cursor inside a numeric input (e.g. dimension
>    field) still undoes the document; Ctrl+Shift+Z / Ctrl+Y both redo.
> 6. Empty history: Ctrl+Z at the start — no error toast (silent
>    no-op), button disabled.
> 7. CAM: generate a toolpath → undo → toolpath disappears; redo →
>    toolpath returns (or shows needs-regenerate).
> 8. Edit menu → Undo History: named entries, multi-step undo asks for
>    confirmation.
>
> ## FREEZE-HUNT ROUND (2026-09-17) — dropdown freeze + dimension-persistence race FIXED, user-verified
>
> Reported as: line/circle tools kept auto-dimensions even without
> editing (draft-group sequencing race), then the app froze — user
> pinpointed: **opening the Edit/undo dropdown froze everything** (X
> dead, Ctrl+C from console only). Log showed repeated react_warning
> "Encountered two children with the same key... Delete Sketch
> Dimension" at the 2s heartbeat cadence.
>
> Root causes:
> 1. **Dimension persistence race (bug 1):** the combined post-commit
>    effect consumed `draftUndoGroupOpenRef` on the begin_group's own
>    document reply BEFORE `scheduleDimensionDeletion` ran (scheduled
>    after the await) → auto-dim deletion never fired → dims stayed.
>    Fix: drains run unconditionally; the flag is set after the
>    pending work is scheduled; the group closes only when BOTH
>    pending refs are drained.
> 2. **Dropdown freeze:** `MenuDropdown` keyed items by `item.label`
>    (AppHeader.tsx) and the undo history legitimately held duplicate
>    names — the CLICK-commit path (commitDraftPointerUp) deleted
>    auto-dims OUTSIDE the "Dimension" undo group (only the drag path
>    opened it), leaving one raw "Delete Sketch Dimension" step per
>    untyped click-drawn line. Duplicate React keys corrupted the
>    menu DOM; every heartbeat store update re-reconciled the broken
>    tree until the JS thread wedged. Fixes: unique keys
>    `key={`${index}-${item.label}`}`; new
>    `scheduleDimensionDeletionInGroup` wrapper in ViewportPanel —
>    the click path now opens the "Dimension" group idempotently
>    BEFORE the add command (every click commit schedules the
>    deletion ahead of the add), so click-drawn entities + their dim
>    cleanup collapse into ONE step like drags.
> 3. Also hardened during the hunt: trim/corner preview flood
>    (one-in-flight cap + pending-newest + 10s stuck-slot safety +
>    `notifyTrimPreviewResponse`/`notifyCornerTrimPreviewResponse`
>    from ViewportPanel on every response); save (async toBlob
>    thumbnail, one-save-at-a-time guard, progress messages). STEP
>    slowness explained (projection live-link refresh on every bump —
>    Unlink projections is the workflow fix; caching refresh is a
>    possible future perf round).
>
> TEMPORARY DIAGNOSTICS REMOVED: ui_heartbeat, keydown/sketch_keydown
> logs, draft_commit logs, scratch_trim_crash_repro.cpp + CMake entry.
> Gates re-run after removal: core rebuild clean + **52/52 suites** +
> tsc clean.
>
> **User in-app verification (2026-09-17):** "I have stressed enough
> and did not freeze. The log is clean" (the single "drained + end
> group" line was the temporary diagnostic, now removed).
>
> ## POST-COMMIT REGRESSION (2026-09-17, uncommitted) — dimension drain mis-attribution FIXED, awaiting user verification
>
> User after the commit: "first line i draw has dimmensions and the I
> draw another line. the first line loses the dimension but the new
> line has now dimensions." Root cause: the click-path group wrapper
> made the post-commit effect fire on the begin_group's own reply,
> and the deletion drain targeted "the last entity" + cleared the
> pending ref unconditionally — on the begin reply the previous line
> was still the last one, so ITS dims were deleted and the ref was
> consumed before the new line landed (new line keeps its dims; the
> group also closed early).
> Fix: `PendingDimensionDeletion` now carries `tool` + pre-add
> fromLineCount/fromCircleCount/fromPolygonCount (captured at
> schedule time); `deletePendingAutoDimensions` gates on
> `pendingEntityLanded` and keeps the pending ref until the entity
> lands — same pattern the expression drain already used. tsc clean.
> The earlier fix (unique dropdown keys) stays the freeze fix; the
> wrapper race was an independent regression it exposed.
>
> Previous task (trim redesign, merged as PR #86) — section below.

---

# COMPLETED: TRIM TOOL REDESIGN — MERGED as PR #86 (2026-09-17)

> **Branch:** `feature/trim` (from `dev` @ b32f9fb). **Committed
> `a54c6eb`, pushed, squash-merged as PR #86 → `dev` @ `01c5149`
> after user in-app verification ("OK it is working", 2026-09-17).
> Branch deleted (local + remote).**
> **Spec:** wiki/Trim-Tool-Redesign-Requirements.md (user approved
> D1–D5: FreeCAD-style constraint transfer, extend-to-intersection,
> no right-click cycling, circle keeps concentric/equal, rebuild in
> place). Research: 4 agents (industry semantics, engine deep-dive,
> architecture constraints, user history) — reports summarized in the
> spec.
>
> **Phases:** 0 engine rebuild (no behavior change) -> 1 constraint
> transfer + projection pruning -> 2 Corner/Extend/Split -> 3 drag-paint.
> Each phase gated: build + suites + tsc. **FINAL GATES 2026-09-17:
> 51/51 C++ suites + tsc clean** (51st = the new stage test) + user
> in-app verification.
>
> **Phase 0 progress (2026-09-17, all 50 suites green after each step):**
> - P0.1 DONE: freeze minted split points BEFORE the refresh solve
>   (sketch_trim_commands.inc pins them by scanning entity endpoints;
>   the old code froze after — the trim's own solve ran unfrozen).
> - P0.2 DONE: one cleanup pipeline (new trim_attachment_cleanup.inc,
>   trim_remove_entity_attachments) replaces the five divergent
>   erasures — isolated-delete and single-survivor paths no longer
>   leak relations/anchors/fillets/chamfers; the old line-only
>   trim_line_relation_cleanup.inc is deleted.
> - Line middle-split iterator invalidation fixed (re-locate after
>   push_back); spline trim re-fits into temporaries and commits only
>   when every span succeeds (no more throw-after-undo-push).
> - P0.3 VERIFIED-AS-EXISTING: the splitters already collapse degenerate
  pieces at source (line drops < kTrimCoincidentTolerance, circle
  returns nothing for tangent touches, kMinArcSpan) — command-level
  points_match guards stay as backstops; the global zero-length
  cleanup stays as the non-trim safety net (per spec).
- P0.4 SCOPED DOWN: quadrant ids stay legacy for now (the vertex-N
  migration is a derived-geometry modernization, not needed for the
  trim redesign); the legacy hardcodes are consolidated into ONE
  place (live_sketch_point_ids in trim_attachment_cleanup.inc) —
  the three ad-hoc safety nets (circle/arc/ellipse phases) and the
  line orphan net now share it.
- P0.6 DONE: preview payload symmetry — every kind now emits
  param_start/param_end + start/end per segment; a line with no
  intersections paints the whole line red (hovered_index 0, one
  [0..1] segment) instead of the old null payload (industry: the
  click deletes the whole entity, so the preview shows it). Schema
  already accepted all fields as optional — emission-only change.
- P0.7 DONE: intersection-dispatch unification — new
  trim_intersection_traversal.inc (for_each_trim_candidate,
  append_shared_curve_pairs, finish_trim_intersections); the five
  find_all_intersections overloads now keep only their per-kind
  analytic fast paths + endpoint-touch passes and share one
  traversal/sort/dedup tail.
- GHOST-CIRCLE BUG FIXED (2026-09-17, the user's vision report —
  "trimming one of two arcs between two crossing lines paints the
  whole circle red and the trim silently does nothing; the walk
  direction is wrong, it deletes the part of the arc that does not
  exist"). Root cause, traced with the new endpoint-only regression
  tests:
  1. split_arc_at_intersections' wrap fixups used `<=`/`>=`: an
     intersection sitting EXACTLY on the arc's own endpoint was
     shifted a full 2π, manufacturing ghost FULL-CIRCLE segments
     beside the real piece; the click landed in the ghost and the
     trim wrote the endpoint back onto itself — a silent no-op.
  2. arc_angles() returns both sweep ends WRAPPED — the end-unwrap
     + snap/clamp logic must compare against the UNWRAPPED end or
     interior angles collapse onto the wrapped end (one-piece
     result → whole arc deleted; killed the six-petal workflow).
  3. angular_gap's min(d, 2π-d) went NEGATIVE for d > 2π (angles on
     opposite unwrapped axes) and snapped everything.
  Fixes (arc splitter + partial/full-ellipse splitter + circle
  splitter): boundary-coincident intersections SNAP to the sweep
  edges (tol = kTrimCoincidentTolerance/r), wrap fixups are strict,
  the sweep end is unwrapped before comparing, angular_gap is
  fmod-normalized, and the degenerate-piece drop is DIMENSIONAL
  (span × r < kTrimCoincidentTolerance — the old 1e-9-rad floor
  kept ~1e-4-rad trig-roundtrip stubs). Arc/ellipse entity commands
  also gained a segments.size()==1 full-delete guard (the old code
  read segments[1] on a size-1 vector — UB). UI:
  trimPreviewHighlight.sampleFullCurve paints the arc's/partial
  ellipse's OWN sweep instead of 0..2π.
  Tests added: test_arc_with_endpoint_only_intersections_deletes_
  cleanly + test_partial_ellipse_with_endpoint_only_intersections_
  deletes_cleanly (both fail-before/pass-after). Gates: 50/50
  suites + tsc clean. AWAITING user in-app re-verification of the
  circle/2-lines scenario (line trim already confirmed by the user).

PHASE 0 COMPLETE — all suites + tsc green after every step. The
app-layer preview change (P0.6: whole-line red for no intersections)
and the ghost-circle fix were verified in-app by the user
("looks like is fix now").

## PHASES 1-3 COMPLETE (2026-09-17, uncommitted, awaiting in-app test)

**Phase 1 — constraint transfer + projection pruning (D1/D4):**
`capture_trim_constraint_transfer` before cleanup +
`apply_trim_constraint_transfer` after — H/V badge, whitelisted
relations (parallel/perpendicular/equal_length, deduped via
relation_exists), concentric re-target, driven-dim re-derivation
(`dim-trim-<kind>-<id>`, circle_radius→arc_radius remap),
point_line_anchor containment check, coincident re-application.

**COINCIDENT-RECORD BUG FOUND + FIXED (real Phase-1 defect, caught
by the stage test):** coincident records carry LINE ids in
`target_ids` and the merged POINT id in the constraint id
(`"constraint-coincident-<point_id>"`) — the transfer's position
lookup and every orphan sweep compared line ids against the live
POINT set, erasing every coincident constraint after any trim. New
helpers in trim_constraint_transfer.inc: `coincident_point_id`,
`line_references_point`, `coincident_record_alive`,
`sweep_orphan_coincident_constraints` (applied at transfer step 6 +
all six sweep sites: line/arc/circle/ellipse entity commands,
corner, split, stroke).

**Phase 2 — Corner trim (R2), Extend, Split (R4):**
- `corner_trim_commands.inc` — analytic virtual-corner math for
  line/line, line/circle, circle/circle (virtual corners are
  unbounded — extend's finite-segment helpers are the wrong domain);
  nearest candidate to click; new arc sweep joins the surviving
  pieces; A applied first so B adopts A's corner id; arc_angle dims
  flip driven; throws on collapse/inversion.
- Extend already existed — got hotkey (E) + help parity.
- `split_entity_commands.inc` — line/arc/partial-ellipse split at
  ALL intersections (boundary ids resolved first; piece 0 keeps the
  original id); circle/full-ellipse two-click → two CCW pieces
  sharing endpoints; spline re-fit into temporaries first (undo
  safety); transfer drops equal_length on splits; freeze-before-
  refresh.

**Phase 3 — drag-paint trim (R5):** `trim_stroke_commands.inc` —
ONE push_undo_state for the whole stroke; per-entry try/catch
(log_warn + skip); shared minted-vertex freeze; one refresh, one
bump. UI accumulates hovered segments while dragging and commits
the stroke on pointer-up.

**Stage test program (the requested isolation harness):**
`native/cad-core/tests/trim_stages_test.cpp` →
`cad_core_trim_stages_test` (`--stage N`, `--list`; auto-discovered
by scripts/run-core-tests.mjs — the 51st suite). 8 stages:
1 Phase-0 baseline trims, 2 D1 transfer (H badge, relations,
coincident record survival, equal_length partner re-solve, driven
dims), 3 projection pruning, 4 stroke on hand-built entities,
5 corner line-line, 6 extend line-arc, 7 split (line + circle
pieces share vertices), 8 stroke + ONE undo restores everything.
Run directly:
`PATH="third_party/occt8-install/win64/vc14/bin:$PATH" CSF_OCCTResourcePath=third_party/occt8/src build/cad_core_trim_stages_test --stage N`

**UI wiring (live, tsc clean):** Corner Trim button + hover ghost
(core preview → `polysmith-corner-trim-preview` event, orange ghost
group with sampled segments + corner cross), Split button, drag-
paint stroke on Trim, extend hotkey E + help entries for all four
tools; zod schema validation; corner/split IPC command builders;
i18n labels (en.json).

**Docs:** Glossary (Trim entry: click-to-delete wording, companion
tools, D1 link), V1-Roadmap (D1 transfer whitelist wording),
AI-CAD-Command-Language + IPC-Protocol (corner_trim_preview /
corner_trim_sketch_entities / split_sketch_entity /
trim_sketch_stroke).

## USER-VERIFIED IN-APP (2026-09-17) — "OK it is working"

1. Trim: click-to-delete AND drag-paint stroke; a stroke must undo
   as ONE step.
2. Corner Trim: pick A then B → hover ghost shows the virtual
   corner; click commits the corner (arc join).
3. Extend: E hotkey; stretch to the first intersection.
4. Split: one click splits at every intersection; circles/ellipses
   use the two-click rule.
5. Hover an isolated line with Trim → whole line red (P0.6).
6. Trim near coincident/constrained geometry → coincident
   constraints must survive (the fixed bug).

Merged as PR #86 (squash) → `dev` @ `01c5149`; `feature/trim` deleted
(local + remote). `projects/laser board/` was deliberately left
untracked (user data, not part of this change).

---

# (previous task — historical) sketch selection fixes + projection heal — COMMITTED (5f19fed); rounds 9-11 uncommitted: sketch-plane redefine, stray-point heal, broken-sketch extrude guard, screen-size sketch points, log filter, trim-log-flood fix, constraint-glyph toggle (2026-09-14)

> **Branch:** `fix/sketch` (from `dev` @ 69aa509)
> **Committed locally as `5f19fed` after user in-app verification
> ("It is working", "OK it is working") — no Co-Authored-By trailer.
> PR `fix/sketch` → `dev` (squash) + branch cleanup await approval.**
>
> **Rounds 9-11 (uncommitted, awaiting user verification):** round 9 —
> stray projected-point heal + `redefine_sketch_plane` + timeline
> sketch right-click menu + body-vertex-dot gating; round 10 — broken
> sketch extrude now REFUSES instead of silently producing no body,
> sketch points are constant screen size, Logs panel hides debug by
> default; round 11 — trim log flood removed (sketch sluggishness),
> constraint glyphs hidden by default with a toggle. See the Round
> 9-11 sections below.

## Round 9 — stray projected points, sketch-plane redefine, timeline menu (2026-09-14)

User: "does not generate correct surfaces... should be free of any
link or projection but the sketch is flagged", "stray projection as
vertex multiplied after array generation", "all the dots are
highlighted... very big", "I need a right click on the sketch to
allow me to choose another plane", "remove/unlink should also be
right click on the sketch (timeline)".

Findings:
1. The sketch flag was CORRECT — the user deleted the mesh_to_body
   feature (feature-15); the sketch's plane (`feature-15:face:0`) was
   gone. Kept as-is (user confirmed the error should stay); the
   recovery is the new redefine command.
2. The 36 stray purple "projected" vertices were `projected_points`
   entries left behind by "Unlink projections": the heal cleared the
   records but never the standalone-point list, and
   `rebuild_sketch_vertices` re-mints every entry as a
   kind-"projected" vertex on each bump (re-flagging is_projected).
   The array-copied arcs just happened to share positions with them.
3. "Extremely big dots": body vertex dots (round 6) draw with
   depthTest off at 4-5 px screen size — over a faceted fan they
   flood the sketch view.

Fixes:
- Core `remove_sketch_projections`: BOTH modes now sweep
  projection-derived `projected_points` (non-`dxf:` source_id; DXF
  points stay). Remove mode also sweeps orphans whose records were
  already gone.
- Core NEW command `redefine_sketch_plane { feature_id, plane_id }`:
  re-parents an existing sketch onto an origin plane / construction
  plane / body face; geometry keeps sketch-local coords; clears the
  alarm; walker re-validates on the next bump. Unresolvable planes
  rejected. Construction planes must be UPSTREAM in the timeline
  (parametric ordering — pinned by test).
- Core behaviour PINNED: body → construction plane → sketch, then
  deleting the body detaches the plane (frozen frame, NO alarm) and
  the sketch on it stays healthy.
- UI: DocumentHierarchyPanel sketch context menu — "Redefine sketch
  plane" submenu (XY/XZ/YZ + construction planes + "Pick a face…"
  which arms a viewport face pick, Esc cancels) + "Remove
  projections" / "Unlink projections" (shown when records OR stray
  points exist). IPC type/builder/hook wired through
  AppSidebar → App.
- UI: body vertex dots hidden while a sketch is active unless the
  Project tool is armed; 5/4 px → 4/3 px.

Tests (face_projection_arc_test): heal tests now assert
projected_points emptiness (fail-before: unlink leaves them);
`test_redefine_sketch_plane_reparents_and_clears_alarm`;
`test_construction_plane_shields_sketch_from_body_deletion`.

Gates: full rebuild + 46/46 C++ suites + tsc clean. Awaiting
in-app verification, then a second local commit.

## Round 10 — "no surface" + huge dots + 500 logs (2026-09-14)

User: "trim looks like worked well... just not generating a full
surface"; "re open the sketch and all the dots are huge again"; "I
have 500 of this info". Root causes (diagnosed with a scratch test on
the saved part, then removed):

1. **No surface** — `extrude_profiles` accepted a broken-sketch
   profile, but the compiler skips flagged features
   (`compile_bodies_modifier_replay.inc`), so the extrude silently
   compiled to 0 bodies. The user's file was also stale (plane
   `feature-15:face:0` broken, 36 stray projected_points — pre-round-9
   state; re-saving after the round-9 heal fixes it).
2. **Huge dots** — sketch point spheres are WORLD-sized (r 0.7-0.9),
   fine at far zoom, balloons at detail zoom. Body vertex dots (round
   6) were already screen-sized; sketch points were not.
3. **500 logs** — trim's "coincident circles" debug spam, one entry
   per pair per pointer move, with no level filter in the Logs panel.
4. **Blue stray dots** — arc/circle CENTER marks (kind "center",
   axis-blue by design) — normal, not projection leftovers.

Fixes:
- Core `extrude_profiles`: refuses a broken sketch with
  `"The sketch is flagged as broken — <warning> Fix the sketch before
  extruding."` (guard lives at the top, before any profile work).
- UI `ViewportPanel` render loop: sketch point meshes now scale to a
  constant SCREEN size each frame (3 px radius, 4 px hovered/selected
  — same as body vertex dots), normalized by each sphere's geometry
  radius. `paintSketchPointMaterials` no longer owns scale.
- UI `LogsWindow`: debug entries hidden by default; "Show debug" /
  "Hide debug" toggle in the header (i18n `logs.showDebug` /
  `logs.hideDebug`).
- Scratch `tests/scratch_partstefan_diag.cpp` + CMake entry REMOVED;
  stray 0-byte `src/app/app.cpp` (accidental redirect artifact) also
  removed.

Tests: `test_extrude_refused_on_broken_sketch`
(face_projection_arc_test) — box → sketch on top face → delete body
(flagged) → extrude must throw with the warning; suite runs 10/10.

Gates: full rebuild + **46/46 C++ suites** + tsc clean. Awaiting
in-app verification.

## Round 11 — sketch sluggish + FIX glyphs (2026-09-14)

User: "everything is so sluggish that is almost not usable. I cannot
make a line without waiting few seconds... I had 8K file with
thousands of lines and it was handled without delay" + "the FIX
constraint is stupidly random and makes no sense. I do not want to
see it in the screen."

1. **Sluggishness — root cause FOUND and FIXED (core).** Scratch
   perf test on the live file (projects/part-stefan.json): core
   add_sketch_line = **8.2 ms/line** — the core was never the
   bottleneck. The run emitted **15,972 structured log lines** for
   ~20 commands (~800/command): `intersect_circle_circle` in
   `trim_line_circle_intersections.inc` log_debug'd every pair of
   coincident SUPPORT circles, and the PROFILE face walk
   (`sketch_profile_exact.inc:313` → `sketch_curve_intersections`)
   calls it for every curve pair on EVERY geometry bump. The user's
   108 arrayed arcs are all concentric → ~800 debug lines per
   command → ~800 UI events + store updates per command → UI thread
   jank ("line takes seconds to appear"). Fix: the log is REMOVED
   (the NaN/UB guard stays; silent by design — coincident circles
   are routine). Verified: 0 log lines, 5.3 ms/line (logging itself
   cost ~3 ms/line).
   - UI store already bounded: consecutive-duplicate collapse + 500
     cap in cadCoreStore.addLogEntry — no store change needed.

2. **Constraint glyphs (Fix/H/V) hidden by default (UI).**
   - Config: `viewport.showConstraints` (types.ts + config.json
     default **false** + appConfig merge guard).
   - ViewportPanel render loop sets
     `obj.visible = showConstraintsRef.current` on every
     sketchConstraintObjects sprite each frame (covers rebuilt
     sprites); ref synced from config like showViewportGrid.
   - Bottom mini-toolbar: `GridToggleToolbar` gains an optional
     secondToggle (sketch mode only) — "Show/Hide constraints" with
     a new `ConstraintsMiniIcon` (ViewportOverlays.tsx); threaded
     through ViewportPanelShell → ViewportPanel
     (`onToggleConstraints`). i18n `viewport.showConstraints` /
     `viewport.hideConstraints`.

3. `projects/` directory staged for tracking (user-requested
   "track the projects/").

Gates: full rebuild + **46/46 C++ suites** + tsc clean. Awaiting
in-app verification (draw lines fast again; no FIX glyphs; the
bottom mini-toolbar shows a constraints toggle in sketch mode).

## Round 12 — 24 spoke arcs vanished from the wheel (2026-09-14)

User: "many geometries are missing... they are missing from sketch
now" (CAD screenshot with toolpaths). Timeline pinned by file
timestamps: screenshot 1 (wheel GOOD, 130 arcs) at 20:08; save at
20:57 (damaged, 108 arcs) — BEFORE the round-11 FIX build. User's
account: only drew test lines/arcs far from the wheel between the
two.

Findings (evidence-based):
1. **The 24 missing arcs are all in the wheel's top-left quadrant**
   (spoke arcs, r=67.7/69.2, start angles -60..-180) + 2 test arcs
   were added — clean diff between res/part-stefan.json (130 arcs)
   and projects/part-stefan.json (108 arcs).
2. **Clean deletion signature**: both files have 0 dangling vertex
   refs; the arcs + their 35 vertices were removed together — the
   Delete/marquee path or repeated no-intersection trims, NOT a
   data-corruption cascade (corruption would leave dangling refs).
3. **Repro attempt**: load res/ + add lines/arcs (the user's
   sequence) → arc count grows 130→132, survives save+reload. The
   adds do NOT delete spokes. The damage is not an add-entity bug.
4. The deletion happened during the round-11 log-flood lag (~800
   log events per command, seconds of UI stall) — a marquee+Delete
   could execute with the selection rectangle never rendered
   ("they just disappeared"). The flood is now fixed at the source,
   removing the condition.

Actions:
- **Geometry RESTORED**: scratch tool re-added the 24 arcs via
  `add_sketch_arc(..., "center_start_end")` (three_point
  circumcenter is unstable on nearly-collinear triples — 5/24
  mismatched at 0.5mm; center_start_end restored all 24 exactly,
  verified center/radius/endpoints). Saved to
  `projects/part-stefan-restored.json`: 132 arcs, 41 profiles (26 →
  41 — the missing sectors are back), extrude = 1 body / 650 faces.
- Open question (user suspicion): trim preview artifacts + whether
  trim can over-consume — needs a dedicated investigation round.
  `next_trim_entity_index` (max+1 id scheme) already guards id
  reuse after trims (comment in trim_entity_commands.inc).
- Proposed guard (not implemented): confirm dialog on large
  multi-entity sketch deletes so lag-invisible deletes can't happen.
- Scratch files (scratch_missing_diag / scratch_restore_tool /
  scratch_repro_damage) are marked SCRATCH — remove before commit.

## Round 12b — stale-selection delete guard (2026-09-14)

User: "I deleted the temporary lines or arcs I did. we might be back
to the bug of stale selection." File evidence supports it: the test
LINES were deleted together with the 24 spokes while the 2 test ARCS
survived — one Delete carried a stale selection.

Mechanism: `add_sketch_line` DOES clear the old entity selection
(verified in sketch_basic_entity_commands.inc), so the spokes were in
a marquee selection made DURING the laggy session — the selection
rectangle never rendered (UI stalled on the 800-log/command flood),
the user pressed Delete, and the hotkey path had NO confirmation
dialog at all. The flood is fixed at the source (round 11); this
round closes the remaining hole.

Fix (UI-only, all delete paths now confirm LARGE selections):
- `deleteConfirmations.ts`: `PendingSketchDeleteConfirmation` gains
  `entityCount`; `LARGE_SKETCH_DELETE_THRESHOLD = 5` exported;
  `confirmAndDeleteSketchSelectionFromContext` opens the pending
  dialog when dependents exist OR entityCount ≥ 5.
- `SketchDeleteConfirmationPanel.tsx`: large-selection branch
  (sketchDelete.manyTitle / manyBody i18n).
- `sketchHotkeys.ts`: hotkey delete now calls
  `confirmDeleteSketchSelectionRef` instead of raw
  deleteSketchSelection — `BindSketchHotkeysParams` +
  `handleSketchDeleteKey` threading.
- `ViewportPanel.tsx` + `viewportPanelTypes.ts`: new
  `onConfirmDeleteSketchSelection` prop/ref → bindSketchHotkeys.
- `App.tsx`: passes `() => confirmAndDeleteSketchSelection(undefined)`
  (no snapshot; count from freshest UI state; core resolves the live
  selection on confirm).
- i18n en.json: sketchDelete.manyTitle/manyBody.

Gates: core rebuild + **46/46 C++ suites** + tsc clean. Awaiting
in-app verification (restored file loads with the full wheel; hotkey
Delete on a big marquee asks first; small deletes stay instant).

## Round 13 — workspace mode leaks: sketch/CAM cross-contamination (2026-09-14)

User: (1) "the sketch is not closed but I can get in to CAM or other
operations — should either block or close it by default"; (2) "when
an operation is open in cam like setup or 2D contour and I can go in
sketch and I have the cam panels visible there"; (3) "I just lost the
ability to set/change in cam setup the origin — the square popping
out on the corners" (regression ~2-3 iterations ago).

Root cause — ONE bug explains all three: the workspace switch never
closed the sketch session, and starting a sketch never closed the CAM
context. With a sketch active in the CAM workspace, the pointer-move
handler's sketch branch returns BEFORE the CAM origin-pick branch
(ViewportPanel pointer-move: `if (activeSketchPlaneIdRef.current) …
return;` comes before the originPick/wcsPick/drillPick branch), so
the origin pick's snap square + click capture silently die — the
"lost" origin set/change.

Fix (UI-only, close-by-default):
- `App.tsx` — the `setWorkspaceView` passed to
  `useSlicerWorkspaceActions` now finishes the active sketch first
  when leaving `cad` (CAM/drawing/GRBL/slicer).
- `App.tsx` — new effect on the activeSketchPlaneId null→id
  transition: closes the CAM setup panel (disarms origin/WCS picks),
  closes the GRBL panel, clears the selected CAM operation and the
  profile repick, and switches the workspace back to `cad`.

Gates: tsc clean. Awaiting in-app verification (sketch closes when
entering CAM; CAM panels close + CAD workspace returns when opening a
sketch; origin pick shows the snap square + places the origin with
the sketch closed).

## Round 14 — G-code + extrusion verification (2026-09-14)

User: "the generated g-code is not correct... I think extruding is
also not going to be correct." Verified from projects/untitled-part.nc
(22:41) and the restored file:

1. **G-code is geometrically faithful.** Parsed all 1408 moves with
   I/J arc centers: G2/G3 sweeps match the sketch arcs exactly (an
   early 360−sweep "inversion" was MY parse bug — G2 sweeps are the
   correct short arcs). 50 laser-on loops, all close exactly
   (lead-in from pierce, chain returns to the contour start,
   lead-out). No duplicated bboxes; total cut length 4196mm vs
   sketch entity length 3460mm (1.21× = kerf offset + leads +
   joins — no double-cutting). The op saved in the file is the OLD
   one (stale region, fails generation with kerf self-intersect) —
   the user deleted it and made new ops in the running app; the new
   ops are not yet on disk.
2. **Extrusion is valid but scope-sensitive.** Extruding the PLATE
   region (the rectangle profile) = 1 body, 650 faces, 1 solid,
   **0 free edges** (closed/manifold). Extruding ALL 41 profiles =
   **31 bodies**: the plate + 30 loose small solids (the flexure
   pieces extruded standalone — the "not correct" look). The inner
   regions are holes of the plate region; they should not be
   extruded separately. User workflow: select the plate region (the
   rectangle interior) and extrude THAT. Possible future UX: mark
   hole-content regions so whole-sketch extrude skips them.

Gates: core rebuild + **46/46 C++ suites** + tsc clean.

## Round 15 — laser-op geometry selection UX (2026-09-14)

User: "the 2d laser operation has a buggy selection. In the drop down
menu I have only the sketch to select. there is a second option
'select geometry' or something but chosing that does not 'stick' and
I try to select individual geometries but there is only a visual clue
about the geometrys (no list or numbers of geometries)."

Root cause (all UI layer — the core chain was verified sound):
1. **Dropdown no-op:** `CamLaserCutPanel` scope Dropdown swallowed the
   empty ("Selected profiles") value (`if (value) onSetScope(...)`) —
   and re-picking the current option never fires onChange anyway.
   Choosing it did NOTHING, so the user never entered the armed
   re-pick state where clicks select profiles additively.
2. **Custom scope never sticks:** every captured region attests
   `sketch_feature_id` (cam_profile_reference.cpp
   append_captured_profile), so `laserOperationScopeSketchId` always
   resolves the sketch — even for a custom subset. The dropdown can
   never show "Selected profiles" after Apply.
3. **No numbers:** the counts exist (geometrySummary, repickHint,
   profilePicked/Removed toasts) but the user never reached the armed
   state, so clicks fell to `selectCamSketchFeature` (sketch feature
   highlight only, no count feedback).

Verified working in the code (no core change needed): clicks during
the arm route through three paths — screen-space line pick
(pickInactiveSketchLine), raycast entity hit (selectSketchEntity),
profile-fill hit (selectSketchProfile) — and App gates all three on
camProfilePickArmed → `reportCamProfileSelectionChange` →
`selectSketchProfileByEntity(id, true)` / `selectSketchProfile(id,
true)` — core toggles additive (profile_selection_commands.inc).
Arcs are covered (profile_owns_entity scans arcs; the raycaster
entity branch is unfiltered after the profile pick).

Fix (UI-only):
- `CamFloatingPanels.tsx` — `customScopeOperationIds` state +
  markCustomScope/clearCustomScope threaded into
  `buildOperationPanel` (it is a separate function scope, not a
  component). Marked on laser Apply (empty-regions update recaptures
  from the live selection), cleared on onSetScope.
- `CamLaserCutPanel.tsx` — new prop `customProfileScope`; dropdown
  value holds "" while armed or custom-scoped; onChange: value →
  onSetScope, empty → onStartRepick (arms the viewport pick);
  `scopeCustomNote` hint under the dropdown when a custom subset is
  active.
- `en.json` — `cam.laserCut.scopeCustomNote`.

Gates: tsc clean.

## Round 16 — Apply-flow fixes + persisted geometry_scope (2026-09-14)

User tested Round 15: "when I chose 'profiles' it stais now and i can
select profiles and they are liste but the old batch is not deleted
and no mater what I select the preview and generate does not change
nothing and the old full sketch path stais".

Root causes found:
1. **Silent apply failure.** `sendCoreCommand` is fire-and-forget —
   core errors (NO_PROFILE_SELECTION) never reject, so the old
   fire-and-forget Apply (empty regions → core reads its LIVE
   selection) marked "Selected profiles" and disarmed even when the
   core refused the capture → dropdown said profiles, whole-sketch
   regions stayed, path unchanged.
2. **Generate/Preview while armed used the STORED regions**, not the
   live picks — picking then Generate regenerated the old path.

Fixes (UI + one core payload field):
- **`selected_profile_ids` on cam_operation_update** (core
  cam_commands.inc): the Apply now rides the explicit ids — the
  capture no longer depends on the live document selection.
- **`camOperationApplySelection`** (useCadCore): awaited
  (`sendCoreCommandAwaited`); an `error` response rejects → visible
  "action error: …" instead of a silent no-op.
- **Shared `applyRepickSelection`** in buildOperationPanel (laser +
  contour + engrave): no picks → friendly message, op untouched;
  success → "Applied N profile(s)" + disarm. **Generate/Preview while
  armed commit the live selection first** (all three panels).
- **Persisted `geometry_scope`** on CamOperation ("sketch" |
  "selected", default "sketch"): cam_types.h + payload
  serialize/parse (absent → "sketch" for old files); set_scope sets
  "sketch"; create sets selected/sketch by capture mode; the update
  re-select gesture sets "selected"; cam_operation_set_generated
  preserves the stored value. TS CamOperation.geometry_scope
  (optional on create payload); panel derives `customProfileScope`
  from the field — the Round-15 session-state flag is REMOVED (core
  field is the single source of truth).
- en.json: applyRepickNoSelection, appliedProfiles.

Tests: cam_commands_test +1 (geometry_scope payload round-trip,
absent-field default, unknown-value passthrough; set_scope asserts
"sketch"), cam_save_load_test laser op carries "selected" + compared.
**46/46 suites pass**; tsc clean; core rebuilt.

Awaiting in-app verification: pick profiles → Apply (message + path
changes) AND pick → Generate directly (commits + regenerates); zero
picks → clear message, old regions intact; Ctrl+S → reload → dropdown
still shows "Selected profiles" for the custom op.

## Round 17 — scope UI split: sketch dropdown + cut-geometry dropdown (2026-09-15)

User: "now the profile selection works but if I generate another 2d
operation it says in the drop down 'not sketch selected'. I need to
select the sketch because is the only option and then I get also the
option for selected geometry. Probably is save to separate it: one
drop down selecting which sketch and another drop down or check box
like Face, Wholw sketch, geometries etc."

The single scope dropdown overloaded three states into one control
(empty = "No sketch selected" OR "Selected profiles" depending on the
op, AND choosing empty armed the re-pick). Split into two dropdowns
in all three panels (laser/contour/engrave):

1. **Reference sketch** — sketches only; empty = "No sketch
   selected"; picking one = whole-sketch capture (unchanged).
2. **Cut geometry** — "Whole sketch" | "Selected profiles"; disabled
   until a sketch is chosen; value from the persisted geometry_scope
   (armed re-pick reads "selected"). "Selected profiles" arms the
   viewport re-pick — this also fixes the round-15 no-op bug still
   present in the contour/engrave dropdowns (they never got the
   round-15 onChange fix).

Contour's Face input stays its own control (unchanged flow). New
customProfileScope prop on contour/engrave panels, fed from
`operation.geometry_scope === "selected"`. en.json: scopeModeLabel /
scopeModeNoSketch / scopeModeSketch / scopeModeSelected per
namespace; scopeCustomNote copy updated. tsc clean; UI-only change.

User also confirmed round-16 picking works in-app. Still pending:
Generate/Apply behavior in the new split UI, Ctrl+S reload persistence
check, and the Round-17 selection-semantics task (next-session item 0).

## Round 18 — WCS tab restored for sheet machines (2026-09-15)

User: "changing in setup to laser removes the wcs tab from laser. It
is available only in milling and it is strange." Verified: the WCS
tab was hidden for laser/plasma by design (CamSetupPanel
isSheetMachine — "the WCS is the sheet origin and there is no
model-body reference to pick"), BUT nothing else exposed the origin
for laser: the workspace origin pick wrote ONLY stock.origin
(App.tsx placeCamOriginFromPick), while the g-code export offsets by
setup.wcs_origin.position (cam_export.cpp) — which stayed 0,0,0 for
laser. The picked origin never reached the program.

Fix (UI-only):
- CamSetupPanel: WCS tab renders for ALL machine types. Sheet
  variant: "Sheet origin" bed-pick button (reuses onPickOrigin /
  originPickArmed) + X/Y/Z fields bound to wcsOrigin + explanatory
  note; face-anchor/orientation/safety/retract stay mill-only. The
  pickedOrigin effect mirrors the pick into wcsOrigin (+dirty) for
  sheet machines; the machine-tab fallback render for the wcs tab
  (old workaround) is removed.
- App.tsx placeCamOriginFromPick: for laser/plasma setups the pick
  ALSO writes wcs_origin {anchor: "point", position} — so workspace
  picks (not just the panel) set the laser program origin.
- en.json: cam.setup.wcsSheetAnchor + wcsSheetNote.

Gates: tsc clean. Awaiting in-app verification: laser setup → WCS tab
exists; pick a corner → X/Y/Z show it; export → coordinates are
relative to the picked origin; milling WCS tab unchanged
(face-anchor).

## Round 19 — g-code verification: geometry correct, two real defects (2026-09-15)

User: "check if all the geometries are cutted correct. If it is
correct cutted definitly the preview is wrong" (screenshot of the
toolpath preview). Historical: projected-geometry cuts destroyed the
machine (sharp angles, short arcs, short lines) while the preview
looked perfect.

Parsed projects/untitled-part.nc (01:03 export, whole-sketch 2D Cut,
1567 lines) with a true-geometry parser (arc lengths from I/J centers
and sweeps, closure vs pierce):

1. **All 50 geometries ARE cut, correctly**: 24 spokes (2 variants,
   59.4/65.7mm, radii 29.07/30.93/42.08/67.8/69.1), 11 outer-ring
   pieces (79.4mm, r 43.93/55.08), 9 tessellated mid-ring pieces
   (78-79mm), 1 hub loop (313mm), 4 corner circles (full-circle
   arcs r 2.172 — kerf INSIDE, correct hole semantics), 1 plate
   outline (803.6mm, r 0.075 corner rounds, 2mm lead). Every contour
   closes; the 4.0mm "closure" on spoke loops = the 2+2mm lead-in/
   out ramp (normal). Total 4268mm. No double cuts.
2. **Defect A — 279 sub-0.05mm moves (machine-killers, the user's
   historical complaint, still present)**: kerf-corner rounding
   (r 0.075 = kerf/2) at every near-collinear vertex of the
   tessellated ring pieces produces micro-arcs (sweep 0.3-1.7°,
   length 0.0004-0.02mm). Task: generator-side threshold — skip
   corner-rounding when the corner angle is below ~10-15° (a sharp
   near-collinear vertex is harmless; a micro-arc stalls GRBL). Needs
   a regression test asserting no move < 0.05mm.
3. **Defect B — the preview draws the 50 G0 TRAVEL moves as
   full-intensity cut lines** (vision analysis of the screenshot:
   "dense spiderweb-like network of red lines crisscrossing the
   central void", "long straight red lines from the workspace
   corners"). The g-code travels are correct (beam off) — the PREVIEW
   is what lies. Task: render rapid/travel moves distinctly
   (dashed/dimmed, or a toggle) so the cut path reads cleanly.
4. User also: stock-origin fields in the Stock tab and the new sheet
   WCS tab are conceptually the same for sheet machines — consider
   consolidating later.

All recorded as next-step tasks; nothing committed.

## Round 21 — CAM → CAD transition cleanup (2026-09-15)

User: "going back from cam to cad leave all the cam panels open like
setup or 2d if they were not closed and I keep seeing the cam path
and I cannot extrude... the gcode path still remaining visible in
cad. that is a mistake also" + "extrusion also does not work at least
not in preview and has also all the stupid dots visible with missing
parts from the geometry."

The CAM → CAD direction of the workspace leak (round 13 fixed the
sketch→CAM direction): armed CAM picks, panels, and the toolpath all
survived the switch. The origin-pick snap dots kept rendering AND
swallowed pointer clicks (blocking extrude profile selection), the
setup/operation panels overlaid the viewport, and the generated path
stayed drawn over the model.

Fix (UI-only):
- `viewportPanelTypes.ts` + `ViewportPanel.tsx` + `sceneSync.ts` —
  new `showCamToolpath` gate: the toolpath lines only render in the
  CAM workspace (the stock box already had `showStock &&
  workspaceView === "cam"`).
- `App.tsx` — a workspace-transition effect (CAM → CAD): closes the
  setup panel (disarming origin/wcs picks + clearing pickedOrigin),
  closes the GRBL panel, clears the selected operation, disarms the
  profile re-pick (finishCamProfileRepick when armed — also clears
  the leftover selection and re-hides the auto-shown sketch), and
  disarms pocket/contour/drill picks.

Gates: tsc clean. Awaiting in-app verification: CAM → CAD leaves no
panels, no dots, no path; extrude preview works immediately; stock
box/WCS marker hidden in CAD; CAM re-entry restores the path.

## Round 22 — exact hole edges end-to-end + slot cap fix (2026-09-15)

User: "Let's start with that... make that work with my sketch" — the
native sketch extruded polygonal surfaces and generated polygonal
holes, while the projected-body sketch worked. Root cause: profile
inner loops (holes) were stored ONLY as chord samples; non-circle
holes were cut/extruded polygonally. Now exact per-hole boundary
edges flow from the detector to the extrude wire builder and the
laser generator.

1. **Detector → data (`sketch_profile_exact.inc` + types):**
   `SketchProfileRegion::inner_loop_edges` — one exact
   `ProfileBoundaryEdge` list per `inner_loops` entry, in the hole's
   walk order; emitted by the shared `build_loop_edges` lambda for
   outer + every hole. Empty for legacy profiles (sampled fallback).
2. **Document round-trip:** sketch-profile payload and extrude
   parameters both serialize/parse `inner_loop_edges` (parser lambda
   `parse_boundary_edge` shared by boundary + per-hole lists).
3. **Extrude (`sketch_wire_extrude.inc`):** `make_wire_extrude_shape`
   builds hole wires from the exact edges
   (`make_wire_from_boundary_edges` — the generalized exact wire
   builder) when present; falls back to `make_profile_wire` (legacy).
   `make_extrude_parameters_for_profile(s)` + preserve helper carry
   the field; `wrap_with_additional_profiles` clears it for
   additional profiles (their holes keep the sampled path).
4. **Laser (`laser_generate.cpp` + cam_planning):** non-circle holes
   cut from `inner_loop_edges` (exact line/arc base segments) instead
   of the chord sample; new vector overload of
   `build_base_segments_from_edges`. Circle holes keep the
   circle_holes descriptor path.
5. **REAL BUG FOUND BY THE REGRESSION TEST — slot left cap swept the
   wrong half.** `slot_expansion.inc` stored the left cap as
   `ccw=false` (tl→bl): the sweep +90°→−90° passes through 0° — the
   EAST bulge through the slot interior (verified: sampled hole
   points went through (8,10) instead of (4,10)). UI + core agree on
   the convention, so slots were drawn wrong everywhere. Fix:
   `ccw=true` (sweep +90°→+270°, west bulge). Not the user's current
   sketch (0 slots in part-stefan.json) but a real latent bug.
6. **Tests:** sketch_profile_test trimmed-circle hole asserts
   `inner_loop_edges` = one exact full-circle edge (id/center/radius
   pinned); cam_generators_test **Test 67** — slot hole: sampled loop
   must pass through (4,10) (west cap pin) AND the generated toolpath
   must contain G2/G3 cap arcs (r ≈ 1.9 = 2 − kerf/2; kerf corner
   arcs r=0.1 excluded by band). Test 67 failed until BOTH the
   inner_loop_edges flow and the slot-cap fix landed.

Gates: `pnpm core:build` (ALL_BUILD — all test exes relinked) +
**46/46 suites** (incl. Test 67 PASS). No TS changes (tsc not
re-run). Awaiting in-app verification: extrude the user's plate
region → holes smooth/exact (no polygonal prisms); laser 2D cut →
hole contours as true arcs; slots drawn as proper rounded rects.

## Round 23 — extrusion of the exact-arc sketch FIXED (2026-09-15)

User (furious): the native sketch (4-6 lines + 108 arcs + 4 circles;
plate = 4-line outer + 22 holes) does NOT extrude — invalid body, 20
free edges, missing surfaces — while the projected polyline sketch
does. "why extrusion does not work?... it is not water tight."
Evidence-driven diagnosis with a scratch test loading
projects/part-stefan.json and building the plate face hole-by-hole
through the exact path (scratch removed after; see below).

Root causes — ALL in `make_wire_from_boundary_edges`
(sketch_wire_extrude.inc), each found and verified incrementally
(chain: holes 0-11 valid, hole 12 invalid even on a fresh face):

1. **Wrong-arc fallback.** The arc-sense disambiguation's midpoint
   tolerance `kMidTol = 1e-4 mm` was far below the ~0.0013-0.0017 mm
   midpoint mismatch caused by walk nodes carrying ~0.006 mm
   trim-engine error off the analytic circles. Correct short arcs
   were REJECTED and the fallback built the MAJOR arc → hole wire
   bow-tie → invalid face. Fix: `kMidTol = 0.5` mm (the wrong arc is
   tens of mm off — two orders of magnitude apart), comment updated.
2. **Edge tolerance never widened by the vertex snap.** The snap pass
   moves shared-joint vertices to the walk nodes (fixing the ~0.006 mm
   per-circle projection split) and widens VERTEX tolerance — but the
   EDGE tolerance stayed at ~1e-7. The face-level curve-on-surface
   check measures the snapped vertex against the edge's curve with
   the EDGE tolerance → every face with a snapped vertex = invalid,
   even though the wire itself is closed, valid, correctly oriented
   (proved: wire analyzer valid, BRepTools_WireExplorer CW, both
   as-is/reversed/SameParameter face variants invalid, topo-only
   check valid, geom check invalid — then widening edge tolerances to
   0.01 made the face VALID). Fix: the snap pass now also does
   `UpdateEdge(cur, max(edge_tol, best + 1e-4))`.

Verification (scratch, then removed):
- All 22 holes valid incrementally → final face VALID → prism VALID.
- Prism: 75 faces, 2 free edges = the circle-hole wall SEAM lines
  (GeomAbs_Line, refs=1 — normal OCCT cylinder seams; the original
  failure had 20 free edges from missing faces).
- `pnpm core:build` (ALL_BUILD) + **46/46 suites PASS**.
- Scratch `tests/scratch_partstefan_extrude_diag.cpp` + CMake entry
  REMOVED (required cleanup). No debug probes remain.

NOTE: the final copy of build/cad_core.exe FAILED (Permission denied)
because the user's running app holds the old core — the app must be
restarted to pick up the fix. Awaiting in-app verification: extrude
the plate region → one smooth watertight body, exact-arc hole walls.

## Round 24 — exact-arc surfaces end-to-end + CAM pick semantics (2026-09-15)

User (furious): "the core was OK as I am always doing pnpm core:rebuild
&& pnpm dev" — the app runs the CURRENT core, so the remaining
failures are real. "if I can select one surface at all it will be
polygons not arcs. The gcode works only if automatically detects the
whole sketch. If I want to do separate profiles I cannot select the
areas that I want."

Evidence (scratch diag on projects/part-stefan-restored.json, removed
after): detection = 41 regions ✓, plate extrude = healthy solid ✓
(core-side was fine), BUT `viewport_state.sketch_profiles[]` shipped
only the 48-point chord samples — the exact `boundary_edges` never
left the core, so every UI surface fill was a polygon. Whole-sketch
gcode was correct because it cuts the plate's EXACT hole edges; a
SELECTED spoke profile fell back to its chord sample. CAM outline
clicks toggled EVERY region sharing the outline (a spoke arc is also
a plate-hole edge) — the plate jumped in/out of the selection with
every click.

Fixes:
1. **Exact edges flow to the UI (core + TS).**
   - `PolygonSketchProfile` + `ViewportSketchProfilePrimitive` gain
     `boundary_edges` / `inner_loop_edges`; populated in
     detect_sketch_profiles + the profile primitive makers; serialized
     in `viewport_to_payload_sketch_primitives.inc` (same edge shape as
     the document payload).
   - TS: zod schemas (viewportStateSchema + documentStateSchema), types
     (viewport.ts ProfileBoundaryEdge, scene.ts ProfileBoundaryEdgeScene,
     geometry/sketch.ts), viewportScene.ts maps both scene makers.
   - Rendering: `sampleBoundaryEdges` (viewportScene.ts) resamples the
     exact line/arc edges at a 0.005mm sagitta bound;
     `exactProfileContour` + extended `smoothProfileHoleLoop` feed the
     fill mesh, the hover/selected edge loop, the display-hole
     contours, and the analytic pick fallback (sketchObjects.ts,
     sketchProfilePicking.ts). Legacy chord fallback everywhere.
2. **CAM outline click = smallest owning region.** Core
   `select_sketch_profile_by_entity` gains `smallest_only` (area
   ordering: πr² for circle regions, shoelace on sampled points
   otherwise); the CAM re-pick call sites pass it (App.tsx); CAD-mode
   behavior unchanged.
3. **Generator micro-arc machine-killer (round-19 Defect A) FIXED.**
   `offset_closed_loop` round-join mode: corners below a 10° turn
   (tessellated chord junctions) snap the offset ends instead of
   emitting a sliver join arc (GRBL stalls on sub-0.05mm arcs; the
   user's historical machine damage). Deviation ≤ d·tan(θ/2) ≈ 7µm at
   the threshold. Test 4 expectation updated (dent corners now snap).

Tests (all fail-before verified):
- `cad_core_selection_test`:
  `test_shared_boundary_click_smallest_only` — rectangle + divider at
  y=15 → default selects both regions, smallest_only selects the
  40×5 top region only.
- `face_projection_arc_test`:
  `test_viewport_profiles_carry_exact_boundary_edges` — rounded-rect
  profile primitive carries 4 line + 4 arc edges with radius/sense.
- `cam2d_test` Test 13 `test_no_micro_joins_on_near_collinear_corners`
  — 2° corners with d=0.075 produce no segment < 0.05mm (fails with
  the threshold disabled).
- Docs: wiki/AI-CAD-Command-Language.md + wiki/IPC-Protocol.md.

Gates: full rebuild + **46/46 suites** + tsc clean. Scratch
partstefan_profile_diag removed. Awaiting in-app verification:
selected surfaces render as smooth arc fills (not polygons), CAM
re-pick outline clicks select only the visible region, laser gcode
for the tessellated sketch has no sub-0.05mm moves.

## Round 25 — multi-profile extrude: cancel/OK failure chain (2026-09-15)

User: "Still not working. I try the extrusion and initially 'wire'
but incomplete and after adding some more surfaces all become
polygons. I try to cancel the operation and I cannot. Finally I press
OK and application crash" — log: repeated "Nothing to undo" then
"Feature not found: feature-18".

Evidence: scratch replay of the user's exact command sequence (load
part-stefan-restored.json → select plate → extrude_profiles automatic
→ additive-select 3 arc-bounded spokes → update_extrude_profiles) is
CORE-HEALTHY: status healthy, 3 additional profiles with exact
boundary edges, viewport 65 solid faces / 0 polygon_extrudes / 1
body; undo works. The failures are UI-state failures:

1. **Cancel never closed the panel.** `undoUntilExtrudePreviewRemoved`
   hammered undo (20×) while `awaitDocumentChange` (4s timeout,
   REJECTS on no event) ran inside `cancelActiveTool` — the rejection
   aborted the cancel chain, so `setExtrudeAction(null)` never ran.
   Fixes:
   - `extrudeFeatureActions.ts` — loop capped at 10; per-attempt
     `awaitDocumentChange(..., 1500)`; EARLY RETURN when the undo did
     not remove the preview (each failed undo emits no document
     event; retrying cannot help).
   - `toolCancellation.ts` — `cancelActiveToolFromContext` wraps each
     handler in try/catch (a failing handler can never abort cancel;
     message via `addMessage`, no console.log per CLAUDE.md);
     `cancelExtrudeTool` clears the panel in a `finally` block —
     the panel ALWAYS closes on Escape, even when the undo path
     failed (a failed undo already left the document untouched).
2. **"Feature not found: feature-18" storm.** The active preview
   panel kept issuing updates for feature ids that no longer exist
   after an undo race. Fix: `ActiveExtrudePreview.tsx`
   `updateExtrudeFeatures` now reads the FRESHEST document from the
   store and silently skips ids absent from `feature_history`.
3. **Polygon fallback invisible.** `wrap_with_additional_profiles`
   logged the wire-path failure at debug (hidden by default) — a
   fallback to polygon prisms changes the extruded geometry and must
   be visible. Now `log_warn` ("Additional profile wire path failed
   — falling back to polygon") so the Logs panel shows it during the
   retest.

Gates: tsc clean + `pnpm core:rebuild` (0 compile errors — only the
final copy of build/cad_core.exe FAILED: Permission denied because
the user's running app (PID 18416) locks it; compile + all test exes
succeeded) + **46/46 suites PASS**. Scratch
scratch_multiprofile_diag.cpp + CMake entry REMOVED.

Retest (user): CLOSE the running app, re-run `pnpm core:build` (the
copy step completes now), `pnpm dev`, then: extrude plate → add
spoke surfaces → if anything turns polygonal the Logs panel now
shows the warn with the reason; Escape MUST close the panel every
time; OK must not crash; no "Feature not found" spam. If polygons
persist, Ctrl+S so the live state can be inspected (the disk file
may lag the in-app sketch).

## Round 26 — gcode double-cut + kerf-skip analysis (2026-09-15)

User: "for gcode I keep getting this: A profile contour was skipped
... self-intersects after the kerf offset"; "I have a feeling that is
cutting 2 times as I have lead in and out both sides of surfaces."
Saved: untitled-part.nc (11:00) + part-stefan-restored.json (11:03).
The screenshot cannot be viewed in this session (harness returns
"unsupported image") — the extrude was verified from the saved part
instead: feature-20 healthy, plate = 4 line edges + 14 exact holes
(4 circles + 10 ring pieces), no polygons. NOTE: the user's running
core predates the session rebuild (copy step blocked by the running
app) — regenerate after the restart.

VERIFIED (core replay via generate_operation_toolpath on the saved
part reproduces the exported gcode exactly: 4 + 56 laser loops):

1. **Double cuts — the user's feeling is CORRECT.** Op cam-op-3
   (whole sketch) cuts 16 contours twice: the spokes/ring pieces are
   captured as STANDALONE regions AND cut again as the hub's/plate's
   HOLE loops. The generate-time detector found them and only
   WARNED: "16 selected profile(s) duplicate holes of other regions —
   they may be cut twice" (in the user's Logs panel). The
   capture-time dedupe (cam_profile_reference.cpp hole_signature /
   matches_hole: centroid + sqrt(area/π) vs mean point radius) fails
   for non-circular contours — elongated spoke shapes exceed the
   0.05 + 0.02·r tolerance, so NOTHING was deduped except the 4
   corner circles (41 − 4 = 37 regions). Plus the 4 corner circles
   are cut twice ACROSS ops (cam-op-2 selected circles + cam-op-3
   plate circle holes).
2. **Kerf skips are REAL.** cam-op-3: 3 profile contours + 16 hole
   contours SKIPPED with the self-intersection error (0.075mm
   offset). The 3 profiles = 2 three-arc spoke regions + the 5-arc
   ring piece — genuinely narrow flexure features whose offset
   collapses. Skipped contours are simply NOT CUT (missing pieces in
   the part).
3. **Leads are by design**: every loop has a 2mm arc lead-in + 2mm
   arc lead-out (pierce on the scrap side). The "lead on both sides"
   impression = the double cuts (2× leads per surface).
4. **The hub cut is CORRECT** — the hub's r=44 334° arc is emitted
   as a true major arc (G3 with correct I/J); my initial "minor arc"
   alarm was a Python parser artifact (chord 2r·sin(θ/2) is
   identical for θ and 2π−θ). offset_arc_sweep and the viewport
   linearizer both handle >π sweeps correctly.
5. Anomaly for follow-up: the plate's circle holes were cut at
   r EXACT (2.247, no kerf) while the same circles as standalone
   regions were cut at r+kerf/2 (2.322). The CURRENT source offsets
   circle holes (2.172 or 2.322 depending on the path) — the
   running core lags the source; re-verify after the restart.

PROPOSED FIXES (next session, core cam):
- Dedupe at generate time: the duplicate detector
  (laser_generate.cpp ~627) should REMOVE the duplicate group
  (prefer the standalone cut, drop the hole-loop copy) instead of
  only warning — exact-contour comparison (boundary edge arc sets),
  not centroid/radius.
- Degrade instead of skip: when the kerf offset self-intersects,
  fall back to cutting the centerline (offset 0) with a warning, so
  narrow features still get cut (currently they vanish from the
  part).
- Capture-time dedupe: replace hole_signature/matches_hole with an
  exact per-edge match for non-circular contours.
- Regression tests per CLAUDE.md (cam_generators_test): narrow
  feature falls back to centerline (not skipped); duplicate
  hole/standalone pair emits ONE loop.
- User-side check: the 2 skipped spoke regions + the 5-arc ring in
  the sketch are narrow (< ~2×kerf somewhere) — enlarging them
  avoids the skip entirely.
- The vision screenshot could not be viewed ("[Unsupported Image]"
  from the harness on a valid PNG) — extrude state verified from
  the saved part instead.

## Round 27 — kerf=0 still skips: the offset machinery, not the kerf (2026-09-15)

User: "I put the kerf =0 which suppose to cut on the line but still
have this alarms"; "3 skipped features are genuinely narrow in your
sketch but how to know which one is?"; requested: preview should
detect issues, kerf used in min-radius checks + exposed to the
operator, dedupe should offer the operator a choice, an "Analyze"
function with progress that stops per issue (batchable for
thousands-of-lines projections). Also: "for the angle it keeps
complaining about 90deg and says the closest values are 76 and 92" —
message NOT FOUND in the UI or core; needs the exact text/field from
the user.

VERIFIED (core replay with kerf forced to 0 on the saved part):
- **The saved whole-sketch op ALREADY has kerf=0.00** (the user's
  setting saved correctly), and the generator STILL skips the SAME
  16 contours (3 profile + 13 hole) at zero offset. The warning
  message "self-intersects after the kerf offset — the feature is
  narrower than the kerf width" is therefore MISLEADING at kerf=0:
  a zero-offset contour cannot be narrower than a zero kerf.
- Root-cause hypothesis: at d=0 the round-join path in
  offset_closed_loop still emits zero-radius join arcs at arc-arc
  corners with gaps > 1e-9 (trim-engine error), and
  sample_offset_loop turns a 0-radius arc into a single point at
  the vertex (radius 0 → toleranceRatio inf→1, sweep 0) — a spike
  in the sampled loop → false self-intersection → skip. Fix: when
  kerf==0 or kerf_side=="none", bypass the offset machinery
  entirely (planned loop = base segments, gap-snap only, no
  self-intersection gate), and reword the warn to name the contour.
- The 3 skipped PROFILES identified (the pieces that will be MISSING
  from the cut): two 3-arc spokes at sketch coords (−33.0, 42.0) and
  (−44.6, 73.8) — ids profile-poly-arc-44-arc-46-arc-59-… and
  profile-poly-arc-76-arc-77-arc-85-… — plus the 5-arc ring piece at
  (−57.6, 3.0), id profile-poly-arc-2-arc-25-arc-26-arc-30-… (the one
  with the tiny 2.4mm hole). Their boundaries do NOT self-intersect
  at offset 0 (Python exact check, 240 pts/edge) — confirming the
  false positive is in the generator's offset/sampling path.
- The double-cut 16 (round 26) are acknowledged user-caused for the
  4 circles (two overlapping ops) — but the 16 spoke/ring doubles
  are generator-side (whole-sketch capture + hole cuts).
- **Extrude verified from the saved part**: build_viewport_state →
  bodies=1, solid_faces=50, polygon_extrudes=0 — the plate + 14
  exact-arc holes compiles to one valid solid, no polygon fallback.
- Extrude feature-20: boundary 4 line edges + 14 inner_loop_edges
  (4 circles + 10 four-arc rings), healthy, new_body depth 20.

NEXT ROUND PLAN (CAM diagnostics UX — needs user sign-off before
coding; CLAUDE.md workflow):
1. kerf=0 / kerf_side "none" → bypass offset entirely (fixes the
   false skips; regression test: the 3 skipped regions generate at
   kerf 0).
2. NAME the skipped contour in the warning (attestation center +
   kind, or a profile label) so "which one" is answerable; same for
   the duplicate warning.
3. Preview pass must surface the same diagnostics (warnings shown on
   Preview, not only Generate) + highlight the affected contours in
   the viewport.
4. Kerf-aware minimum-radius check: min feature radius ≥ kerf/2
   (laser/plasma); expose the kerf value to the operator.
5. Dedupe with operator choice: keep hole version / standalone /
   both — per pair or bulk ("apply to all").
6. "Analyze" flow: progress-driven pass stopping per issue with
   accept/ignore decisions, bulk-apply for projection-heavy
   sketches (thousands of lines) — design as a panel, not a modal
   storm.
7. Locate the 90°/"76 and 92" complaint (need the exact message).
   NEW: user says the refusal appears as a FLOATING WINDOW on
   Generate in the 2D Cut panel — the panel's debounced param edit
   triggers a live onPreview generation and every core error
   surfaces a toast, so the "refuse" = the running core rejecting a
   live edit. NOT reproducible on the saved part with the CURRENT
   source (generate ok=1) — the message must come from the stale
   running core (no "closest/nearest/76/92" strings exist anywhere
   in the current core or UI). After the app restart: if the 90°
   refusal persists, the user saves + pastes the exact message (the
   76/92 numbers are computed from the live geometry).
8. NEW USER REQUEST: "option to skip the kerf" — laser can work
   without piercing and kerf on some materials, just more passes.
   kerf_side "none" + kerf_width 0 exist as parameters; the Round-27
   offset-bypass fix makes them actually work. ADD: a pierce-less
   mode (laser on at the contour start, no dwell/leads) to pair
   with it. Recorded for the CAM diagnostics round.
9. Round 25/26 leftovers: regenerate after app restart (running
   core still predates the session rebuild).

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
---
# Active task: Laser machine bring-up — FluidNC board swap + first cut (2026-09-13)

> **Branch:** `cam/laser-testing` (checked out; pushes to origin with this update)
> **Previous sprint (GRBL transport, PR #80) is merged — this file now tracks the live-machine work.**
> **2026-09-13 (this machine): connection-loss investigation COMPLETE — root
> cause found and fixed in `gcode_sender.rs` + workspace; see "Root cause" below.
> UNCOMMITTED on `cam/laser-testing`; pending user in-app verification.**
> **2026-09-13 round 2 (this machine): GRBL workspace polish + gcode generation
> work — orientation cube consolidated, burned-color cut progress, big STOP
> button, laser cut from body faces with contour cleanup. All 46 core suites
> green + tsc clean. UNCOMMITTED; pending user verification on the machine.**
> **2026-09-13 round 3 (this machine): Douglas-Peucker base-loop
> simplification (0.03 mm deviation) wired into plan_loop; join-snap
> threshold raised 5° → 10°; Test 15 added, Tests 4/8d/14 re-pinned.
> All 46 core suites green. UNCOMMITTED; pending user verification on
> the machine. IN PARALLEL: user is flashing FluidNC 4.1.0 (Timed-engine
> fixes) — board config must be re-uploaded as `/config.yaml` afterwards
> (see Machine state).**

## ROOT CAUSE of the first-cut failures (found 2026-09-13, this machine)

The sender's status poll sent **`?` WITH a newline** every 500 ms. On this
FluidNC board a newline-terminated `?` is a LINE command: it answers with the
status AND an extra **ok**. Each spurious ok pops a real line's slot from the
127-byte window accounting → the window drifts open without bound → the sender
over-drives the board's input queue → dropped bytes (USB error:36 "I/J tail
dropped"), dropped TCP connections (~line 862), and the WS channel wedging
silently. Verified empirically on the board (bare `?` → status only, no ok;
`?\n` → status + ok) and by replay (a faithful sender replica with `?\n`
fails, with bare `?` the whole 1876-line dense synthetic job completes on
TCP AND WS). LaserGRBL's separate failure at ~raw 1700 is not this bug (it
polls with a bare byte) — likely the board planner + file density, TBD.

Fixes applied in this working tree (gcode_sender.rs, uncommitted):
1. **Poll sends the bare real-time byte** `?` (no newline) — never an ok.
2. **Stray-ok guard**: an ok with nothing in sent_lengths mid-job is logged
   and ignored, never acked against a line.
3. **Link-loss handling**: EOF or hard read error mid-job now aborts the job
   state, tries a last-ditch reset byte, emits a loud error event
   ("CONNECTION LOST mid-job … press Reset or cut power immediately") +
   disconnected. EOF no longer busy-spins the worker.
4. **WS final-ack fallback**: FluidNC's WS channel never acks the FINAL line
   of a job (verified: 3-line job gets 2 oks) — completion now also fires
   when all lines are sent and the board reports Idle for 1.5 s.
5. **React loop fixed**: `GrblWorkspace` memoizes the embeddedProgram object
   (was: fresh object every render → panel effect → setState → "Maximum
   update depth exceeded"); the panel effect now content-compares too.
6. **GRBL viewport orientation**: left-drag rotates, right-drag pans (the
   shared config disabled both), plus a mini orientation cube overlay
   (click a face to snap top/front/right/…; new tokens --cad-cube-x/y/z/edge
   in all 6 themes).

Diagnostic harness: `%TEMP%\grbl_replay.mjs` — faithful sender replica
(filter parity, 127-byte window, pump-on-ok, poll cadence) for TCP 23 and
WS 80 (hand-rolled WS client), synthetic dense-job generator (830 1° arcs +
G1 fill). Usage: `node grbl_replay.mjs [host] [port] [file|synthetic] [pollMs] [tcp|ws]`.
Board must be in CHECK MODE (`$C`, status shows `<Check|…>`) before use.

## Follow-up (not urgent)

- **Orientation cube duplication:** `app/grbl/grblOrientationCube.ts` is a
  second implementation next to the main viewport's
  `layout/viewport/viewCubeRender.ts` + `utils/viewCube.utils.ts`. The
  split is architectural (GRBL preview = own renderer/canvas, Z-up bed
  convention; main cube = render-target blit inside the CAD viewport's
  renderer, Y-up, animated snaps). Still ~80% of the face-texture /
  token / edge / picking logic is shared — extract a common
  `orientationCubeCore` (mesh build + picking, parameterized by up-axis
  and snap-vs-animate) and let both consume it. Do AFTER the cutting
  tests (touches the CAD viewport's cube).

## Next-session checklist

1. **User verification in the app** (binding): stream the real .nc over TCP
   and WS — it should complete now; watch for the stray-ok warnings in the
   console on any transport.
2. Re-test the real file on the OTHER station (the 3490-line one lives
   there); USB error:36 should be gone with the poll fix (the CH340 drop was
   our over-send, not the FIFO).
3. LaserGRBL's failure (~raw 1700) is separate — if it persists after our
   fixes, the board planner / file density is still a factor.
4. WS note: FluidNC never acks the final line over WS — jobs end via the
   Idle fallback now; TCP acks everything, so TCP remains the recommended
   transport for streaming on this board.
5. Commit after verification (no Co-Authored-By trailer).

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
---
## G-code generation round (2026-09-13, uncommitted on this machine)

The burn-test file is terrible at the SOURCE: the part is mesh → body →
**projected sketch**, and the laser op cut the sketch profiles — the
projection/arrangement splits curves into ~1° arc fragments and duplicates
edges. Fix: cut the BODY directly (the laser generator already had a face
path) + heal every contour before the kerf offset.

- **Face-path exactness** (`laser/laser_generate.cpp`): the face path now
  builds `build_base_segments_from_wire` (exact line/circle edges → real
  G2/G3, one arc per circle) with the old sampled-polyline fallback for
  spline/ellipse wires. Full-circle loop area + orientation resolved via a
  wireLoopArea helper (shoelace is 0 for start == end).
- **Contour cleanup** (`cam2d.h/.cpp` — `cleanup_base_segments`): merges
  consecutive collinear lines (angle eps 1e-4 rad, both signs tested),
  consecutive co-circular arcs (center/radius eps 1e-4), drops consecutive
  exact duplicates, out-and-back spurs, and zero-length lines. Applied in
  `plan_loop` — one choke point for profile outers/holes AND face wires.
  One structured log line per generate: "contour cleanup: merged N lines…"
  (tag `cam_laser`, visible in the Logs panel).
- **UI** (`CamLaserCutPanel` + `CamFloatingPanels` + `App.tsx`): laser ops
  with a face region show "Cut from body face" + a **Pick face** armed pick
  (mirrors the contour op's face pick; TNP-safe capture, stock faces
  rejected). Face selection at 2D-Cut time already worked
  (`camLaserActions.ts`); this adds RE-picking for existing ops.
- **GRBL workspace polish (same uncommitted batch)**: orientation cube
  deduplicated into the shared `@/utils` cube core (GRBL shell only);
  executed toolpath segments render in the new `--cad-toolpath-burned`
  token (all 6 themes) — "Cut (burned)" legend; big STOP button in the
  workspace toolbar (bg-danger, grblReset + overlay clear, disabled when
  disconnected).
- **Tests**: `cam2d_test.cpp` Test 13 (cleanup unit rules, both epsilon
  signs); `cam_generators_test.cpp` Tests 8b (circular face → ONE exact
  arc, no chord polylines) + 8c (split sketch side merges into one cut).
  All 46 suites green (`pnpm test:core`), `tsc --noEmit` clean.

0.5. **CAM program polish (Round 18, user: "Check your facts and also
   add for the next step what is neccessary").** Verified facts +
   tasks:
   - **No explicit rapid from origin.** The laser program header is
     `(op name) / G21 / G90 / G94 / G17 / M5` and the first move is a
     G0 straight to the first loop's lead entry — the start position
     is the machine's wherever-it-is, there is NO `G0 X0 Y0` and no
     return-to-origin at the end (laser footer = `M5 / M2`). Applies
     to ALL laser ops, whole-sketch and selected-profiles alike.
     Task: post-template preamble/postamble — explicit origin start
     and end-of-program laser off + air off (M9 is already emitted
     when air was on) + return-to-origin, configurable in the post
     definition files (post_processor.cpp builtin templates).
   - **Multi-op export ALREADY combines**: `cam_export.cpp`
     post_document posts every ENABLED op in order into one file
     (footer on the last op; failed-generation ops are skipped with
     comments). The "saved separately" experience was separate
     exports at different times. Missing: a visible UI choice —
     "export all operations" (default) vs "selected operation only".
   - **Multi-setup origins exist**: each op exports through its own
     setup's WCS origin. Verify the setup-creation UX lets the user
     add consecutive setups (setup → ops → setup → ops) and that the
     combined file sequences them correctly.
   - **Toolbar machine-type bug (user-verified)**: the laser Cutting
     tab renders a Facing (face-milling) button — a mill op in laser
     mode (CamCuttingToolbar.tsx CamFaceOpButton, label
     common.faceOp). "2D Contour"/Pocket/Adaptive/Drill/Slot/Engrave
     live on the Milling tab; the tab follows machine-type changes in
     Setup but manual tab clicks can show mill tools on a laser
     machine. Task: remove Facing from the cutting toolbar; decide
     gating for manual tab switching (laser users should not see
     mill-only ops). Laser "engrave" = the Mode option inside the 2D
     Cut panel (cut/score/engrave) — not a separate toolbar button.

0. **NEW FIRST PRIORITY — sketch selection semantics (Round 17, user:
   "so bad and annoying").** Standard selection behavior:
   - A plain click on a new geometry REPLACES the current selection
     (deselect everything else); Ctrl (or Shift/Cmd) click adds.
   - Drawing a geometry must NOT leave it selected afterwards — "make
     a rectangle and the last line in the rectangle remain selected".
     CONFIRMED: every draw command sets the SINGULAR
     `selected_sketch_entity_id = ...back().id` and clears the plural
     list (sketch_basic_entity_commands.inc ~lines 36/90/234/268/297/
     330/361). The singular id feeds the dimension editor (auto-open
     on the new line's dimension) — decide whether to clear it after
     draw or keep it but exclude it from DELETE resolution.
   - Delete on 2 clicked arcs reported 9 geometries: the delete
     resolver merges the SINGULAR ids into the delete set
     (`sketchSelectionDelete.ts` `currentSketchDeleteSelection`
     dedupeSelectedIds) — a stale last-drawn entity (and projection
     batch-selections: after a projection every generated point stays
     selected, sketchHotkeys.ts comment) joins the clicked set.
   - Where the "9" accumulation really came from needs a repro FIRST
     (plan mode): candidate paths — UI additive flags (verify plain
     clicks send additive=false through pointerUpActiveSketch /
     pointerUpSceneSelection), the singular-id merge above, projection
     leftovers, and click-on-profile/vertex clearing asymmetries.
   - Regression tests per CLAUDE.md (cad_core_selection_test.cpp):
     click-replaces/Ctrl-adds, draw leaves nothing selected (or
     delete ignores it), delete set = clicked set only.
   - Memory: selection-highlight-follow-up.md updated with these
     requirements.
1. **User verification in the app (binding per CLAUDE.md):**
   - Round 16 (CAM laser selection): pick profiles → Generate commits
     + regenerates; Apply → "Applied N profile(s)" + path changes;
     zero picks → clear message, old regions intact; Ctrl+S + reload
     → dropdown still "Selected profiles".
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

## Round 28 — sketch fillet on arc operands (2026-09-15, in progress)

User: "I am trying to modify the sketch but I found out that the
fillet tool does not work on arcs". Scope confirmed via plan:
**fillet only** (chamfer stays line-line), **current radius UX kept**,
line-line behavior byte-identical.

Plan: C:\Users\ThinkPad\.claude\plans\delightful-snuggling-thunder.md
(user-approved). Three commits planned:

- **Commit A — API surface + fail-before tests (DONE, uncommitted).**
  `SketchFillet` gains optional `arc_a_id`/`arc_b_id` (kind derived:
  non-empty arc id ⇒ arc operand). Defaulted params threaded through
  `add_sketch_fillet` (feature decl, DocumentManager, entity command).
  Create command dispatches on kind: line-line falls through to the
  verbatim v1 body; arc cases threw "not yet supported" at this stage.
  New suite `tests/sketch_fillet_arc_test.cpp` (16 tests,
  hand-computed geometry: canonical line-arc r=2 → O=(37.746,2),
  arc-arc → O=(41.796,2.204), corner-at-END mutation, radius edits,
  delete restores, oversized/tangent/construction rejections,
  enforcement after far-end move + radius-dimension drive, profile
  set via profiles_match, trim/delete sibling cleanup, both-ends
  guard, v1 line-line regression). Verified fail-before: 12 arc
  tests threw the not-yet-supported error, 46 old suites green.

- **Commit B — solver + create/enforce/delete (DONE, uncommitted).**
  New `impl/private_fillet_arc_solver.inc` (included in the anon
  namespace before private_fillet_refresh.inc): shared
  tangent-circle solve for line-arc/arc-arc. Candidates = offset
  carriers (line ±r; concentric R±r, internal skipped when ≤ eps);
  fit rules = line foot within (ε, len−ε), arc trim angle within the
  walk-directed body sweep (corner-at-END flips the direction),
  distinct trim points; tie-break = minimal total trim (hugging the
  corner — load-bearing for wide-sweep arcs). Create: validation
  ladder (shared corner, non-construction/non-degenerate arcs,
  extended double-fillet guard, chamfer guard unchanged, tangent
  corner rejected via away tangents), operand mutation mirrors v1
  (arc corner endpoint swaps to the trim id). Enforcement
  (`enforce_sketch_fillet_arc`): corner re-derived (line×circle
  unclamped quadratic / circle×circle, nearest cached corner), arc
  far coords prefer a line sharing the far vertex (pass-ordering
  freshness), silent-continue contract preserved. Delete: restore_arc
  mirroring restore_line. Sibling cleanup predicates extended with
  arc_a/arc_b in trim_line_relation_cleanup.inc,
  trim_arc_entity_command.inc, sketch_delete_selection_commands.inc
  (zero-length-line cleanups need no change — line ids covered).
  Two bugs found via trace prints during bring-up, both fixed:
  corner-derivation roots evaluated with the UN-normalized direction
  (factor-of-len), and the line-arc corner branch read the arc from
  the wrong side (end() deref — inverted ternary). ALL 47 suites
  green including the new one.

- **Commit C — serialization + UI + AI + docs (DONE, uncommitted,
  awaiting user in-app verification).** Core payload writes arc_a_id/
  arc_b_id only when non-empty, reads via read_optional_string; app
  handler reads all four ids optionally. TS: SketchFilletEntry,
  AddSketchFilletCommand payload, documentStateSchema, command
  builder (omit empty arc ids), picker rewritten to accept any
  line+arc pair with kinds (generated entities excluded), plumbing
  threaded through useCadCore/App.tsx/viewportPanelTypes/
  viewportCallbackRefs/pointerUpActiveSketch; chamfer picker guard
  verified covered as-is. AI zod + prompt updated. Wiki IPC +
  AI-CAD docs updated. NEW serialization round-trip test added to the
  suite (payload → document_from_payload preserves arc operand ids +
  generated arc). Gates: tsc clean, 47/47 suites green.
  Payload: write arc ids only when non-empty; read with
  read_optional_string. App handler reads all four ids optionally.
  TS: documentStateSchema.ts fillets, geometry/sketch.ts
  SketchFilletEntry, types/ipc/sketchCommands.ts payload — optional
  arc_a_id/arc_b_id. Picker sketchFilletPicking.ts accepts
  line+arc pairs (excluding generated arcs), returns kinds;
  plumbing through makeAddSketchFilletCommand, useCadCore.ts,
  App.tsx, viewportPanelTypes.ts, viewportCallbackRefs.ts,
  ViewportPanel.tsx, pointerUpActiveSketch.ts; chamfer picker
  already-filleted guard extended. AI zod + prompt. Wiki docs.
  tsc clean, then user in-app verification.

NOTE: without Commit C, arc fillets lose their parametric relation on
save/load (ids not serialized) — the shape survives, the fillet
record degrades.

## Round 29 — split-vertex heal + tolerant fillet + id-counter collision fix (2026-09-15, in progress)

User: fillet refuses the 7 sharp corners in the flower center;
snap flip-flops "end point"/"intersection"; at max zoom 2 lines
visible; "the trim tool did this... I want to know if this was not
affecting also the extrude operation... I want the heal operation and
the fillet to but the cause need to be eliminated."

Diagnosis (scratch tests, removed):
- The 7 corners each hold TWO vertex ids a constant 1.31e-5 mm apart
  — arc-fit noise from the LEGACY projection build (two mint bursts,
  vertex-1188..1249 and vertex-1267..1297), before cross-record
  endpoint welding existed. TODAY'S trim/extend welds correctly
  (scratch cases proved it) — trim is innocent; the splits are legacy
  projection artifacts. Root cause already eliminated in the current
  core.
- **Extrude is NOT affected**: 7/7 split corners join in profile
  regions (the exact detector welds by coordinates at 0.01) — 41
  profiles, extrude healthy. Confirmed to the user with evidence.

Core changes (this round):
1. **Heal command** `merge_coincident_sketch_points { feature_id }`
   (new `sketch_merge_coincident_points_commands.inc`): union-find
   clustering of every referenced vertex within 0.01 mm (smallest id
   = representative, position from first occurrence), remaps line/
   arc/circle/ellipse/spline endpoints + centers, fillet/chamfer
   corner/trim ids, anchors, constraint target_ids; single undoable
   action; refresh + geometry bump. Included via
   sketch_entity_commands.inc, declared in
   document_manager_sketch_entity_commands.inc, handler in
   sketch_create_fillet_command_handlers.inc.
2. **Tolerant fillet weld** (`sketch_fillet_create_command.inc`
   generic path): corner resolution now accepts an operand endpoint
   within kCoincidentTolerance (0.01) of the corner VERTEX position
   (vertex-table lookup), then PERMANENTLY welds that endpoint's id +
   coords onto the canonical corner id — delete then restores both
   operands to one shared vertex.
3. **REAL PRE-EXISTING BUG FOUND + FIXED — sketch id-counter
   desync.** Sketch-layer minting (array copies, trims, offsets —
   all via `next_trim_entity_index`) never bumped the DocumentManager
   id counters, so a SAME-SESSION fillet/chamfer after any array
   copy/trim/offset minted a colliding id (test proved it: the array
   copy and the fillet arc both got `arc-3`; enforcement then
   overwrote the copy with fillet geometry). Fix:
   `sync_sketch_id_counters()` in manager_state_helpers.inc, called
   from `bump_geometry_revision` (every mutation) — monotonic max
   over live ids for all 12 sketch counters (never lowers, so
   undo/redo can't reuse freed ids). Mirrors the load-time restore.

Tests (sketch_fillet_arc_test.cpp, 25 total — 4 new):
- heal merges split corner (fabricated via zero-offset
  create_linear_array — the array copy path deliberately mints fresh
  vertex ids at coincident positions); heal leaves far vertices
  (>0.01) alone.
- fillet welds split arc-arc corner (canonical trims on the shared
  corner, delete restores both operands to one vertex); corner
  beyond tolerance rejected.

UI (this round):
- DocumentHierarchyPanel sketch context menu — "Merge coincident
  points" (i18n sketch.mergeCoincidentPoints); IPC type/builder/hook
  wired through AppSidebar → App.
- sketchFilletPicking.ts — picks across ALL vertices within the pick
  tolerance (split corners), dedups incident entities, returns entity
  A's nearest endpoint as the corner id; already-filleted guard
  matches any near vertex.

Gates: **49/49 C++ suites PASS**, tsc clean. Scratch tests
(scratch_trim_weld_test / scratch_load_profiles_test) + CMake entries
REMOVED. Awaiting user in-app verification: right-click sketch →
Merge coincident points → the 7 flower corners fillet; the flower
extrude still healthy; then commit (approval needed, no
Co-Authored-By).

## Round 30 — extrude hole-touch guard: refusal instead of silently broken body (2026-09-15, in progress)

User: "extrusion does not work... the body I see has many missing or
strange areas... I exported as dxf and imported in Fusion and has many
areas that cannot generate surface. The trim function working
different now?" — approved the fix plan ("yes go ahead").

Diagnosis (verified with BRepCheck on the saved file — user was
RIGHT, it was not a rendering problem):
- The plate's cap faces were INVALID: hole loops sharing welded
  vertices (flower petals drawn with sub-0.01 mm slits against the
  hub) — OCCT cannot build a planar cap whose hole wires touch. The
  DXF/Fusion failures were a faithful export of that broken state.
  Trim is unchanged (suite green); its leftover stubs plus the new
  closing arcs exposed the topology.
- The face walk itself was CORRECT (PS_TRACE_FACES 4-edge walks) —
  no detector change needed; task #18 (degenerate-hole rejection)
  DROPPED, the extrude-side guard catches the same cases.

Fix (`private_extrude_profile_parameter_helpers.inc`):
`validate_hole_separation()` — samples each hole loop (16 samples per
circular edge, endpoints for lines; `inner_loops` fallback when
`inner_loop_edges` is empty, e.g. circle_holes path), refuses gaps
< 0.05 mm against the outline AND between holes. Collects EVERY
violation and throws once: "Extrude refused: 3 pairs of holes touch
or overlap (smallest gap 0.000 mm). Separate the pieces by at least
0.05 mm and extrude again." (count-variant wording per UI copy
rules — no internal ids in the message; per-pair log_warn entries
carry loop indices). Called from polygon / ellipse /
multi-profile parameter builders.

Verified on the user's current file (projects/part-stefan-fillet.json,
now 4 lines / 144 arcs / 12 fillets / 41 profiles): the plate refuses
with 3 touching pairs — hub loop (arc-20/arc-98) touches petals
arc-3, arc-7, arc-119 (gaps 0.004 / ~0 / 0 mm). User must enlarge
those 3 slits to >= 0.05 mm, then the plate extrudes cleanly.

**Follow-up (user dispute → confirmed):** the user rejected the
"slits" explanation — CORRECTLY. scratch_touch_analysis proved the
petal INNER arcs are drawn EXACTLY ON the hub circle (same center
(-54.7,52.1), same r=44 as the hub's 334-degree arc-98: arcs 26, 32,
47, 52, 61, 70, 79, 92, …) — overlapping curves, the user's own
hypothesis. The 2 gap-0 pairs are coincident curves; only arc-3's
pair is a real 0.004 mm slit. Fix = draw the petal inner arcs at a
smaller radius (visible gap >= 0.05 mm) before the circular array;
then the hub stays solid, spokes connect it, extrude is valid. Their
laser design is fine as 2D cuts — the 3D extrude cannot represent
coincident hole wires. Suggested follow-up feature (not scheduled):
sketch warning for curves lying exactly on top of each other. DXF has
134 arcs vs sketch 144 (export predates last 10).

Tests (extrude_quality_test.cpp, 3 tests):
- touching hub+petal topology (the user's exact shape, minimized) →
  refusal with "touch" in the message
- same geometry separated 0.2 mm → healthy extrude
- TWO touching pairs in one plate → message names "2 pairs of holes
  touch or overlap" (one refusal round, not one per slit)

Gates: **49/49 C++ suites PASS** (extrude_quality rebuilt against the
new lib). Scratch diag (scratch_extrude_fillet_diag) + CMake entry
REMOVED. tsc unchanged this round (no TS edits). Awaiting user
in-app verification: extrude the plate → refusal naming all 3 pairs;
enlarge the 3 hub-petal slits ≥ 0.05 mm → extrude works; then commit
(approval needed, no Co-Authored-By).

## Round 31 — profile-walk fix: phantom major-arc half-edge (2026-09-15, done — awaiting in-app verification)

**Round 30's conclusion was wrong.** The user's dispute was RIGHT a
second time: nothing in the wheel sketch is drawn wrong. The "3 pairs
of touching holes" were an artifact of the face walk walking one hole
edge the LONG way around its circle. No sketch changes are needed.

Root cause (user's wheel, hub arc-98, ccw 26.08° on the r≈44 circle):
- The intersection solver computed a split on arc-98's carrier circle
  2 ULPs BELOW `sweep_start`. `exact_lift_to_sweep` lifted it +2π, so
  the seq contained a phantom point at sweep_start+2π.
- The unique-collapse kept the phantom (lowest after the -2π wrap) but
  dropped the arc's real endpoint — and the phantom produced a half-edge
  of 334° (the major arc), which the largest-cw-turn face walk PREFERS
  (5.44 rad of turn beats the correct 2.25).
- Result: the hub hole wire wrapped the whole circle, crossing the 11
  neighbouring petal loops; the point-sampling guard only caught 3 of
  the collisions. The exported DXF/Fusion breakage was a faithful
  export of that phantom wire.

Fix (`sketch_profile_exact.inc`, seq-building in build_exact_profiles):
- After the unique pass, filter split params against the arc's drawn
  sweep domain in LIFTED space (direction-aware valid window), drop
  out-of-sweep lifts, then re-push the arc's own endpoints
  unconditionally (a collapsed endpoint must never erase the endpoint).
- Circles/full ellipses keep the unconditional closing entry.

Regression test (`sketch_profile_test.cpp`,
test_hole_arc_walks_short_span_not_major_arc): the user's EXACT
17-digit hub-wire literals (arc-20/96/98/134) + a ±150 rect — fails
before ("walked arc span must stay below 180 degrees"), passes after;
asserts the full profile set and every hole-edge span < π. Rounded or
perturbed literals do NOT reproduce (the phantom needs the exact ulp
coincidence), hence full-precision literals.

All 4 temporary DIAG blocks in sketch_profile_exact.inc REMOVED
(ISECT/TOUCH/splits/DIAGHE).

Gates: **49/49 C++ suites PASS**. scratch_touch_analysis on the user's
file: 0 projections, 0 long-way edges, hub loop = 4 short arcs
(12°/26°/12°/27°), and the end-to-end `extrude_profiles` on the plate
now returns **status=healthy** (was "3 pairs of holes touch"). The
extrude guard (Round 30) stays — it is the safety net for genuinely
touching wires.

Next session checklist:
- Remove scratch_touch_analysis.cpp + its CMake entry before commit
  (kept until then as a diagnostic tool).
- Await user in-app verification: reload the wheel file → extrude the
  plate → healthy body, no refusal, hub/spokes continuous. Then commit
  (approval needed, no Co-Authored-By) — message should name
  cad_core_sketch_profile_test + the full test:core run.
- The user's "stupid surface detection algorithm" verdict: documented
  honestly — the walk now matches the drawn arcs.

## Round 32 — laser kerf offset: sharp arc corners + duplicate holes (2026-09-15, done — awaiting in-app verification)

User extruded the wheel successfully (Round 31 verified in-app), then
generated the laser gcode and hit 36 "A hole contour was skipped: A
hole contour self-intersects after the kerf offset" warnings — the
gcode was missing 36 of 40 hole cuts.

Root cause 1 (cam2d offset): `offset_closed_loop`'s arc-corner branch
assumed arc corners are tangent ("tangent by sketch construction") and
always drew a round join.  The wheel's trim corners turn toward the
offset side (right turns) — there the two offset curves cross ahead of
the corner, so the round join bulged into the stock, crossed the
neighbouring offset arc, and the self-intersection scan skipped the
whole hole.  Fix: compute the corner turn from the base tangents; when
turn·d < 0, miter-trim to the crossing of the two offset curves
(new line-circle / circle-circle intersection helpers, nearest the
corner, capped at 20·|d| so near-tangent corners can't spike); corners
that turn away keep the round join (rolling ball).  Also fixed
`append_round_join` to use |d| as the join radius — a signed radius
mirrored the arc onto the wrong side for negative offsets (kerf_side
"outside" on holes, conventional cuts) — pre-existing latent bug.

Root cause 2 (double cuts): with all holes cut, a new warning surfaced
— "36 selected profile(s) duplicate holes of other regions".  The
whole-sketch capture is supposed to skip standalone regions that
duplicate another region's holes, but `matches_hole` compared
sqrt(area/π) against the hole loop's mean point radius — for
non-circular holes (the user's lens-shaped petals) the two disagree
wildly, so every petal was captured twice (the saved op has
geometry_scope=sketch, 37 machining regions) and the generator traced
each petal a second time with the kerf offset on the WRONG side (into
the stock).  Fixes:
- generate-time: for sketch-scoped ops, drop the standalone outer and
  keep the hole cut ("...they are cut once" warning); explicitly
  selected regions keep the cut-both design (nested separate parts,
  Test 20 locks it) with the "may be cut twice" warning.
- capture-time: `matches_hole` now matches centroid + bounding box
  (the bbox is the discriminator for non-circular holes) so future
  whole-sketch captures don't record holes twice.

Tests:
- cam2d_test Test 14 (sharp arc corners miter-trim): lens hole (arc
  corners) — inward offset must not self-intersect, no join arcs at
  the tips, stays inside the lens; outward offset keeps 2 join arcs,
  stays outside; PLUS the user's exact wheel-hole literals offset
  cleanly at kerf.  Fail-before verified empirically via the scratch
  (SELF-INTERSECTS on the exact hole-4 arcs with the old code).
- cam_generators_test Test 68 (laser duplicate holes cut once):
  rect + circle, whole-sketch-scope capture of both → hole cut at
  9.9 exactly once, no 10.1 duplicate, "cut once" warning.
- cam_generators_test Test 69 (whole-sketch capture dedupes lens
  holes): rect + 2-arc lens → capture_profile_references_from_sketch
  keeps exactly ONE region (fail-before: 2 — the old radius heuristic
  misses lenses; real-world fail-before: the user's saved op with 37
  regions).

Gates: **49/49 C++ suites PASS**.  Scratch on the user's saved
project: 40/40 holes cut (was 4/40), warnings 108 → 73 (remaining are
pierce-placement notes: sharp arc corners pierce mid-segment),
toolpath 440 moves (each petal cut exactly once), extrude still
healthy.

Remaining pierce notes ("N sharp corner(s) excluded... Every corner is
sharper than the pierce threshold") fire one line per loop — dedupe
into a single summary line is a candidate polish, not scheduled.

Next session checklist:
- Await user in-app verification: regenerate the laser gcode → all 40
  holes present, no skip warnings, no double cuts; then commit with
  approval (no Co-Authored-By) — scratch_touch_analysis.cpp + its
  CMake entry must be removed first.

Round 32 addendum (pierce fallback + warning dedupe, same session):
the remaining pierce-note flood (72 lines) is now collapsed.  Two
changes: (1) the all-corners-sharp fallback scans ALL segments for
the longest edge and pierces mid-segment (arcs included — arc length
via offset_arc_sweep, mid at startAngle + sweep/2) — the old code
skipped arcs and fell through to the nearest corner vertex while the
message claimed "longest straight edge"; copy now says "longest edge".
(2) laser_generate collapses identical repeated warnings into one
counted line " (×N)" at the end (per-loop pierce notes otherwise
flood the Logs).  Tests: cam_generators Test 70 (two petal holes —
mid-arc pierce on the r=55 offset circle clear of all corners,
fallback + sharp-count warnings each appear once with ×2, no
"straight edge" copy).  Gates: 49/49 suites PASS; user's saved
project now emits FOUR warning lines total (dedupe note, 6-sharp ×12,
8-sharp ×24, every-corner ×36), 40/40 holes cut, extrude healthy.
Note: the app was running during the final rebuild — cad_core.exe
copy hit the known MSB3073 lock (harmless; test exes all rebuilt) —
the user must restart the app to pick up the pierce/dedupe core.
---
## Join-arc snap round (2026-09-13, after analyzing untitled-part2.nc)

The user regenerated: **untitled-part2.nc (1877 lines) vs
untitled-part.nc (3489)** — the face path + cleanup halved the file. File
analysis found the remaining noise: every G1 is followed by a **2-micron
G3 with radius 0.075 (= kerf/2) and 1.5–2° sweep** — degenerate
round-join arcs from `append_round_join`. They fire at REFLEX facet
corners where the miter crossing (distance ≈ d/sin θ) lands beyond the
short segment ends; convex corners miter at the corner point (t ≈ 1).
332 such G3s in part2.

Fix: `offset_closed_loop` now snaps the corner (current.end =
nextOffset.start) when |sweep| < 5° (kJoinSnapSweepRad) — the arc's
sagitta there is ~6 µm, far below the 150 µm kerf. Tests: cam2d Test 14
(reflex notch fixture, both sides of 5°: dy 0.02 → snap / dy 0.0225 →
one join in [5°,6°]) + cam_generators 8d (120-gon → ZERO arcs, pre-fix
it carried 120 degenerate joins). All 46 suites green.

Remaining in part2 after this fix (≈1545 lines): ~950 facet G1s ≥1 mm
(the mesh's own resolution) + ~320 micro-facets <0.2 mm at tight corners
(real model geometry). Optional next step: Douglas-Peucker polyline
simplification of the base loop at ~0.02–0.05 mm deviation.

## Douglas-Peucker round (2026-09-13, this machine, uncommitted)

- `cam2d.h/.cpp`: `simplify_polyline_dp(segments, epsilon)` — recursive
  DP over maximal runs of consecutive chained LINES; arcs are exact
  anchors (copied untouched, bound each run, so loop closure survives).
  A vertex drops only when its whole span lies within epsilon of the
  replacement edge (segment distance, `xy_point_segment_distance`).
  All-line loops form one cyclic run (closing edge re-emitted).
  `SimplifyStats { vertices_before, vertices_after }`.
- `laser/laser_generate.cpp`: wired into `plan_loop` after
  `cleanup_base_segments`, before the kerf offset — one choke point for
  profile outers, holes, AND face wires. `kDpSimplifyEpsilonMm = 0.03`
  (well below kerf/2 = 0.075 and arc_tolerance 0.05). Log line:
  "contour simplify: N points -> M (max deviation 0.03 mm)" (cam_laser).
- Tests: cam2d Test 15 (collinear chain collapses; both epsilon signs of
  a bulge vertex; arcs anchor runs + connectivity; closed all-line loop
  keeps corners/closure; zero epsilon = no-op). cam_generators 8d pin
  updated: 120-gon at r = 20 now posts `lines < facets` (pre-DP: one
  line per facet) and keeps ≥ facets/2 — the DP metric is the 2-facet
  span deviation r·(1−cos 3°) ≈ 0.027 mm < 0.03 mm at r = 20 (at the
  old r = 50 it was 0.069 mm and nothing simplified).
- **Join-snap threshold raised 5° → 10°** (kJoinSnapSweepRad, cam2d.cpp).
  WHY: DP re-spaces the surviving facet corners near 2·acos(1−eps/r) —
  ~6.3° at r = 20, eps = 0.03 — so the post-DP corners escaped the old
  5° snap and the degenerate-join noise class returned (8d saw 46 arcs
  at 6°). At 10° the snapped join's sagitta at kerf scale is still
  < 0.3 µm. Tests re-pinned: cam2d Test 14 (both sides of 10° — snap at
  θ ≈ 8.0°, dy 0.035, d 0.075; join kept at θ ≈ 11.4°, dy 0.05, d 0.15
  — d must be large enough that the miter crossing still lands beyond
  the 0.5 mm legs, else the corner miters instead of joining), Test 4
  (the 5.8° convex corner now snaps: 4 joins + 6 lines). All 46 suites
  green.

**User verification on the machine (binding):** RESTART the app (a
running `pnpm dev` locks cad_core.exe — MSB3073), select the mesh body's
top face → 2D Cut → Generate → check the Logs panel for the cleanup
counts → export (untitled-part3) + burn. Acceptance: no G3 noise, GRBL
runs it clean. With the DP round: regenerate (untitled-part4), expect
"contour simplify: N -> M (max deviation 0.03 mm)" in the Logs panel —
the acceptance test for the lost-steps/noise problem. After the 4.1.0
flash: re-upload the config (above), rejoin WiFi, re-zero XY, then burn.

## Machine state (all hardware verified in-hand)

- Old board MKS DLC32 (GRBL 1.1h) replaced by **MKS LS ESP32 PRO V2.1_002**,
  mainline FluidNC esp32s3-wifi, WiFi STA `192.168.1.19`. Was v4.0.3;
  **user is flashing 4.1.0 now (Timed-engine bug fixes)** — the flash
  erases the config, so re-upload after flashing, AS `/config.yaml`
  (the fresh build boots the default name; the old board used an
  alternate `$Config/Filename`):
  ```bash
  curl -F "path=/" -F "/config.yamlS=2234" -F "myfile[]=@C:/Users/PC/grbl_tools/laser-board.yaml;filename=/config.yaml" http://192.168.1.19/files
  ```
  Engine: the X/Y step pins (gpio.16/15, gpio.7/6) ARE RMT-routable on
  the S3 (RMT goes through the GPIO matrix — pins are NOT the problem).
  The blocker is firmware: FluidNC S3 RMT support was added in PR #1622,
  reverted in PR #1792, re-added in current rmt_engine.c — but the
  official builds still don't compile it. **Verified on the flashed
  4.1.0 (esp32s3-wifi, 2026-09-13): config.yaml with `engine: RMT`
  (2232 B) boots with the merged `$CD` dump showing `engine: Timed` —
  silent fallback, same as I2S_STREAM on 4.0.3. Runtime set also
  rejected ("Runtime setting of step_engine objects is not supported").
  RMT is dead on this board until someone compiles a custom S3 build.
  Final stack: `engine: Timed` + 4.1.0's Timed fixes + DP-simplified
  gcode.**
- Wiring: fully plug-and-play (DLC32 V2.1 shares XH connectors). Exceptions:
  - Dual-Y gantry: old PCB mirrored the Y2 pins, LS does NOT → swap BOTH
    phase pairs (A↔B) on ONE Y motor plug (verified working).
  - 2-pin power-switch port next to the DC jack must be jumpered (installed).
  - SPREAD jumpers ON (SpreadCycle — audible hum is normal).
  - No limit switches on this machine; Zero XY at the part corner before run.
- Live config: `C:\Users\PC\grbl_tools\laser-board.yaml` (uploaded to board):
  - `engine: Timed` — on v4.0.3, **I2S_STATIC caused "Configuration is
    invalid" error:152 on the S3 build; the I2S engines target the
    I2SO shift-register architecture and RMT needs physical pins this
    board doesn't wire — Timed is the only engine, on any version**.
    Runtime switch: `$X` then `$/Stepping/Engine=Timed` (case-sensitive).
  - X step/dir gpio.16/15, Y gpio.7/**6:low** (`:low` = direction invert —
    FluidNC inverts direction via the pin attribute, not a stepstick field),
    limits gpio.39/40 (unused), laser gpio.2, 80 steps/mm, 6000 feed,
    500 accel, `junction_deviation_mm: 0.03`, `arc_tolerance_mm: 0.05`,
    `planner_blocks: 60` (all tuned 2026-09-13, not yet proven by a full cut).
- HTTP upload protocol that works: `POST /files` multipart with `path=/`,
  `/<name>S=<size>`, `myfile[]=@<win-path>;filename=/<name>` (curl must use
  a Windows path — `/tmp` breaks; filename override is REQUIRED or the file
  lands under the local basename).

## First-cut failures (the open problem)

The job (`res/untitled-part.nc`, 3490 raw / 1875 filtered lines, 830 tiny
1°-step arcs) does not complete on ANY transport:

| Path | Failure |
|---|---|
| App USB (CH340, 115200) | FluidNC **error 36** "no offsets in plane" at ~line 468 — I/J tail of the arc line dropped. Old GRBL board had error 1 at the SAME line (same cause). |
| App TCP :23 | Board drops the connection at ~filtered line 862 (reproduced in check mode with the app's exact 127-byte window accounting). Board survives. |
| App WS :80 | Dies within the first ~3 holes. |
| LaserGRBL USB | Died ~raw line 1700 ("board died" = connection drop; board survives). |

Working theory, two stacked causes:
1. **CH340 USB has no flow control** (tiny FIFO) — 127-byte bursts drop bytes
   mid-line → the clean parse errors (36/1) at a repeatable spot.
2. **The dense file overwhelms FluidNC v4.0.3** (1° arc steps × tight
   tolerances → planner churn; user saw jerky motion right before failures).
   Connection drops at varying lines per transport; check-mode replay
   reproduced it over TCP.

Fixes applied so far: tolerance/planner tuning (above). Not yet verified.
**Discriminator test for the other station:** upload the .nc to the board FS
and run `$SD/Run=untitled-part.nc` (no host streaming). Completes → host
transports are the problem; dies → file/firmware. Also: stream a trivial
30-line square over each transport; try a FluidNC build newer than v4.0.3.

**Safety gap:** when a connection dies mid-job the board KEEPS CUTTING
(buffered lines + laser on). Hit Reset immediately. App bug: on link
EOF/error while a job is in flight, `gcode_sender.rs` run() just stops
reading — no abort, no loud error event. Fix alongside the React bug.

## React bug found 2026-09-13 — "Maximum update depth exceeded" (TCP)

- `apps/desktop-ui/src/app/GrblWorkspace.tsx` ~line 474:
  ```tsx
  embeddedProgram={
    loadedProgram?.source === "internal"
      ? { text: loadedProgram.text, label: loadedProgram.label }  // NEW OBJECT EVERY RENDER
      : embeddedProgram
  }
  ```
- `apps/desktop-ui/src/layout/CamGrblPanel.tsx` line 254:
  ```tsx
  useEffect(() => { if (embeddedProgram) setLoadedProgram(embeddedProgram); },
    [embeddedProgram]);
  ```
- Object identity changes every render → effect → setState → render → loop.
  TCP makes it hot: 5 Hz status events re-render the workspace.
- **Fix:** wrap the object in `useMemo(..., [loadedProgram, embeddedProgram])`
  in GrblWorkspace (canonical), or compare by content in the panel effect.

## Next-session checklist

1. Fix the React loop (useMemo) — verify by connecting TCP and watching for
   the warning to disappear.
2. Fix the connection-loss safety gap in gcode_sender.rs (emit error + abort).
3. Run the discriminator tests on the other station ($SD/Run, simple square,
   newer FluidNC).
4. CAM-side optimization (the real cam/laser-testing work): bigger arc spans
   (5–10°), merge collinear G1s, dedupe the 36 duplicate holes — a leaner
   file reduces load on every transport and fixes the jerky section.
5. First completed cut → calibration check (jog 100 mm vs ruler), laser power
   curve, then commit CAM changes with test coverage.

## Key files

- `apps/desktop-ui/src-tauri/src/gcode_sender.rs` — worker, ByteWindow(127),
  500 ms `?` poll, EOF handling gap
- `apps/desktop-ui/src/app/GrblWorkspace.tsx` — embeddedProgram loop source
- `apps/desktop-ui/src/layout/CamGrblPanel.tsx` — loop consumer (effect 254)
- `C:\Users\PC\grbl_tools\laser-board.yaml` — machine config (mirror of board)
- `res/untitled-part.nc` — the problem job (bounds 0..232.7 × 0..172.4)
---
# Active task: STEP import viewport perf — regression fix (2026-09-16)

> **Branch:** `step-import-viewport-perf` (checked out from origin — pushed
> from the other workstation). dev + 1 commit (873ff65, seam-analysis rewrite
> + step-2 pick threshold). Working tree clean on arrival.
> **Round 1 (this machine): oversized-import regression FIXED — decimated
> per-face pick proxies + projection routing. UNCOMMITTED; pending user
> in-app verification.**

## Round 1 — decimated face proxies + projection (2026-09-16)

Regression from step 2 (oversized imports > 8000 edges emitted NO face
pick entries): sketch-on-face placement and the Project tool on the board
both broke; both worked after step 1 (user-confirmed on the other station).

Fixes:
- Core `viewport.cpp` — oversized imports again call `enumerate_body_faces`,
  passing `decimate_pick_faces=oversized_import`; per-edge/per-vertex
  entries stay skipped (the 78k-line / 156k-sprite flood stays out).
- Core `body_face_helpers.inc` — `decimate_pick_faces` joins the
  `mesh_to_body` decimation condition (faces > 48 triangles only, small
  faces ship full). Planar faces emit the centroid fan (boundary strided
  to the 48 budget); NON-planar faces fall back to the strided subset
  (a fan through a curved surface's outer wire spans one cross-section
  and misses picks). Face ids + plane frames + surface kinds flow, so
  sketch-on-face / face selection / face features work again.
- UI `viewportFaceSelection.ts` — body-id clicks now project
  `step_import`/`iges_import` bodies (section/silhouette) like
  mesh imports.
- Tests `viewport_seam_enumeration_test` test 4 REWRITTEN:
  `test_oversized_import_decimated_face_proxies` — 900 boxes + cylinder
  + 100-gon prism compound (11103 edges): 5505 face proxies, every
  proxy ≤ 48 triangles, planar capped fan present, cylinder wall keeps
  surface_kind + radius witness. Verified FAIL-BEFORE (old gate → 0
  faces → suite red) / pass-after.

Gates: `pnpm core:build` + **50/50 suites** + `tsc --noEmit` clean.

Next-session checklist:
1. **In-app verification (binding, before any commit):** import the real
   PCB board STEP → (a) start a sketch on a board face, (b) Project tool
   → silhouette/section projection works, (c) payload stays bounded
   (Logs panel viewport event size; expect ~20 MB, not 47.5 MB), (d)
   small STEP imports unchanged (full face/edge/vertex picking).
2. Commit after the user confirms (no Co-Authored-By trailer).

## Key files (this branch)

- `native/cad-core/src/core/viewport/viewport.cpp` — oversized gate +
  decimate flag pass-through
- `native/cad-core/src/core/viewport/impl/body_face_helpers.inc` —
  fan/strided decimation, boundary cap, planar gate
- `apps/desktop-ui/src/app/viewportFaceSelection.ts` — step/iges
  projection routing
- `native/cad-core/tests/viewport_seam_enumeration_test.cpp` — test 4
- `STEP_IMPORT_VIEWPORT_PERF.md` — branch work notes (regression section
  updated)
---
# Active task: select-tool stale-delete race + marquee flood (2026-09-16)

> **Branch:** `fix/select-stale-delete` (from dev @ 4040c64). UNCOMMITTED;
> user verified the fixes in-app (right-click delete, sketch-point
> selector, circle dimension doubling) — commit awaits user approval.

## Round 7 — circle dimension doubling: the REAL root cause (2026-09-17)

Round 6's fix (remove the core's numeric /2) did NOT resolve the
doubling — "editing the circle dia alway double the real value".
TEMP DIAG stacks (dim_diag in useCadCore + Logs panel) showed the
cancel path (Escape) sending 10 → 20 → 40, one doubling per round.

Real root cause (code-proven): the document payload EMITS the
displayed value for circle_radius dims —
feature_to_payload_sketch_dimension_entries.inc multiplies value×2
when display_as != "radius" ("the payload carries the displayed
diameter", parser comment) — but the UI treated the payload value as
the RADIUS (dimensionValueDisplay.ts ×2//2 pair + cancel re-sending
originalValue.value). Removing the handler /2 (Round 6) left NO
conversion anywhere: payload diameter → UI → radius → grow ×2 per
round-trip. The pre-existing UI/payload disagreement also explains
the popup showing 4× the radius and the "mixed radius and dia" chaos.

Fix — ONE convention everywhere: IPC `value` = the DISPLAYED value.
- app/impl/sketch_dimension_update_command_handlers.inc: numeric
  branch halves circle_radius values again, gated on
  display_as != "radius" (Round 6's unconditional-removal reverted;
  the OLD unconditional /2 was itself the radius-mode bug). String
  branch: same display-mode gate.
- core/sketch/impl/dimension_expression_reify.inc: expression
  re-evaluation halves only in diameter mode (was unconditional).
- UI dimensionValueDisplay.ts: dimensionDisplayValue /
  dimensionCoreValue pass circle values through (doc value = the
  displayed value); only angle conversions remain.
- wiki/AI-CAD-Command-Language.md rewritten: value = displayed value
  (diameter by default, radius when display_as == "radius").

Regression tests (dimension_completion_test.cpp, +2):
test_circle_dimension_payload_carries_displayed_value (payload emits
×2 in dia mode, raw in radius mode, toggle round-trip) and
test_circle_expression_reify_respects_display_mode (dia-mode
expression → radius d/2; radius-mode → no halving — fails without the
reify gate). All 50/50 suites pass; tsc clean.

User verified in-app: "well looks like now it is working".

LESSON (hard): a vite dev serve IS type-stripped output + injected
preamble — the "June-era stale file" theory was a misread of esbuild
type-stripping (the inline source map's sourcesContent proved the
source was current). Before declaring stale serving, decode the
source map. Also: the temp dim_diag blocks in useCadCore.ts and
App.tsx are REMOVED.

## Round 6 — circle dimension radius/diameter double conversion (2026-09-17)

User: "the circle dimension ... has mixed radius and dia option and
when I edit the dimension either take radius instead of dia or the
other way around... during creation of the lower right one I
introduced 3.2 dia and it result 2 times smaller. I had to introduce
6.4 or something to make it 3.2."

Evidence from the saved file (laser board3.polysmith): circles all
r=1.6 (dia 3.2) while dim-circle-circle-138/-152 store value 3.2 with
display_as '' — the label renders value*2 = 6.4.

Root cause (code-proven): the `update_sketch_dimension` IPC handler
(app/impl/sketch_dimension_update_command_handlers.inc) divided
circle_radius NUMERIC values by 2 — but the UI's dimension editor
already converts display (diameter) → core (radius) before sending
(dimensionValueDisplay.ts dimensionCoreValue). Typed diameter was
divided twice (UI /2 then core /2 → radius = typed/4): typed 3.2 →
r 0.8 ("2 times smaller"), typed 6.4 → r 1.6 (the user's workaround).
Cancel also re-sent the stored radius and got halved. The core value
of a circle_radius dimension IS the radius (the solver enforces
radius == value).

Fix: the numeric branch no longer converts (comment explains); the
STRING expression branch keeps its /2 (expressions are authored as
diameters — the circle draft field is a diameter — and
reify_dimension_expressions.inc divides consistently).
Docs: wiki/AI-CAD-Command-Language.md — numeric value = radius
passed through; string expression = diameter.

Gates: core rebuild + 50/50 suites + tsc clean. In-app verification
(binding): edit a circle dim in diameter mode — typed 3.2 → dia 3.2
(label shows 3.2); radius mode — typed 3.2 → r 3.2; Escape/cancel
keeps the current radius; the stale 3.2-value dims in the saved file
retype cleanly (one edit re-syncs value = radius).

CORRECTED by Round 7 (below): this analysis was wrong — the UI/payload
convention disagreement was the real cause, and the Round-6 fix
doubled every Escape/Enter round-trip until Round 7 closed the loop.

## Round 5 — right-click select regression + sketch-point selector (2026-09-17)

User: "it works but now started to delete the lines again like
before" after Round 4. Cause: the Round-4 right-click replace-select
— a right-click on the SURFACE between the marquee'd circles flipped
the selection to the profile and the menu Delete killed the
perimeter. Fix: right-click opens the menu WITHOUT changing the
selection (opening a menu is not a selection action); the menu's
Delete always deletes the live selection; only when NOTHING is
selected does it first select the single clicked item (entity /
sketch point via select_sketch_point / profile) and then delete —
no snapshot ever rides the delete.

Second bug caught by the user ("Malformed vertex id: vertex-204"):
the fallback routed sketch vertex ids to the 3D select_vertex
("<body>:vertex:<n>" format). Fixed with a NEW onSelectSketchPoint
prop (select_sketch_vertex) wired App → ViewportPanel → menu actions
(viewportPanelTypes, ViewportPanel, viewportContextMenuActions).

User verified: "now it works". Fillet investigation followed
(separate — see Future work): the fillet rejection was correct
geometry (1 mm stubs cannot host r=2); after the user redrew the
corner it fit (max 2.034 mm).

## Future work recorded (2026-09-17, user-requested documentation)

1. **"Unify lines" tool** — merge collinear adjacent line segments
   (and co-circular arcs) into single entities. Motivation: STL/STEP
   projections and trims fragment contours into many small lines;
   the CAM cleanup already does this at generate time
   (cleanup_base_segments, cam2d) but the SKETCH keeps the fragments.
   Sketch-level merge must handle constraints/dimensions on the
   consumed segments + weld the joint vertices (same class of work
   as the zero-length cleanup in refresh_zero_length_line_cleanup.inc).
2. **Fusion-style consume-short-line fillet** — when one line is
   shorter than the fillet's trim distance, Fusion consumes it: the
   short line is deleted, the arc takes over to its far endpoint
   (welded onto the far vertex), the long line is trimmed. Needs:
   consume detection, constraint/dimension sweep on the consumed
   line, arc-endpoint weld to the far vertex id, fillet record
   remembering the consumed line for delete/undo restore (new
   optional payload field — forward-compatible), regression tests.
   NOTE: naive "allow trim == line length" is NOT cheap — the
   zero-length cleanup deletes the line AND the new fillet record
   (refresh_zero_length_line_cleanup.inc erases fillets referencing
   the deleted line). Until then: redraw short lines longer
   (accepted user discipline).
3. Zero-length / micro-line handling in sketches — generally fragile
   for CAD; to be addressed with the two features above.

## Round 4 — the user's selection rule, enforced everywhere (2026-09-16)

User: "the clicked-item snapshot only applies when nothing is
selected. I think this is stupid. Any selection action without Ctrl
should remove all the other items selected. I think this is the flaw
that we have combined with items that remain in state of selected.
I keep asking you to get rid of that phenomenon." — the recorded
Round 17 FIRST-PRIORITY semantics task, now implemented.

Changes:
- Core draw/trim/extend/update commands NO LONGER leave the new/
  affected entity selected (sketch_basic_entity_commands.inc 6 sites +
  sketch_circle_polygon_commands.inc circle+polygon +
  update_sketch_slot + sketch_trim_commands.inc +
  sketch_extend_commands.inc + sketch_update_geometry_commands.inc).
  The AUTO DIMENSION selection stays (line/rectangle/circle) so the
  dim editor still auto-opens.
- Core delete resolver: PLURAL lists win over the SINGULAR echo ids
  (the stale last-drawn entity used to join the delete set — "Delete
  on 2 clicked arcs deleted 9"). Singular falls back only when the
  plural list is empty. UI mirror in sketchSelectionDelete.ts
  (dedupeSelectedIds).
- UI right-click = a selection action: handleContextMenu replace-
  selects a single clicked sketch item when no Ctrl/Cmd/Shift is
  held, so the context menu's Delete ALWAYS acts on the live
  selection — the snapshot conditional (user: "stupid") is REMOVED
  (viewportContextMenuActions.deleteSketchSelection passes null
  unconditionally).
- Marquee additive: Ctrl/Cmd now also additive (was Shift only).
- Tests: test_draw_leaves_nothing_selected (line keeps the auto dim,
  circle keeps none/its auto dim) + test_delete_ignores_stale_singular
  _when_plural_present (via the load path). test_load_prunes_orphans
  re-pinned (explicit click-select instead of the draw auto-select).

Gates: core build + **50/50 suites** + tsc clean.

Next-session checklist:
1. **In-app verification (binding):** draw a line -> nothing stays
   highlighted; draw a rectangle -> no lingering selection; click an
   entity -> replaces; Ctrl-click -> adds; right-click an unselected
   entity -> it becomes selected, menu Delete removes the selection;
   marquee -> replace (Ctrl = add); Delete after drawing deletes
   nothing (nothing selected).
2. Commit after the user confirms (no Co-Authored-By trailer).

## Round 3 — the REAL bug: context-menu delete on the surface (2026-09-16)

User: "nothing changed" after round 2 (verified rebuild + restart).
The new rect command's select_diag logs during the reproduction
(07:51) showed the marquee selecting circles correctly and the delete
resolving ONLY circles — IT WORKS. The decisive delta was the
context-menu fix: with a live selection, the menu's Delete now passes
null (core resolves the selection at command time) instead of the
right-click snapshot. The user's right-click landed on the SURFACE
between the marquee'd circles — the snapshot carried the PROFILE, so
the delete removed the outside contour while the circles survived.
One-by-one worked because the right-click was ON the selected circle.

Fixes this round:
- viewportContextMenuActions.deleteSketchSelection: selection-aware —
  live selection -> null (empty ids); nothing selected -> the clicked
  item's snapshot. Ref type widened to accept null.
- TEMP DIAG select_diag logs REMOVED after the confirmation.

Gates: core build (compile + test exes; final copy blocked by the
running app — completes on the user's next core:rebuild) + **50/50
suites** + tsc clean. User in-app verification: "now it works" with
two marquee+delete cycles in the logs (circles only, profiles=0).

Next-session checklist:
1. Commit after the user confirms (no Co-Authored-By trailer).
2. Optional follow-up: the rect selection could skip construction
   circles too if the user ever complains.

## Round 2 — marquee selection moved INTO the core (2026-09-16)

Round 1 (batch command + Delete-during-drag guard) did NOT fix the
user's repro: the wrong delete persisted even though the marquee was
"done long time ago" — the bug was inside the multi-selection itself,
not a race. Core-side replay of every variant on the saved file (laser
board2.polysmith: 10 lines / 16 arcs / 150 circles / 155 profiles)
was CORRECT each time — the failure had to be in the UI's screen-space
marquee collection (stale scene data / projection math).

Fix: the marquee now resolves CORE-side.
- Core NEW command `select_sketch_rect { x1, y1, x2, y2, window_mode,
  additive? }` (DocumentManager::select_sketch_rect): sketch-local
  corners + the screen drag direction; window mode = fully inside,
  crossing mode = touching. Lines/arcs/ellipses skip construction;
  construction circles selectable (mirrors the old UI rules). Replaces
  or additive-toggles the entity selection, clears profile/vertex/
  dimension selections. Dispatch + commands.schema.json.
- UI `performRectangleSelect`: resolves BOTH drag corners to
  sketch-local coordinates via resolveSketchPlanePoint and sends the
  ONE rect command (no scene walk, no projection math). The old
  collectRectangleSelectionIds + its helpers are DELETED
  (selectionGeometry.ts now holds only the overlay + drag types).
  `batchSelectSketchEntities` hook removed (the select_sketch_entities
  core command + tests stay).
- Tests (cad_core_selection_test): `test_rect_select_window_and_crossing`
  (window mode picks only fully-inside circles; crossing mode adds the
  crossing divider) + `test_rect_select_then_delete_keeps_perimeter`
  (profile selected -> rect over circles -> empty delete removes the
  circles, perimeter survives — the user's exact regression).
- TEMP DIAG logs (select_diag) removed; scratch_board2_diag removed.
- Docs: wiki/IPC-Protocol.md + wiki/AI-CAD-Command-Language.md.

Gates: `pnpm core:build` + **50/50 suites** + `tsc --noEmit` clean.

Next-session checklist:
1. **In-app verification (binding, before any commit):** CLOSE the app,
   `pnpm dev` fresh; marquee over internal circles -> highlight
   near-instant; Delete -> circles gone, perimeter intact. Test both
   drag directions. One-by-one delete unchanged.
2. Commit after the user confirms (no Co-Authored-By trailer).
