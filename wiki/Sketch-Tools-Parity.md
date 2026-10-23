# Sketch Tool Parity — SolidCraft vs PolySmith, and the implementation plan

**Date:** 2026-10-23 · **Branch:** `feature/sketch-refactor`
**Sources:** SolidCraft's own built registry (`solidcraft-cli.exe commands` → 106 `sketch.*` entries, with each command's own parameter documentation) and PolySmith's toolbar (`layout/header/SketchToolbar.tsx`) plus its command reference.

PolySmith's **line tool is better than SolidCraft's** (chained segments, double-click to break the chain, H/V inference, draft dimensions, endpoint drag with solver preview). It is explicitly out of scope here — the goal is to reach parity on the *other* families without touching it.

---

## 1. SolidCraft's sketch catalogue (grouped)

**Create — geometry (22):** `line`, `line.midpoint`, `point`, `rectangle.two_point`, `rectangle.center`, `rectangle.three_point`, `rectangle.rounded`, `circle.center`, `circle.two_point`, `circle.three_point`, `circle.two_tangent`, `circle.three_tangent`, `arc.three_point`, `arc.center_point`, `arc.tangent`, `polygon.inscribed`, `polygon.circumscribed`, `polygon.edge`, `slot.center_to_center`, `slot.center_point`, `slot.overall`, `slot.arc_three_point`, `slot.arc_center`, `spline.fit_point`, `spline.control_point`, `spline.control_point_5`, `ellipse`, `conic`, `centerline`, `infinite`, `text`.

**Create — reference/projection (10):** `project`, `project_to_surface`, `intersection_curve`, `isoparametric_curve`, `spun_profile`, `include_3d`, `intersect`, `fit_curves_to_section`, `insert_dxf`, `insert_svg`.

**Modify (22):** `trim`, `trim.path`, `extend`, `break`, `cut`, `fillet`, `chamfer.equal_distance`, `chamfer.distance_angle`, `chamfer.two_distance`, `offset`, `offset.break`, `mirror`, `pattern.circular`, `pattern.rectangular`, `move`, `scale`, `blend_curve`, `break_link`, `delete`, `drag`, `drag_end`, `move_point`, `redefine`, `export_dxf`, `edit_text`.

**Constraints (16):** `coincident`, `collinear`, `concentric`, `curvature`, `equal`, `fix`, `horizontal_vertical`, `midpoint`, `parallel`, `perpendicular`, `polygon`, `symmetry`, `tangent`, `auto_constrain`, `auto_constrain_from_datum`, `finish_auto_constrain`, `constrainer`.

**Dimensions & diagnostics (12):** `dimension`, `dimension_text`, `toggle_driven`, `glyphs`, `inspect`, `solve`, `snap`, `loose_ends`, `loose_ends.tie`, `chain`, `options`, `finish`.

Plus lifecycle: `create`, `edit`, `finish`, `copy`, `cut`, `paste`, `delete`, and the 3D-sketch group (`line3d`, `spline3d`, `point3d`, `move3d`).

---

## 2. PolySmith today

**19 toolbar tools:** select, line, dimension, rectangle, circle, polygon, arc, fillet, chamfer, corner, extend, split, offset, ellipse, slot, spline, text, trim, project (7 with hotkeys).

**Creation modes already implemented** (split-button menus in `SketchToolbar.tsx` / `SketchToolPanel.tsx`):

| Family | PolySmith modes |
|---|---|
| Rectangle | `corner_corner`, `center_point`, `three_point` |
| Circle | `center_radius`, `two_point`, `three_point`, **`tangent_two_lines`**, **`three_tangent`** (both implemented — a stale comment says otherwise) |
| Arc | `three_point`, `center_start_end` |
| Polygon | `inscribed`, `circumscribed`, `edge` |
| Slot | `center_to_center` (one) |
| Spline | control-point only, degree ≤ 3 |
| Dimension | `auto` functional; 6 more sketch modes fall through |

---

## 3. The matrix

Verdicts: **=** parity · **+** PolySmith better · **−** missing · **~** partial · **×** out of scope (3D/surface).

### 3.1 Create

| SolidCraft variant | PolySmith | Verdict |
|---|---|---|
| `line` (+ midpoint variant) | line | **+** better; untouched by request |
| `rectangle.two_point` / `.center` / `.three_point` | same three | **=** |
| **`rectangle.rounded`** | — | **−** |
| `circle.center` / `.two_point` / `.three_point` | same | **=** |
| `circle.two_tangent` / `.three_tangent` | implemented | **=** |
| `arc.three_point` / `.center_point` | same | **=** |
| **`arc.tangent`** | — | **−** |
| `polygon.inscribed` / `.circumscribed` / `.edge` | same | **=** |
| `slot.center_to_center` | ✅ | **=** |
| **`slot.center_point`**, **`slot.overall`** | — | **−** |
| **`slot.arc_three_point`**, **`slot.arc_center`** | — | **−** |
| **`spline.fit_point`** | — (control points only) | **−** |
| `spline.control_point` | ✅ | **=** |
| **`spline.control_point_5`** (degree 5) | degree capped at 3 | **−** |
| `ellipse` | ✅ | **=** |
| **`conic`** | — | **−** (defer) |
| **`point`** (standalone) | — (points exist only as endpoints) | **−** |
| **`centerline`**, **`infinite`** | construction toggle only | **−** |
| `text` | ✅ (+ text-on-path parameter) | **+** |
| `project` / `include_3d` | project edge/face/vertex/body/profile | **=** |
| **`project_to_surface`**, `intersection_curve`, `isoparametric_curve`, `spun_profile`, `intersect` | — | **×** needs surfaces |
| `insert_dxf` | DXF import (File menu) | **=** |
| **`insert_svg`** | — | **−** |
| `draw.dimensioned` | draft-dimension system (geometry + dimension in one commit) | **=** |
| `line3d` / `point3d` / `spline3d` / `move3d` | — | **×** |

### 3.2 Modify

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| `trim`, `trim.path` | trim + trim-stroke painting | **+** |
| `extend`, `break`, `cut` | extend, split | **=** |
| `fillet` (line, line-arc, arc-arc) | same | **=** |
| `chamfer.equal_distance`, `.two_distance` | ✅ | **=** |
| **`chamfer.distance_angle`** | — | **−** |
| `offset` | offset (non-parametric copy) | **=** |
| **`offset.break`** | n/a — offsets are not associative here | **=** equivalent |
| `mirror` | ✅ | **=** |
| `pattern.circular`, `.rectangular` | circular + linear arrays | **=** |
| `move`, `scale` | transform (move/rotate/scale/copy) | **=** |
| **`blend_curve` (G1–G4)** | — | **−** (defer: Class-A) |
| `break_link` | `remove_sketch_projections` / unlink | **=** |
| `delete`, `drag`, `move_point`, `redefine`, `export_dxf`, `edit_text` | all present | **=** |

### 3.3 Constraints

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| coincident, collinear, concentric, equal, fix, H/V, midpoint, parallel, perpendicular, symmetry, tangent | all present (concentric by inference only — no command) | **=** / **~** |
| **`curvature` (G2)** | — | **−** (defer) |
| **`polygon`** (regular polygon) | — | **−** (defer) |
| **`auto_constrain`**, `finish_auto_constrain` | — | **−** |
| **`constrainer`** (apply the near-miss constraint) | — | **−** |

### 3.4 Dimensions & diagnostics

| SolidCraft | PolySmith | Verdict |
|---|---|---|
| `dimension` (12 kinds, refusal naming the determiner) | 12 kinds; conflict is counted, not named | **~** (S4 in the plan) |
| `dimension_text`, `toggle_driven` | ✅ | **=** |
| **`glyphs`** (show/hide constraint glyphs) | always on | **−** |
| **`inspect`**, **`solve`** | — (solve is automatic) | **−** |
| `snap` (queryable) | core emits candidates; UI resolves | **=** |
| **`loose_ends`** + tie | — | **−** |
| `chain` (double-click select) | marquee/click select | **=** |
| `options` | selection-filter panel | **=** |

---

## 4. The plan

Three phases. Effort is per item, one developer who knows the codebase. Nothing here touches the line tool.

### T1 — Cheap parity (≈ 7 days, no profile-detection risk)

| # | Item | Approach | Effort |
|---|---|---|---|
| T1.1 | **Slot: `center_point` + `overall`** | Same `add_sketch_slot` payload; only the pick sequence and the length meaning differ (span vs centre distance). Mode in the toolbar, arithmetic in the draft commit. | 1 d |
| T1.2 | **Chamfer `distance_angle`** | `SketchChamfer` gains `mode: equal \| two_distance \| distance_angle`; geometry is a line trimmed by an angle from one edge; third split-button mode. | 1 d |
| T1.3 | **Rounded rectangle** | New core command `add_sketch_rounded_rectangle { start, end, radius }` that creates the rectangle then four parametric fillets **atomically** (one undo step, one command). Avoids five round-trips and gives the UI a single call like `rectangle`. Needs the internal no-undo fillet entry point (`sketch_fillet_create_command.inc` computes trim/arc indices from `next_trim_entity_index`). | 2 d || T1.4 | **Point entity** | New `add_sketch_point`; standalone points participate in holes/patterns/constraints but not in profile loops. Touches the vertex model — the only T1 item with model risk. | 2 d |
| T1.5 | **Centerline / infinite line** | Line flags rather than new tools: `centerline`, `infinite` alongside `is_construction`; dashed/dash-dot styles already exist in SolidCraft as a reference; PolySmith has the `constraint` field to extend. | 1 d |

### T2 — Geometry work (≈ 15 days)

| # | Item | Approach | Effort |
|---|---|---|---|
| T2.1 | **Fit-point spline (+ degree 5)** | `mode: "fit" \| "control"` on `add_sketch_spline`; least-squares fit to a clamped B-spline (`spline_math.h` has the sampler); allow degree up to 5. **This is the item that also addresses S0** — the red `cad_core_spline_test` fails in `test_spline_extrude_smoke`, i.e. the spline→profile→extrude path this work rewrites. Treat S0 as the first sub-step: fix the failing case, then add fit points. | 3 d |
| T2.2 | **Tangent arc** | Third arc mode: start on an existing curve end, tangent there, through the cursor. Reuses the tangent-circle solve from the fillet work. | 2 d |
| T2.3 | **Arc slots (2 modes)** | New entity: two concentric arcs + two straight tangent sides; needs serialisation, rendering, trim/split behaviour and profile integration. Highest risk in the plan — `sketch_profile_exact.inc` is the repo's most fragile file. | 3 d |
| T2.4 | **Insert SVG** | Parse paths/polylines into sketch lines/beziers; the DXF importer is the template. | 1 d |
| T2.5 | **Auto-constrain + constrainer** | Infer, then dimension until DOF 0; keep a candidate only when DOF drops. Needs S2/S3 from the completion plan (one DOF source) first. | 4 d |
| T2.6 | **Loose ends + inspect + glyph toggle** | Loose ends from the vertex table the profile walk already builds; inspect reports counts/DOF; glyph toggle is a render flag. | 3 d |

### T3 — Deferred

Conic, blend curves (G1–G4), G2 curvature constraint, regular-polygon constraint, all 3D-sketch/surface commands, fit-curves-to-section. Each is either Class-A or needs the surface workspace.

---

## 8. Progress log

**2026-10-23 — T1.3 core, first slice (committed).**
`add_sketch_rounded_rectangle { start_x, start_y, end_x, end_y, radius }` exists and works, but the composition is simpler than planned: instead of an internal no-undo fillet entry point, it opens a **named undo group** (`begin_undo_group("Rounded Rectangle")`), calls the ordinary `add_sketch_rectangle` and four times the ordinary `add_sketch_fillet`, then collapses them. `undo_group_commands.inc` already folds nested pushes into one entry, and an abort restores the start snapshot, so a failed fillet leaves nothing behind. The result keeps real `SketchFillet` records, so the radius stays editable and deleting a fillet restores its corner — four arcs welded on would not.

Validated by `tests/sketch_rounded_rectangle_test.cpp` (registered in CMake, passing): four lines, four arcs, four fillet records at the requested radius, one closed profile; the radius is editable through `update_sketch_fillet_radius`; a radius of half the shorter side is refused; and a single `undo()` removes the sides, the arcs and the fillet records together.

Remaining for T1.3 (UI, next): the `rounded` rectangle mode in the toolbar split button, a radius field in the sketch tool panel, and the draft commit path calling the new command — plus the two-commit-paths check from `Sketch-Tool-Implementation.md` (Enter and click-click).

---

## 5. Order and why

**T1.3 (rounded rectangle) → T1.1 (slots) → T1.2 (chamfer) → T1.5 (line types) → T1.4 (point)**, then **T2.1 (fit-point spline, fixing S0 on the way)**.

Rationale: rounded rectangle is the most visible gap in everyday parts and it is *composed from primitives that already work* (rectangle + fillets), so the risk is integration, not geometry. Slots and the chamfer mode are arithmetic. The point entity is last in T1 because it touches the vertex model. Fit-point spline opens T2 both because it is the next most-requested family and because it shares the failing path with S0.

## 6. Testing contract per item

- Every creation variant ships with a C++ suite in `native/cad-core/tests/` asserting the **complete region set** (`profiles_match`, per CLAUDE.md) plus the parametric records the variant creates (fillet ids for the rounded rectangle, slot geometry for the modes).
- Anything touching `sketch_profile_exact.inc` (T2.3 only) runs the full `pnpm test:core`.
- UI variants also need the in-app checklist in the PR body: the tool appears in the split button, both commit paths work (Enter and click-click — see `Sketch-Tool-Implementation.md`), and Escape cancels without leaving geometry.
- IPC additions: `pnpm registry:write`, `IPC-Protocol.md`, `AI-CAD-Command-Language.md`.

## 7. What this branch already fixed, for context

The extrude/sketch interaction defects (D-001a–f in [Sketch-Defect-Log](Sketch-Defect-Log)) came out of the same code these tools sit on. Two of those fixes matter here directly: the profile-id staleness after an edit is why arc slots (T2.3) are priced as high-risk, and the fit-point spline path is the same one S0 fails in.
