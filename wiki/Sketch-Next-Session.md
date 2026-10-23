# Sketch — next session handoff

**Read this first, then the three documents it points at.** Written 2026-10-23 at the end of the session that fixed the extrude/sketch defects and started tool parity.

**Branch:** `feature/sketch-refactor` (local only, 7 commits ahead of `dev` @ `cfcae31`). Working tree clean.

---

## 1. What is done and verified

| Commit | What it covers |
|---|---|
| `87adf38` | **Extrude survives sketch edits.** The linked-extrude refresh replaced the whole parameter struct, resetting `side1` (body half thickness), `operation` (one body per profile) and dropping the boundary edges. Now copies the existing parameters and overwrites only profile-derived fields. Also: `update_extrude_parameters` no longer treats the payload's stripped (derived) geometry as authoritative; `update_extrude_profiles` clears the stale `dependency_broken`; re-editing reveals the source sketch and re-selects profiles; a re-picked selection clears the broken block so Confirm re-binds. |
| `24d0fbe` | **UI error capture.** `log_ui_error` → `%APPDATA%\polysmith\ui-errors.log` (survives the renderer), `open_devtools` on F12 / Ctrl+Shift+I, the error boundary writes the real `componentStack` and renders in dev instead of a blank div. Plus the two render crashes it exposed (`Path.closePath()` on an empty loop; `attributes.position.count` on an attribute-less BufferGeometry). |
| `c4bec67` | **Dispatch ↔ schema integrity.** Seven dispatched-but-unlisted commands added; unknown commands now answer with `UNKNOWN_COMMAND` carrying the real id; `pnpm lint:architecture` diffs the two in both directions. |
| `d685bf0` | **Docs**: completion plan, defect log, measurements. |
| `127b9c3` | **Parity for loft/revolve/sweep** edit sessions (same reveal treatment). Their core repair paths already cleared the flag. |
| `76f48b2` | **Rounded rectangle, core side.** `add_sketch_rounded_rectangle { start_x, start_y, end_x, end_y, radius }` — rectangle + four parametric fillets in one undo group; `tests/sketch_rounded_rectangle_test.cpp` passes. |

**Gates that were green:** `pnpm test:core` **74/75** (the red one is S0 below), `tsc --noEmit`, `pnpm lint:architecture` 3/3.

## 2. Read these, in this order

1. **[Sketch-Tools-Parity.md](Sketch-Tools-Parity.md)** — the SolidCraft↔PolySmith tool matrix (106 SolidCraft commands grouped, variant by variant), the T1/T2/T3 plan with effort, and a progress log. **This is the work to continue.**
2. **[Sketch-Defect-Log.md](Sketch-Defect-Log.md)** — D-001a–f, each with the reproduction and the fix; also the "behaviour confirmed as intended" section, so nothing there is re-litigated as a bug.
3. **[Sketch-Completion-Plan.md](Sketch-Completion-Plan.md)** — the broader sketch plan (solver coverage, reach, legibility). Tool parity is the tools half of the same story.

## 3. The immediate next step

**T1.3, UI half — the rounded rectangle cannot be used yet.** The core command exists and is tested; nothing calls it.

- `apps/desktop-ui/src/types/geometry/sketch.ts` — add `"rounded"` to `RectangleToolMode`.
- `apps/desktop-ui/src/layout/header/SketchToolbar.tsx` and `layout/viewport/SketchToolPanel.tsx` — the rectangle split button (4th mode) + i18n label in `i18n/en.json`.
- A radius field for the mode (the fillet panel is the precedent; the draft-dimension session is the other).
- The commit path: `layout/viewport/draftCommit.ts` (`activeSketchTool === "rectangle"` branch) and the click path in `layout/viewport/pointerUpActiveSketch.ts` — **both** must send `add_sketch_rounded_rectangle`, per the two-commit-paths rule in `Sketch-Tool-Implementation.md`.
- Builder in `lib/ipc/sketchCommands.ts`, payload type in `types/ipc/sketchCommands.ts`, hook in `hooks/useCadCore.ts`.

Then, in order: **T1.1** slot `center_point` + `overall` (arithmetic only), **T1.2** chamfer `distance_angle`, **T1.5** centerline/infinite line, **T1.4** point entity. After that **T2.1** fit-point spline, which is also where **S0** should be fixed (see below).

## 4. S0 — the one red suite

`cad_core_spline_test` fails deterministically at `test_spline_extrude_smoke` with `Extrude preview failed`. Pre-existing (the suite landed 2026-08-28, no record of it passing since), unrelated to any commit above, and **folded into T2.1**: the fit-point spline work rewrites the spline→profile→extrude path this test exercises. Diagnose with `PS_TRACE_FACES=1`.

## 5. Environment notes that will waste your time otherwise

- **The app holds `native/cad-core/build/cad_core.exe`.** `pnpm core:build` then fails at the post-build copy with `MSB3073` — the compile and link succeed, only the copy fails. Close the app first, or ignore it and copy `Release/cad_core.exe` afterwards. A stale copy means you test the old core.
- **A render crash used to blank the window with no trace.** It no longer does: the message lands in `%APPDATA%\polysmith\ui-errors.log` and (dev builds) on screen. F12 opens the WebView devtools — Tauri binds neither by itself.
- **The extrude panel's input is the global profile selection.** Clicking a timeline item clears it (D-001f, known, not fixed). If a panel looks like it has "0 faces selected", that is why.
- Gates: `pnpm core:rebuild`, `pnpm test:core`, `pnpm --filter desktop-ui exec tsc --noEmit`, `pnpm lint:architecture`, `pnpm registry:write` after any command change.

## 6. Things deliberately not done

- Line tool parity — **explicitly out of scope**; PolySmith's is better than SolidCraft's.
- Arc slots (T2.3) — highest risk, touches `sketch_profile_exact.inc`.
- Conic, blend curves, G2/regular-polygon constraints, 3D sketch curves — T3.
- D-001f (timeline click clearing the selection) — recorded, wants a design fix rather than a patch.
- The completion plan's solver work (S1/S9: dimension kinds through planegcs) — separate effort, not started.
