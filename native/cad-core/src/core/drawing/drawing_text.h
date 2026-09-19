#pragma once

#include <vector>

#include "core/drawing/drawing_sheet.h"

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  Vector glyph emission for flattened-sheet text (P7)
// ══════════════════════════════════════════════════════════════════
//
// Every SheetText record in the flattened stream stays as DATA (the
// DXF backend emits a real DRW_Text from it); these line primitives
// give the viewport and the PDF/SVG backends true single-stroke
// glyphs laid out by the core text engine with the bundled ISO 3098
// font (OSIFONT — LGPL v3 + font-embedding exception).  One purpose
// ("text_glyph") so the backends can filter them (the DXF backend
// drops them — the DATA record wins there).

/// Converts one SheetText record into vector glyph line primitives
/// (sheet-mm, purpose "text_glyph", thin continuous lines).  Layouts
/// are cached per (font, text, height, angle, h_align) so repeated
/// flattens of the same sheet stay cheap.  Returns an empty vector
/// when the text is empty or the layout fails (the text engine
/// warning is logged once).
std::vector<SheetPrimitive> drawing_text_glyphs(const SheetText& text);

}  // namespace polysmith::core
