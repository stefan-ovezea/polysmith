# Active Task: Sketch Text tool (feature/text)

> **Branch:** `features/text` (from `dev`)
> **Date:** 2026-08-20

## Status: implemented, C++ suites green, awaiting user verification + commit approval

Fusion-style parametric sketch text: `SketchText` records expand into
generated sketch lines on every recompute (deterministic ids, fixed
vertices), so text extrudes/renders/exports through the ordinary
pipeline. Emboss-ready by design (see `wiki/Emboss-Deboss-Design.md`).

## Shipped in this branch (all uncommitted)

- **C++ core**: `SketchText` + `generated_by` entity fields, text
  expansion pass (`impl/text_expansion.inc`), `TextEngine`
  (`core/text_engine.{h,cpp}` — OCCT `Font_BRepFont`/`Font_FTFont`,
  TKService+TKV3d linked), text commands (`impl/sketch_text_commands.inc`),
  DocumentManager wrappers (`document/impl/sketch_text_entity_commands.inc`),
  app handlers (`app/impl/sketch_text_command_handlers.inc`),
  serialization (texts + generated_by), viewport passthrough,
  generated-entity guards, text-aware `find_equivalent_profile`
  (exact id-set matching), DOF counter treatment.
- **UI**: Text toolbar tool + `SketchTextPanel`/`ActiveSketchTextPanel`
  (place → debounced live update → Enter confirm / Escape delete),
  click-glyph-in-select reopens the panel, `generated_by` scene
  threading, i18n, zod schemas. tsc clean.
- **Tests**: `cad_core_text_engine_test` + `cad_core_text_test`
  (14 cases). All 11 C++ suites green: `pnpm test:core`.

## Verification checklist (user, before commit)

1. `pnpm dev` → sketch → Text tool → click to place "Text" (10 mm).
2. Panel: edit string (multi-line), height, angle, spacing, H/V
   alignment — glyphs update live; Enter confirms, Escape deletes.
3. Extrude the text profile (New Body) → STEP/STL export if desired.
4. Select mode: click a glyph → panel reopens; Delete on selected
   glyph deletes the text.
5. Save → reload: text + extrude round-trip with no drift.
6. Load a user `.ttf` via the font dropdown.
7. Height edit after extrude (re-enter sketch first) keeps the extrude
   healthy; string edit degrades it with a timeline warning.

## Known limitations (documented)

- Default font = system-font fallback until Liberation Sans is dropped
  into `apps/desktop-ui/src-tauri/resources/fonts/` (plumbing ready —
  `POLYSMITH_TEXT_FONT_PATH`; needs network to fetch the TTF).
- Text-on-path, bold/italic, vertical text, DXF TEXT import = follow-ups.
- `delete_sketch_selection` glyph→text mapping exists in core; UI
  hover/selection excludes glyph lines in v1 (panel is the editor).

## Next session

- Await user manual verification → fix issues → get commit approval
  (branch workflow: squash-merge `features/text` → `dev`).
- Follow-up ideas: bundle font, text-on-path, emboss feature.
