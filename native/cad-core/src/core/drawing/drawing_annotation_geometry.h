#pragma once

#include <optional>
#include <vector>

#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_sheet.h"

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  Annotation graphics (GEOMETRY / SYMBOLS / ANNOTATE tabs)
// ══════════════════════════════════════════════════════════════════
//
// Pure function of (resolved attachment geometry in view-mm, view
// transform, annotation cosmetics) — the annotation analogue of
// build_dimension_graphics.  All offsets are SHEET-mm constants so
// annotations keep their size independent of the view scale.
//
// The output goes straight into the flattened stream (purpose
// "annotation") — the same primitives the SVG/DXF/PDF backends and
// the viewport consume.  Emitters use ONLY the line / filled_poly /
// circle_arc primitive kinds: the drawing_annotation_preview
// serializer (drawing_commands.inc) does not carry ellipse fields.

struct AnnotationGraphics {
  std::vector<SheetPrimitive> primitives;  // sheet-mm, purpose "annotation"
  std::optional<SheetText> text;           // purpose "annotation"
};

/// Builds the annotation presentation for one resolved attachment.
/// `annotation` supplies the cosmetic fields (text_override is
/// already in the resolved text; text_offset steers placement like
/// the dimension drag).
AnnotationGraphics build_annotation_graphics(const ResolvedAttachment& resolved,
                                             const Annotation& annotation,
                                             const DrawingView& view,
                                             bool stale);

}  // namespace polysmith::core
