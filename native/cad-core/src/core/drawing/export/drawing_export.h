#pragma once

#include <string>

#include "core/drawing/drawing_sheet.h"
#include "core/export/export.h"

namespace polysmith::core {

struct DocumentState;

// ══════════════════════════════════════════════════════════════════
//  Drawing sheet export (P8: SVG + DXF)
// ══════════════════════════════════════════════════════════════════
//
// Both backends consume the SAME flattened stream the viewport draws
// (drawing_sheet.h) — consistency by construction.  The stream
// carries sheet-mm geometry with ISO 128-2 dash patterns ALREADY
// applied and the P7 vector glyphs for every text record.
//
//  - SVG: renders the primitives as-is (glyphs included — the text
//    records stay DATA and are skipped).  mm viewBox at the sheet
//    size, y-axis flipped to SVG's screen convention.
//  - DXF: ASCII R2013 (AC1027).  Named layers with ISO lineweights,
//    the pre-dashed segments stay on CONTINUOUS layers (the pattern
//    is baked in by the core — re-dashing would double it; the
//    HIDDEN/CHAIN linetypes are still DEFINED for user reuse and the
//    P9 annotated mode).  Text records become real DRW_Text entities
//    (the glyph primitives are skipped — a drawing must not carry
//    the text twice).  The title block travels as a
//    BLOCK ("POLYSMITH_TITLE_BLOCK") + INSERT, registered before
//    definition (the libdxfrw UB trap).
//
// Both throw std::runtime_error on I/O failure or an unknown format.

/// Exports one sheet of a drawing ("svg" | "dxf").  The sheet is
/// flattened from the current runtime projections — an export never
/// re-projects and never dirties the document.
ExportResult export_drawing_sheet(const DocumentState& document,
                                  const std::string& drawing_id,
                                  const std::string& sheet_id,
                                  const std::string& format,
                                  const std::string& file_path);

/// Backends (stream-level; the test suite calls them directly).
ExportResult export_sheet_as_svg(const SheetPrimitiveStream& stream,
                                 const std::string& file_path);
ExportResult export_sheet_as_dxf(const SheetPrimitiveStream& stream,
                                 const std::string& file_path);

}  // namespace polysmith::core
