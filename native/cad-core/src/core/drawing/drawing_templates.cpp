#include "core/drawing/drawing_templates.h"

#include <algorithm>
#include <vector>

#include "protocol/serialization.h"

namespace polysmith::core {

std::string drawing_template_validate(const DrawingTemplate& template_) {
  if (template_.name.empty()) {
    return "The drawing template needs a name.";
  }
  if (template_.sheets.empty()) {
    return "A drawing template needs at least one sheet.";
  }
  static const std::vector<std::string> kPaperSizes = {
      "A0", "A1", "A2", "A3", "A4"};
  for (size_t i = 0; i < template_.sheets.size(); ++i) {
    const DrawingSheet& sheet = template_.sheets[i];
    const std::string where = "Template sheet " + std::to_string(i + 1);
    if (std::find(kPaperSizes.begin(), kPaperSizes.end(), sheet.paper_size) ==
        kPaperSizes.end()) {
      return where + ": unknown paper size '" + sheet.paper_size + "'.";
    }
    if (sheet.orientation != "portrait" && sheet.orientation != "landscape") {
      return where + ": unknown orientation '" + sheet.orientation + "'.";
    }
    if (sheet.projection_angle != "first_angle" &&
        sheet.projection_angle != "third_angle") {
      return where + ": unknown projection angle '" +
             sheet.projection_angle + "'.";
    }
  }
  return "";
}

nlohmann::json drawing_template_to_json(const DrawingTemplate& template_) {
  nlohmann::json out;
  out["name"] = template_.name;
  out["sheets"] = nlohmann::json::array();
  for (const DrawingSheet& sheet : template_.sheets) {
    // Setup-only: sheet ids and view ids are minted at create time.
    nlohmann::json sheet_json = polysmith::protocol::to_payload(sheet);
    sheet_json.erase("sheet_id");
    sheet_json.erase("view_ids");
    out["sheets"].push_back(std::move(sheet_json));
  }
  return out;
}

DrawingTemplate drawing_template_from_json(const nlohmann::json& payload,
                                           std::string& error) {
  DrawingTemplate template_;
  template_.name = payload.contains("name") && payload.at("name").is_string()
                       ? payload.at("name").get<std::string>()
                       : "";
  if (payload.contains("sheets") && payload.at("sheets").is_array()) {
    for (const auto& sheet_json : payload.at("sheets")) {
      // The payload parser's lenient defaults (missing keys never
      // fail) — the validate pass below rejects wrong VALUES.
      template_.sheets.push_back(
          polysmith::protocol::drawing_sheet_from_payload(sheet_json));
    }
  }
  error = drawing_template_validate(template_);
  return template_;
}

}  // namespace polysmith::core
