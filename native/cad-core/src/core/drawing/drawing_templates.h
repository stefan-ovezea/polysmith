#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  Drawing templates (CREATE DRAWING dialog) — pure functions, no
//  file I/O and no DocumentManager: the command handlers own the
//  streams (the cam_tool_import_file precedent).
// ══════════════════════════════════════════════════════════════════

/// Mirrors the drawing_create + drawing_sheet_update rules: a
/// non-empty name, at least one sheet, and per sheet the exact
/// paper_size / orientation / projection_angle enums.  Returns "" when
/// the template is valid, an error string otherwise (never throws).
std::string drawing_template_validate(const DrawingTemplate& template_);

/// Serializes the setup-only shape {name, sheets:[{name, paper_size,
/// orientation, projection_angle, title_block}]} — sheet_id and
/// view_ids are stripped (ids are minted at drawing_create time).
nlohmann::json drawing_template_to_json(const DrawingTemplate& template_);

/// Parses the template shape leniently (missing keys take the same
/// defaults as the drawing payload parser), then validates.  Sets
/// `error` to the validation message when invalid.
DrawingTemplate drawing_template_from_json(const nlohmann::json& payload,
                                           std::string& error);

}  // namespace polysmith::core
