#pragma once

#include <cstddef>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

struct DocumentState;
struct Drawing;
struct SheetPrimitiveStream;

// ══════════════════════════════════════════════════════════════════
//  ISO 7200 title block (P7)
// ══════════════════════════════════════════════════════════════════
//
// 180 mm × 63 mm, bottom-right inside the frame.  The eight
// mandatory ISO 7200 fields, the ISO 5455 scale auto-filled from the
// sheet's FIRST view ("1:1" while the sheet has none), the
// segment/sheet number ("Sheet x/y"), "Dimensions in millimetres",
// the ISO 8015 note, and the ISO 5456-2 projection symbol inside the
// top-right cell.  The revision table (zone/rev/description/date/
// approved) sits directly above the block and is drawn only while at
// least one row exists.
//
// Block lines land in `stream.primitives` in furniture order; field
// texts land in `stream.texts` as DATA records (purpose
// "title_block") — flatten_sheet appends their vector glyphs with
// the rest of the sheet texts.

void flatten_title_block(const DocumentState& document,
                         const Drawing& drawing, const DrawingSheet& sheet,
                         size_t sheet_index, size_t sheet_count,
                         SheetPrimitiveStream& stream);

}  // namespace polysmith::core
