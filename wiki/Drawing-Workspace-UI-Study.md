# Drawing Workspace UI Study — the Fusion-style rework (2026-09-19)

> **Status:** study complete; R1 implemented and merged (PR #90, `aa9c4ba`).
> R2–R5 + ANNOTATE implemented on `feature/ISO-drawing-fix` (2026-09-20/21,
> user-verified in-app, awaiting merge): dimension mouse drag, the
> ANNOTATE (text/leader), GEOMETRY (center mark/centerline/edge
> extension) and SYMBOLS (surface finish/welding/tolerance frame/datum/
> balloon) tabs, the CREATE DRAWING dialog + templates, and Section +
> Detail views (ISO 128-3 §4.12). This is the
> blueprint for the drawing-workspace UI rework requested by the user:
> a 2026-feel, toolbar-driven workspace in the spirit of Fusion 360's
> Drawing workspace — tools on the ribbon, direct manipulation on the
> sheet, no floating windows (the New Drawing setup dialog is the one
> deliberate exception, per the user).
>
> **User mandate (verbatim):** "I want 2026 feel of the UI. I want
> Fusion feeling with tools that I can call from the toolbar instead
> of the stupid floating windows... keep the first one 'new drawing'
> like a setup window and then should be same like a sketch where I
> can add projections, dimensions etc. In fusion you can simply drag
> the body in the 3 iso projection and in the 4th iso view of the
> body... In the menu you just pick up sections, annotations, symbols
> (welding, roughness, tolerance etc). That kind of UI I want."

## 1. What Fusion 360's Drawing workspace actually does (the study)

Grounded in the Autodesk help + tutorial corpus (see Sources):

- **New Drawing > From Design** is a SETUP DIALOG: sheet size,
  orientation, standard (ISO → first-angle, ASME → third-angle),
  title block. After that, everything happens ON the sheet with tools.
- **The ribbon is tabbed by intent:** DRAWING VIEWS (Base, Projected,
  Section, Detail), GEOMETRY (center marks, centerlines, edge
  extensions), DIMENSION, SYMBOLS (surface texture, feature control
  frames / GD&T, datum identifiers), plus MODIFY (move/rotate),
  TEXT (text + leaders), TABLE (parts list + balloons). The user's
  "menu with sections, annotations, symbols" maps directly onto this.
- **Base View placement:** click the tool → the Drawing View dialog
  carries the settings (orientation dropdown: Front, Top, NE
  Isometric, Named Views…, scale, style, hidden lines); a View Cube
  with drag manipulators lets you rotate the model preview freely
  (Inventor's canonical form of the dialog); then click on the sheet
  to place. This is the interaction the user describes as "drag the
  body in the 3 iso projection".
- **Projected View placement:** after (or instead of) the base view,
  moving the cursor right/left/up/down from the parent shows a LIVE
  projected view that snaps to orthographic alignment — click to
  place, **drag diagonally to get an isometric projected view**,
  Enter finishes the mode. This loop — base view once, then
  cursor-directed projections — is the heart of the "2026 feel".
- **Section View:** pick a line across the parent view → the section
  ghost follows the cursor → place it; letters auto-increment (A, B,
  C). **Detail View:** click a center, drag the boundary circle, the
  enlarged view follows → place.
- **Dimension:** highlight a vertex/edge → drag → the dimension
  follows the cursor with auto orientation → click to place. Chained
  dimensions by continuing along the same edge line.
- **Views are objects:** drag by their grip to move (children follow
  the parent in ortho alignment), double-click to edit (orientation,
  scale, style, hidden lines), Esc unsnaps / finishes tools.
- **Everything is model-linked** — edit the design, the drawing
  updates; that part PolySmith already owns (refresh pass + TNP).

## 2. PolySmith state vs the target (what exists, what's missing)

Already in place (core — do not rebuild):

- `DrawingView` already carries `custom_frame` + kind `"axonometric"`
  (`drawing_types.h`), the payload parses it, and `resolve_view_frame`
  prefers it when `standard_view` is empty. **Isometric and
  arbitrary-orientation views are CORE-READY** — the UI just never
  sent a custom frame.
- Non-mutating `drawing_view_preview` (ghost geometry in the exact
  committed-view vocabulary) + `drawing_view_move` + mouse plumbing:
  pointer hover→sheet-mm resolution (`resolveSheetPoint`), click-vs-
  pan guard, pointer capture, view-frame hit-testing with named
  meshes (`view-frame:<id>`), crosshair cursor. All of it reusable by
  every tool below.
- Sections + hatching + cutting-plane traces + A–A labels (P4),
  dimensions linear/angular/radius/diameter with core-computed
  preview graphics (P6), title block + sheets + exports (P5/P7/P8/P9).
- `AnnotationExtension` (tolerance_frame, datum, surface_texture…)
  was designed for exactly the SYMBOLS tab — no migration needed.

Missing / to build:

- **A real drawing ribbon** replacing the `DrawingToolbar` stub and
  the floating panels (Insert View, Dimension, Sheet settings are all
  to become tools; the Sheet settings can live in a compact
  toolbar-popover or the browser tree).
- **A tool state machine** (`drawingTool` like `activeSketchTool`):
  `idle | insert_base | insert_projected | section | detail |
  dimension | center_mark | centerline | text | leader | symbol_* |
  move | edit`.
- **Orientation picker** for the base view: six standard + four
  isometric (NE/NW/SE/SW) + **"current viewport"** (capture the CAD
  camera as a `DrawingViewFrame`: origin = body center, normal =
  view direction, x_direction = camera right projected onto the view
  plane). A mini orientation cube with drag-to-rotate (the user's
  "drag the body in the 3 iso projection") can be phase 2 of the
  picker; "current viewport" ships first — the user orients in 3D,
  then inserts what they see.
- **Projected-mode auto-engagement:** after placing a base view the
  tool stays armed; the cursor direction picks the projection
  (ortho axes + diagonals → iso), snapping to first/third angle per
  the sheet. Reuses the existing slot table + ghost.
- **Core gaps to close (small, per phase):**
  - detail views: `DrawingView` needs `detail: {parent_view_id,
    center, radius, scale}` fields + payload + refresh (the projection
    is the parent's geometry clipped to the circle);
  - symbols: new `Annotation` kinds (surface_texture, welding,
    tolerance_frame, datum, balloon) + flatten graphics + commands
    (`drawing_annotation_create/update/delete`, one generic pair is
    enough — the graphics builders are per-kind);
  - dimension placement offset: the preview must accept a live text
    offset so the dimension follows the cursor before the commit
    (`drawing_dimension_preview` + an `offset` field, or a
    UI-translated ghost — prefer the core, one source of truth);
  - projected-view inheritance (scale/style) is UI-side only.
- **Hover feedback everywhere:** nearest-edge highlight for
  dimensions, section-line snapping to view centers, cursor styles
  per tool. The sheet curves are already in the viewport payload —
  nearest-segment math is UI-side.

## 3. Target interaction design, tool by tool

### Ribbon (drawing workspace, replaces DrawingToolbar + floating panels)

| Tab | Tools | Mode on the sheet |
|---|---|---|
| VIEWS | New Drawing (setup dialog — KEPT), Base View, Projected View, Section, Detail, Delete View | arm tool → sheet interaction |
| GEOMETRY | Center Mark, Centerline, Edge Extension | click arcs / edges |
| DIMENSION | Dimension (smart), Radius, Diameter, Angle, Chain | hover → click → drag → click |
| SYMBOLS | Surface Finish, Welding, Tolerance Frame, Datum, Balloon | click attachment → place |
| ANNOTATE | Text, Leader Text | click → place → type (sketch-text pattern) |
| MODIFY | Move, Delete, Edit View (double-click shortcut) | drag frames / context |

Settings that the floating panels currently own become a compact
**toolbar strip**: while a view tool is armed, a small inline control
row (scale dropdown, hidden-lines toggle, section letter, hatch
angle) sits in the toolbar — same state, no window. Escape cancels
the tool and restores `idle` (the contextual workflow pattern).

### Base View (the flagship)

1. Click Base View. Crosshair over the sheet.
2. The ghost shows the front view by default; the toolbar strip has
   the orientation control: six standard buttons + four iso buttons +
   **"Current 3D view"** — this one captures the CAD viewport camera
   into a custom frame, so "orient in 3D, then insert what you see"
   works exactly like the user asked (drag the body in ISO, place
   the 4th view).
3. Move over the sheet → ghost follows (already working). Click →
   view lands. The tool auto-switches to **projected mode**: moving
   up/down/left/right from the base view previews the aligned
   projection (first/third angle per sheet), moving diagonally
   previews the isometric projection. Click to place each; Esc ends.

Phase 2 upgrade: a mini orientation CUBE (the app already has a view
cube component for the 3D viewport) embedded in the toolbar strip —
drag it to rotate the ghost freely.

### Section & Detail

- **Section:** the tool asks for a line across the base view (drag
  from edge to edge, snapping to view centers) → the section ghost
  follows the cursor → click to place. Label auto-increments A, B, C;
  cutting-plane trace + arrows + labels render on the parent (P4/P7
  already do this for committed views — the trace must also appear in
  the PREVIEW while the line is being drawn).
- **Detail:** click the center, drag the boundary circle, the scaled
  view follows → place. Needs the `DrawingView.detail` core fields.

### Dimension (rework of the current pick flow)

1. Hover highlights the nearest sheet edge (thin highlight ribbon).
2. Click the edge → the dimension preview appears FOLLOWING the
   cursor at the perpendicular offset (live text + arrows + extension
   lines), not at a fixed spot.
3. Click to place. For distance/angle, a second edge click switches
   to the two-edge form, then the same drag-and-place.
4. Chain: while the tool is armed, continuing from the last placed
   dimension chains the next one (ISO 129-1 baseline/chain).
Core change: `drawing_dimension_preview` gains a live offset, and
`drawing_dimension_create` takes the placement point.

### Symbols (SYMBOLS tab)

Data model is ready (`AnnotationExtension`); the core needs per-kind
graphics + one generic annotation command family. Symbols are
model-ANCHORED (attached to a view + a witness where meaningful —
welding on a section edge, roughness on a face, GD&T frames on
dimensions) and derive their sheet placement at regeneration, like
dimensions. Rendering reuses the vector-text engine (P7).

### Move / Edit

- Drag a view frame → children follow in ortho alignment (current
  drag moves only the grabbed view; add child-follow in the UI move
  action). Drop commits `drawing_view_move`.
- Double-click a view → Edit View strip (orientation, scale, style,
  hidden lines) — `drawing_view_update` already exists.
- Delete key with a selected frame → `drawing_view_delete`.

### Browser tree (phase 5, nice-to-have)

Left tree: document → drawing → sheets → views (children indented),
right-click rename/delete; sheet settings (paper/orientation/angle)
move here from the floating Sheet panel.

## 4. Phased plan for the new session

- **R1 — Ribbon + tools + Base/Projected (the big one):** drawing
  ribbon with tabs, `drawingTool` state machine, toolbar strip for
  settings, Base View tool with orientation control incl. "Current 3D
  view" → custom frame, auto-projected mode with ortho + diagonal iso
  ghosts, remove the Insert View floating panel (New Drawing dialog
  stays). Reuses: ghost preview, cursor-follow, click-to-place,
  view drag.
- **R2 — Dimension rework:** hover highlight, drag-and-place with
  live offset (core preview offset + create placement point), chain.
  *(Done: whole-dimension mouse drag + decimal-dot default.)*
- **R3 — Section & Detail tools:** line-drag section + preview trace,
  detail view fields in core (data model + payload + refresh + tests).
  *(Done: Section panel flow + Detail View circle-drag tool;
  `drawing_detail_clip` + `cad_core_drawing_detail_test`.)*
- **R4 — Symbols:** core annotation extensions → per-kind graphics +
  commands + tests; SYMBOLS tab with surface finish / welding / GD&T /
  datum / balloon. *(Done: 8 emitters, 7 commands, SYMBOLS tab live.)*
- **R5 — Geometry + polish:** center marks/lines, text/leaders,
  browser tree, double-click edit, cursors/hovers/Esc semantics,
  perf pass (1000-curve sheet < 100 ms scene build).
  *(Done: GEOMETRY + ANNOTATE tabs; browser tree + double-click edit
  + the perf pass remain open.)*

Gates per phase: `pnpm core:build` + `pnpm test:core` + `tsc
--noEmit`; new core commands ship with schema + IPC doc + regression
tests; labels via en.json; colors via theme tokens; user verifies
in-app before any commit.

## 5. Sources

- [Autodesk Fusion 360 — Drawings workspace help](https://help.autodesk.com/view/fusion360/ESP/?guid=GUID-54D1504C-8885-4EF7-A60E-8E3B902A2632)
- [Autodesk Fusion 360 — Activity 2: Create projected and detail views](https://help.autodesk.com/view/fusion360/ENU/?guid=GUID-085FEA7C-EE94-4010-B1FC-2D9908ABAABB)
- [Autodesk Inventor — Base and Projected Views enhancements (View Cube on the dialog)](https://help.autodesk.com/cloudhelp/2020/ENU/InventorLT-WhatsNew/files/GUID-493B5FC5-3AAB-4DF8-8987-1D4C5B5CA7F2.htm)
- [Tulane makerspace — Fusion 360 Intro Manual (drawing view placement)](https://makerspace.tulane.edu/images/f/f2/Fusion_Intro_Manual.pdf)
- [How to rotate a drawing view — Autodesk forums (projected-view association caveat)](https://forums.autodesk.com/t5/fusion-design-validate-document/how-do-i-rotate-a-drawing-view/m-p/7395927/highlight/true)
