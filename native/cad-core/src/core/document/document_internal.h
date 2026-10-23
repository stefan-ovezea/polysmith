#pragma once

// Non-OCCT free helpers shared by the DocumentManager method translation
// units (document_lifecycle.cpp and future promotions). Extracted from
// `core/document/impl/private_document_helpers.inc` and
// `core/document/impl/manager_state_helpers.inc` so promoted .cpp units can
// reach them without the anonymous-namespace include. OCCT-bound helpers stay
// in document.cpp (the §7.1 #4 architecture boundary).

#include <algorithm>
#include <cctype>
#include <sstream>
#include <source_location>
#include <string>

#include "core/document/document.h"
#include "core/geometry/refresh_dependents.h"

namespace polysmith::core {

// Re-derive every sketch-derived feature (extrude / loft / revolve / sweep)
// whose source profiles live in the document's sketches. Defined in
// document.cpp (it needs the source-profile builder) and used by the
// document lifecycle right after a load, exactly like a sketch edit does.
void refresh_linked_features_after_load(DocumentState& document);

inline int action_count(const DocumentState& document) {
  int count = 0;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "root_part") {
      ++count;
    }
  }
  return count;
}

inline bool is_origin_plane_reference(const std::string& reference_id) {
  return reference_id == "ref-plane-xy" || reference_id == "ref-plane-yz" ||
         reference_id == "ref-plane-xz";
}

inline bool includes_feature_at_cursor(const DocumentState& document,
                                       const std::string& feature_id) {
  int remaining_actions =
      document.timeline_cursor.value_or(action_count(document));
  for (const auto& feature : document.feature_history) {
    if (feature.id != feature_id) {
      if (feature.kind != "root_part" && remaining_actions > 0) {
        --remaining_actions;
      }
      continue;
    }
    return feature.kind == "root_part" || remaining_actions > 0;
  }
  return false;
}

inline void normalize_timeline_cursor(DocumentState& document) {
  if (!document.timeline_cursor.has_value()) {
    return;
  }
  const int max_actions = action_count(document);
  if (document.timeline_cursor.value() >= max_actions) {
    document.timeline_cursor = std::nullopt;
  } else if (document.timeline_cursor.value() < 0) {
    document.timeline_cursor = 0;
  }
}

// Derives the default step name from the pushing method (see
// manager_state_helpers.inc for the parsing rationale).
inline std::string undo_step_name_from_location(
    const std::source_location& location) {
  std::string raw(location.function_name());
  const size_t params = raw.find('(');
  if (params != std::string::npos) {
    raw = raw.substr(0, params);
  }
  const size_t scope = raw.rfind("::");
  if (scope != std::string::npos) {
    raw = raw.substr(scope + 2);
  }
  std::string name;
  name.reserve(raw.size());
  bool capitalize = true;
  for (char c : raw) {
    if (c == '_') {
      name.push_back(' ');
      capitalize = true;
    } else if (capitalize) {
      name.push_back(static_cast<char>(std::toupper(
          static_cast<unsigned char>(c))));
      capitalize = false;
    } else {
      name.push_back(c);
    }
  }
  return name;
}

inline std::string face_owner_id(const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return "";
  }
  return face_id.substr(0, separator);
}

inline std::string edge_owner_id(const std::string& edge_id) {
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos) {
    return "";
  }
  return edge_id.substr(0, separator);
}

inline bool is_selectable_plane_reference(const DocumentState& document,
                                          const std::string& reference_id) {
  if (is_origin_plane_reference(reference_id)) {
    return true;
  }
  for (const auto& feature : document.feature_history) {
    if (feature.id == reference_id && feature.kind == "construction_plane") {
      return true;
    }
  }
  return false;
}

inline SketchFeatureParameters::SketchPlaneFrame to_sketch_plane_frame(
    const PlaneFrame& frame) {
  return SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

// Extract the trailing positive integer from id strings like "feature-12",
// "doc-3", "line-7". Returns 0 if no trailing integer is present.
inline int trailing_integer(const std::string& id) {
  size_t i = id.size();
  while (i > 0 && id[i - 1] >= '0' && id[i - 1] <= '9') {
    --i;
  }
  if (i == id.size()) {
    return 0;
  }
  try {
    return std::stoi(id.substr(i));
  } catch (...) {
    return 0;
  }
}

inline constexpr double kPi = 3.14159265358979323846;

inline void mark_feature_healthy(FeatureEntry& feature) {
  feature.status = "healthy";
  feature.dependency_broken = false;
  feature.dependency_warning.clear();
}

inline std::string make_move_parameters_summary(const MoveFeatureParameters& params) {
  std::ostringstream stream;
  stream << "Move " << params.translation_x << ", " << params.translation_y
         << ", " << params.translation_z << " mm · rotate "
         << params.rotation_x_degrees << ", " << params.rotation_y_degrees
         << ", " << params.rotation_z_degrees << " deg";
  return stream.str();
}

inline std::string feature_name_by_id(const DocumentState& document,
                                      const std::string& feature_id) {
  const auto it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  return it == document.feature_history.end() ? std::string{} : it->name;
}

}  // namespace polysmith::core
