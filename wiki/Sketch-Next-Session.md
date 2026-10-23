# Sketch — next session handoff

**Read this first, then the three documents it points at.** Written 2026-10-23 at the end of the session that shipped most of tool parity T1 and fixed S0.

**Branch:** `feature/sketch-refactor` (local only). The previous handoff's commits are on it; **this session's work is uncommitted in the working tree** (see §3) and needs review + an approved commit.

---

## 1. What is done and verified

| Item | State |
|---|---|
| **T1.3 rounded rectangle (UI)** | Toolbar fourth mode, panel radius field, both commit paths send `add_sketch_rounded_rectangle`, live rounded preview, auto side dimensions dropped on purpose. Tests: preview maths, click-commit, **real core through the real builder**. |
| **T1.1 slot `center_point` / `center_to_center` / `overall`** | One shared `slotDraftGeometry()` drives preview + commit. Split button + panel dropdown. Tests: mode arithmetic, `length >= 2r` across a span sweep, commit args. |
| **T1.2 chamfer `equal` / `two_distance` / `distance_angle`** | Core: `SketchChamfer.mode` + `angle_degrees`; distance along B is **derived from the angle on every recompute**. Update keeps the record's mode/angle when not supplied. UI: three-mode split button + a panel that shows only the fields the mode needs. Tests: four new C++ cases, a real-core IPC test, schema coverage. |
| **T1.5 centerline / infinite line** | `SketchLine.line_type` flag (`""` / `"centerline"` / `"infinite"`), both flavours force construction (so profile/dimension rules are unchanged), persisted + emitted in the viewport payload. UI: Line type dropdown replaces the Construction checkbox for the line tool; centerlines dash-dot, infinite lines drawn 100 000 mm past their endpoints (presentation only — scene bounds come from the core). Tests: new C++ suite, IPC round-trip, renderer rules. |
| **S0 (T2.1 first sub-step)** | `cad_core_spline_test` passes. Root cause: `load_document_from_path` never re-derived sketch-derived features after refreshing sketch profiles, so a reloaded extrude whose opaque profile snapshot was absent compiled with an empty profile (`BRep_API: command not done`). Fix: `refresh_linked_features_after_load`. |
| **D-003 — uncaught "sketch object vertex-N not found"** | The partial `sketch_entities_changed` reply was emitted for edits that change arcs/circles/fillets/etc., leaving the UI's document stale and the planegcs bridge throwing mid-drag. Guarded by `sketch_partial_event_safe`; the bridge now degrades to "solve failed"; a degenerate endpoint drop onto a connected neighbour is refused locally instead of provoking the core's zero-length error. See the defect log. |
| **D-004 — "Unsupported sketch tool: split"** | `corner` and `split` were missing from `kSupportedSketchTools()` while the toolbar could arm both, so every click on Split logged a core error and left the core's active tool stale. Added both; the C++ whitelist suite now asserts the canonical list covers the UI's whole `SketchTool` union, and an IPC test drives all 20 ids. |

**Gates, all green at the end of the session:** `pnpm test:core` **78/78**, `pnpm --filter desktop-ui exec tsc --noEmit`, `pnpm lint:architecture` 3/3, offline UI suites **94 passed / 5 skipped** (the two previously-red files are fixed), `pnpm registry:write` (no drift beyond the new optional chamfer keys).

## 2. Read these, in this order

1. **[Sketch-Tools-Parity.md](Sketch-Tools-Parity.md)** — the SolidCraft↔PolySmith matrix, the T1/T2/T3 plan, and the session-by-session progress log (§8).
2. **[Sketch-Defect-Log.md](Sketch-Defect-Log.md)** — D-001a–f and "behaviour confirmed as intended".
3. **[Sketch-Completion-Plan.md](Sketch-Completion-Plan.md)** — solver coverage, reach, legibility. Tool parity is the tools half.

## 3. What is left, in order

1. **T1.4 — standalone point entity.** The last T1 item and the only one with model risk.
   - `add_sketch_point { x, y }`, a point that participates in holes/patterns/constraints but never in profile loops.
   - The model precedent is the **projected point** (`SketchProjectedPoint`, minted by `project_vertex_into_sketch`): a standalone vertex that survives `rebuild_sketch_vertices`. Reuse that path rather than inventing a new entity kind, but do not mark user points `is_projected` (they must stay movable and unconstrained by `enforce_derived_points_fixed`).
   - The vertex table is **not** on the document_state wire (`include_vertices=false`): the UI reads points from `viewport_state.sketch_points`, so the point must be emitted there.
   - Ship it with a C++ suite asserting the complete profile set (a point alone produces none; a point inside a rectangle does not change it) and a real-core IPC test.
2. **T2.1 proper — fit-point spline (+ degree 5).**
   - `mode: "fit" | "control"` on `add_sketch_spline`; least-squares fit of a clamped B-spline through the clicked points (`spline_math.h` has the sampler/evaluator; `BRepBuilderAPI_MakeEdge`/`Geom_BSplineCurve` already consume the exact poles).
   - Raise the degree cap from 3 to 5 in `add_sketch_spline` and in the wire builder (it already takes `spline_degree` from the boundary edge).
   - UI: a third creation mode in the spline split button (there is no spline split button yet) + the draft preview fitting through the placed points.
   - S0 is already fixed; keep `cad_core_spline_test` green.

## 4. Gates

`pnpm core:rebuild`, `pnpm test:core` (must stay **77/77** — S0 included), `pnpm --filter desktop-ui exec tsc --noEmit`, `pnpm lint:architecture`, `pnpm registry:write` after any command change.

## 5. Environment notes that will waste your time otherwise

- **The app holds `native/cad-core/build/cad_core.exe`.** `pnpm core:build` then fails at the post-build copy with `MSB3073` — the compile and link succeed, only the copy fails. Close the app first, or ignore it and copy `Release/cad_core.exe` afterwards. A stale copy means you test the old core.
- **The zod schemas strip unknown keys.** Any new field in a core payload must be added to `apps/desktop-ui/src/lib/schemas/ipc/*` or it silently disappears at the UI boundary (this cost real time twice this session: chamfer `mode`, line `line_type`). Note string enums must include `""` when the core emits an empty string for "none".
- **`latestDocument()` can be stale after a sketch edit.** Edits that leave the dimension list unchanged answer with a partial `sketch_entities_changed` event; core-harness tests must send `get_document_state` before reading the document. Copy the pattern in `apps/desktop-ui/src/lib/ipc/sketchLineType.core.test.ts`.
- **The core harness is the fastest end-to-end test available.** `apps/desktop-ui/src/lib/ipc/*.core.test.ts` drive the real `cad_core` binary through the real TS command builders; they skip when the binary is missing. `pnpm --filter desktop-ui exec vitest run <file>`.
- **The partial sketch reply is guarded (D-003).** `sketch_entities_changed` carries only line endpoints and vertex coordinates; `sketch_partial_event_safe` sends the full `document_state` for anything else. Do not assume the partial event is available for a new command — see D-003 in the defect log.
- A render crash used to blank the window with no trace; it now logs to `%APPDATA%\polysmith\ui-errors.log` and F12 opens the WebView devtools.
- The extrude panel's input is the global profile selection (D-001f, known, not fixed).

## 6. Things deliberately not done

- Line tool parity — **explicitly out of scope**; PolySmith's is better than SolidCraft's.
- Arc slots (T2.3) — highest risk, touches `sketch_profile_exact.inc`.
- Conic, blend curves, G2/regular-polygon constraints, 3D sketch curves — T3.
- D-001f (timeline click clearing the selection) — recorded, wants a design fix rather than a patch.
- The completion plan's solver work (S1/S9) — separate effort, not started.
