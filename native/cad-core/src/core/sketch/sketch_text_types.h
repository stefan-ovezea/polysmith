#pragma once

#include <optional>
#include <string>

namespace polysmith::core {

// Parametric sketch text entity (Fusion-style). The glyph geometry is NOT
// stored here — `refresh_sketch_texts` expands every entry into plain
// SketchLine segments (owned via `generated_by`) on every recompute, so
// the existing profile detection / extrude / viewport pipeline consumes
// text with zero downstream changes. This struct only carries the
// parameters that define the text.
struct SketchText {
  std::string id;  // "text-N", assigned by the document manager counter
  // UTF-8 string; '\n' starts a new line.
  std::string text;
  // Empty = the engine's default font (bundled / embedded fallback).
  // Otherwise an absolute path to a user-loaded .ttf file.
  std::string font_path;
  double height_mm = 10.0;
  double angle_deg = 0.0;
  // Anchor point in sketch-local coordinates. The anchor maps to the
  // alignment point of the text frame (see h_align / v_align).
  double anchor_x = 0.0;
  double anchor_y = 0.0;
  // "left" | "center" | "right"
  std::string h_align = "center";
  // "top" | "middle" | "bottom"
  std::string v_align = "middle";
  // Fraction added to each character advance (0.5 = +50% spacing).
  double char_spacing = 0.0;

  // ── Reserved for text-on-path (follow-up) ─────────────────────
  // When set (and the follow-up ships), glyphs are placed along the
  // referenced sketch curve instead of the flat frame. Unused in v1;
  // kept in the struct + serialization so documents don't need a
  // format bump when the feature lands.
  std::optional<std::string> path_entity_id;
  std::optional<double> path_offset;

  // Diagnostic: set by the expansion pass when the last render failed
  // (missing font file, unsupported text, ...). Empty = renders fine.
  std::string render_error;
};

}  // namespace polysmith::core
