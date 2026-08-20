# Active Task: Sketch Text tool + text on path (feature/text)

> **Branch:** `features/text` (from `dev`)
> **Date:** 2026-08-20

## Status: text committed (a20a1a0); text-on-path committed (db0d9c5); picker/builder fix user-verified, committing

Fusion-style parametric sketch text: `SketchText` records expand into
generated sketch lines on every recompute (deterministic ids, fixed
vertices), so text extrudes/renders/exports through the ordinary
pipeline. Emboss-ready by design (see `wiki/Emboss-Deboss-Design.md`).
Text on path: `path_entity_id` binds the text to a user line/arc —
glyphs flow along the curve rotated to its tangent (see
wiki/Text-Tool-Implementation-Plan).

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

## Verification checklist (user, text on path)

1. `pnpm dev` → sketch a line or arc → Text tool → click to place text.
2. Panel → Path → **Pick path** → click the line/arc in the viewport —
   glyphs flow along the curve, rotated to its tangent.
3. Offset (mm) shifts the text perpendicular; Clear unbinds (flat text
   returns); angle/v-align disable while bound.
4. Drag the path entity — the text follows on the next recompute.
5. Extrude a path-bound text (New Body) → Save/reload round-trips.
6. Delete the path line — text degrades gracefully (no crash, no
   glyphs) and recovers when a new path is bound.

## Known limitations (documented)

- Default font = system-font fallback until Liberation Sans is dropped
  into `apps/desktop-ui/src-tauri/resources/fonts/` (plumbing ready —
  `POLYSMITH_TEXT_FONT_PATH`; needs network to fetch the TTF).
- Bold/italic, vertical text, DXF TEXT import, fit-to-path toggle =
  follow-ups.
- `delete_sketch_selection` glyph→text mapping exists in core; UI
  hover/selection excludes glyph lines in v1 (panel is the editor).

## Next session

- Await user manual verification of text-on-path → fix issues → get
  commit approval (squash-merge `features/text` → `dev` when ready).
- Follow-up ideas: bundle font, fit-to-path, emboss feature.
