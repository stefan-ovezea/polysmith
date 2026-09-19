#pragma once

#include <optional>
#include <vector>

#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_sheet.h"

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  Dimension graphics (P6) — ISO 129-1 presentation in sheet-mm
// ══════════════════════════════════════════════════════════════════
//
// Pure function of (resolved attachment geometry in view-mm, view
// transform, annotation cosmetics): extension lines (8×d gap and
// overshoot off the thin 0.25 line), closed filled 30° arrowheads
// (one style per drawing), an unbroken dimension line, text above it
// (unidirectional — always readable from the bottom of the sheet).
// All annotation offsets are SHEET-mm constants so dimensions keep
// their size independent of the view scale, exactly like real CAD.
//
// The output goes straight into the flattened stream (purpose
// "dimension") — the same primitives the SVG/DXF/PDF backends consume.

struct DimensionGraphics {
  std::vector<SheetPrimitive> primitives;  // sheet-mm, purpose "dimension"
  std::optional<SheetText> text;
};

/// Builds the dimension presentation for one resolved dimension.
/// `annotation` supplies the cosmetic fields (prefix is already in
/// the resolved text; text_offset / arrow_flip steer placement).
DimensionGraphics build_dimension_graphics(const ResolvedDimension& resolved,
                                           const Annotation& annotation,
                                           const DrawingView& view,
                                           bool stale);

}  // namespace polysmith::core
