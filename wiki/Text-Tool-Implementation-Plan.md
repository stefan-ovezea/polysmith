# Text Tool — Implementation Plan & Design

> **Status:** v1 implemented on `features/text` (2026-08). This page is the
> canonical design + follow-up reference; the original 2026-05-24 research
> plan has been superseded by the hybrid-entity design below.

## Overview

Text is a first-class sketch entity. A `SketchText` record stores the
parametric definition (string, font, height, angle, anchor, alignments,
spacing); on every recompute the **text expansion pass** re-derives the
glyph geometry into ordinary sketch lines with deterministic ids. Text
therefore flows through profile detection, extrude, viewport rendering,
and STEP/STL export with **zero downstream changes** — text profiles are
normal profiles, which is exactly what a future Emboss feature needs
(see [Emboss-Deboss-Design](Emboss-Deboss-Design)).

## Architecture

```
SketchText (parameters.texts)          — parametric record
   │  refresh_sketch_texts (top of refresh_sketch_derived_state)
   ▼
SketchLine[]  id = line-text-<text-id>-c<contour>-s<seg>
              vertex ids = vertex-text-<text-id>-c<contour>-s<idx> (shared joins)
              generated_by = "text:<text-id>", constraint = nullopt,
              vertices always is_fixed (re-asserted after the flag sync)
   │  existing pipeline
   ▼
profiles → extrude / viewport / export   (no changes anywhere downstream)
```

- **TextEngine** (`native/cad-core/src/core/text_engine.{h,cpp}`) wraps
  OCCT's `StdPrs_BRepFont` / `Font_FTFont` (TKService + TKV3d, FreeType
  linked statically). Glyph faces → wires → edges are classified
  (line / circle / B-Spline) and chordal-tessellated with tolerance
  `clamp(height/200, 0.01, 0.2)` mm; layout mirrors `Font_TextFormatter`
  math (`AdvanceX` kerning, `LineSpacing` for `\n`) so there is no drift
  from OCCT's own layout. Default font = DejaVu Sans via `Font_FontMgr`
  (embedded fallback in TKService — deterministic on every machine, no
  font file required). User fonts = absolute `.ttf` path.
- **Expansion** (`core/sketch/impl/text_expansion.inc`): strips all
  `generated_by` entities, re-expands each text, pushes `SketchLine`s
  directly (bulk pattern — no inference, no dimensions, no solver
  participation; text vertices are fixed).
- **Guards**: `require_user_line` / `ensure_user_editable_entity` reject
  generated entities in update/trim/move/dimension/constraint/anchor
  commands; `delete_sketch_selection` maps a pure-glyph selection to the
  owning text (deleting text geometry deletes the text, Fusion-style).
- **DOF**: generated lines report fully constrained (fixed endpoints),
  so glyphs render in the "fixed" color.

## TNP contract

The generated ids are deterministic and the tessellation tolerance is
proportional to the height (OCCT renders at a fixed 72 pt and scales
linearly, so the subdivision structure is scale-invariant), so the
**line-id set** of every glyph region is stable across geometric edits.
`find_equivalent_profile` matches text regions by **exact id-set
equality** (not containment — the contour indices are reused across
strings, so containment would match "O" to "A"):

| Edit | Line-id set | Linked extrudes |
|---|---|---|
| height, angle, anchor, alignment, spacing | unchanged | re-snapshot, stay healthy |
| text string or font | changes (glyph set differs) | `dependency_broken` + "Source profile unavailable" |
| delete text | generated lines removed | profiles vanish → `dependency_broken` |

(Profile *ids* themselves embed corner coordinates and change under any
geometric edit — for text and user sketches alike; the id-set matching
above is what keeps extrudes re-snapshotting.)

All degradation goes through the existing `refresh_linked_extrudes` /
`find_equivalent_profile` path — never a crash. Note: editing the text
of a sketch requires re-entering the sketch first — extruding
deactivates the active sketch, exactly like other sketch edits.

## IPC

- `add_sketch_text { text?, font_path?, height_mm?, angle_deg?, anchor_x, anchor_y, h_align?, v_align?, char_spacing? }`
- `update_sketch_text { text_id, ...partial patch... }` — the core merges
  over the stored record; each command is one undo entry (UI debounces
  typing at 250 ms, mirroring the fillet panel).
- `delete_sketch_text { text_id }`

Undo semantics follow the fillet precedent: the text entity exists the
moment it is placed; Escape cancels via `delete_sketch_text` (one undo).

## Tests

- `cad_core_text_engine_test` — layout, determinism, tolerance policy,
  multi-line, spacing, angle, alignment anchors, missing-font failure.
- `cad_core_text_test` — full profile sets for "O" (outer + hole) and
  "AB" via `profiles_match`, re-expansion stability, height-edit id
  stability + 1.5× scaling, string-edit id change, guard matrix, extrude
  from text (ring prism with through-hole), delete-via-selection,
  save/load round trip (zero drift), TNP break-vs-survive matrix, undo.

## Follow-ups (not in v1)

- ~~**Text on path**~~ — **SHIPPED (2026-08-20).** `path_entity_id` binds
  the text to a user sketch line or arc; the engine places each glyph at
  its advance distance along the curve, rotated to the tangent, with
  `path_offset` (mm) shifting the baseline perpendicular (positive =
  left of travel) and multi-line stacking one line-height to the right
  of travel. In path mode `h_align` aligns along the curve
  (start/center/end), while `angle_deg`, `v_align`, and the anchor are
  ignored — the curve drives placement. The path is re-read from the
  sketch on every recompute, so dragging the path entity moves the text
  with it; a missing path degrades to `render_error` (no geometry, no
  crash) and recovers once a real path is bound. UI: the panel's Path
  section (Pick path arms the picker — the next viewport click on a
  line/arc binds it; Clear unbinds; Offset input), with angle/v-align
  disabled while a path is bound. Text longer than the curve overflows
  past the end ("fit to path" is a later polish).
- **Exact arc emission** — circle edges currently tessellate to chords;
  a `kEmitExactArcs` switch in the engine can emit `SketchArc`s.
- **Bold / italic** — `Font_FontAspect` variants of the loaded font.
- **Vertical text**, **text selection/drag in the viewport**, **DXF
  TEXT/MTEXT import** (currently skipped).
- **Bundled Liberation Sans** — the plumbing exists
  (`POLYSMITH_TEXT_FONT_PATH` env set by the Tauri spawn when
  `resources/fonts/LiberationSans-Regular.ttf` exists, plus
  `TextEngine::bundled_font_path()` fallbacks); only the font file +
  license copy need to be added to `apps/desktop-ui/src-tauri/resources/fonts/`.
