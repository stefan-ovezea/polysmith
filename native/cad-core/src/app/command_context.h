#pragma once

// Shared command-handling context for the app command handlers.
//
// These used to live in `app/impl/command_reader_basic_helpers.inc`, textually
// included into app.cpp's anonymous namespace — which made every command
// handler an `.inc` fragment of the app TU. Extracting them into a header lets
// individual handlers promote into real `.cpp` units (Stage 4 of the
// refactoring plan) without duplicating the singleton or the payload readers.
//
// `document_manager()` is `inline` with a function-local static: a static local
// in an inline function with external linkage is a single object across every
// translation unit, so the app and every promoted handler share one manager.

#include <optional>
#include <stdexcept>
#include <string>

#include "core/document/document.h"
#include "protocol/ipc.h"
#include "protocol/serialization.h"

namespace polysmith {

inline polysmith::core::DocumentManager& document_manager() {
  static polysmith::core::DocumentManager manager;
  return manager;
}

inline double read_dimension(const polysmith::protocol::json& payload,
                             const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number()) {
    throw std::runtime_error(
        std::string("Command payload is missing numeric field '") + key + "'");
  }
  return payload.at(key).get<double>();
}

inline int read_int(const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number_integer()) {
    throw std::runtime_error(
        std::string("Command payload is missing integer field '") + key + "'");
  }
  return payload.at(key).get<int>();
}

inline std::string read_string(const polysmith::protocol::json& payload,
                               const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(
        std::string("Command payload is missing string field '") + key + "'");
  }
  return payload.at(key).get<std::string>();
}

inline std::optional<std::string> read_optional_string(
    const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(
        std::string("Command payload field is not a string '") + key + "'");
  }
  return payload.at(key).get<std::string>();
}

inline bool read_bool(const polysmith::protocol::json& payload,
                      const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_boolean()) {
    throw std::runtime_error(
        std::string("Command payload is missing bool field '") + key + "'");
  }
  return payload.at(key).get<bool>();
}

inline double read_optional_dimension(const polysmith::protocol::json& payload,
                                      const char* key, double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(
        std::string("Command payload field is not numeric '") + key + "'");
  }
  return payload.at(key).get<double>();
}

// Emits the phase-D2 reply for a command that changed ONE feature's stored
// parameters in place (extrude depth, hole size, fillet radius, ...). Returns
// true when the event was written (the caller stops); false means "send the
// full state". Moved out of `app/impl/feature_changed.inc` so promoted handler
// units can share it.
inline bool emit_feature_changed(
    const std::string& command_id,
    const std::string& feature_id,
    const polysmith::core::DocumentState& after) {
  const polysmith::core::FeatureEntry* after_feature = nullptr;
  for (const auto& feature : after.feature_history) {
    if (feature.id == feature_id) {
      after_feature = &feature;
      break;
    }
  }
  if (after_feature == nullptr) {
    return false;
  }
  polysmith::protocol::write_message(
      polysmith::protocol::make_feature_changed_event(
          command_id,
          polysmith::protocol::json{
              {"revision", after.revision},
              {"feature_id", feature_id},
              {"feature",
               polysmith::protocol::to_payload(*after_feature,
                                               /*include_opaque=*/false,
                                               /*include_vertices=*/false,
                                               /*include_profiles=*/false)},
              {"removed_feature_ids", polysmith::protocol::json::array()},
          }));
  return true;
}

// Reads the fillet/chamfer edge selection: an `edge_ids` array, or a legacy
// single `edge_id` string when `require_one` is set.
inline std::vector<std::string> read_edge_ids(
    const polysmith::protocol::json& payload, bool require_one) {
  std::vector<std::string> edge_ids;
  if (payload.contains("edge_ids") && payload.at("edge_ids").is_array()) {
    for (const auto& entry : payload.at("edge_ids")) {
      if (entry.is_string()) {
        edge_ids.push_back(entry.get<std::string>());
      }
    }
  } else if (require_one) {
    edge_ids.push_back(read_string(payload, "edge_id"));
  }
  return edge_ids;
}

// Parses a semantic hole feature's parameters (hole_type, extent, diameter,
// counterbore/countersink, thread) from the command payload. Moved out of
// `command_reader_threaded_feature_helpers.inc` so the hole handler can be a
// promoted unit.
inline polysmith::core::HoleFeatureParameters read_hole_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::HoleFeatureParameters params{};
  params.hole_type =
      read_optional_string(source, "hole_type").value_or(params.hole_type);
  params.extent_type =
      read_optional_string(source, "extent_type").value_or(params.extent_type);
  params.diameter =
      read_optional_dimension(source, "diameter", params.diameter);
  params.depth = read_optional_dimension(source, "depth", params.depth);
  params.counterbore_diameter = read_optional_dimension(
      source, "counterbore_diameter", params.counterbore_diameter);
  params.counterbore_depth = read_optional_dimension(
      source, "counterbore_depth", params.counterbore_depth);
  params.countersink_diameter = read_optional_dimension(
      source, "countersink_diameter", params.countersink_diameter);
  params.countersink_angle_degrees = read_optional_dimension(
      source, "countersink_angle_degrees", params.countersink_angle_degrees);
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.standard_size =
      read_optional_string(source, "standard_size").value_or(params.standard_size);
  params.hole_fit =
      read_optional_string(source, "hole_fit").value_or(params.hole_fit);
  if (source.contains("thread_enabled") && source.at("thread_enabled").is_boolean()) {
    params.thread_enabled = source.at("thread_enabled").get<bool>();
  }
  params.thread_spec =
      read_optional_string(source, "thread_spec").value_or(params.thread_spec);
  params.thread_pitch =
      read_optional_dimension(source, "thread_pitch", params.thread_pitch);
  params.major_diameter =
      read_optional_dimension(source, "major_diameter", params.major_diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.thread_depth =
      read_optional_dimension(source, "thread_depth", params.thread_depth);
  params.thread_representation =
      read_optional_string(source, "thread_representation")
          .value_or(params.thread_representation);
  return params;
}

inline polysmith::core::HelixFeatureParameters read_helix_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::HelixFeatureParameters params{};
  params.radius = read_optional_dimension(source, "radius", params.radius);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.height = read_optional_dimension(source, "height", params.height);
  params.handedness =
      read_optional_string(source, "handedness").value_or(params.handedness);
  params.start_angle_degrees = read_optional_dimension(
      source, "start_angle_degrees", params.start_angle_degrees);
  return params;
}

inline polysmith::core::ThreadFeatureParameters read_thread_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::ThreadFeatureParameters params{};
  params.target_body_id =
      read_optional_string(source, "target_body_id").value_or(params.target_body_id);
  params.axis_source_id =
      read_optional_string(source, "axis_source_id").value_or(params.axis_source_id);
  params.mode = read_optional_string(source, "mode").value_or(params.mode);
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.size = read_optional_string(source, "size").value_or(params.size);
  params.major_diameter =
      read_optional_dimension(source, "major_diameter", params.major_diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.length = read_optional_dimension(source, "length", params.length);
  params.thread_angle_degrees =
      read_optional_dimension(source, "thread_angle_degrees", params.thread_angle_degrees);
  params.start_offset =
      read_optional_dimension(source, "start_offset", params.start_offset);
  params.handedness =
      read_optional_string(source, "handedness").value_or(params.handedness);
  params.representation =
      read_optional_string(source, "representation").value_or(params.representation);
  return params;
}

inline polysmith::core::FastenerFeatureParameters read_fastener_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::FastenerFeatureParameters params{};
  params.standard =
      read_optional_string(source, "standard").value_or(params.standard);
  params.size = read_optional_string(source, "size").value_or(params.size);
  params.diameter =
      read_optional_dimension(source, "diameter", params.diameter);
  params.minor_diameter =
      read_optional_dimension(source, "minor_diameter", params.minor_diameter);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.length = read_optional_dimension(source, "length", params.length);
  params.thread_length =
      read_optional_dimension(source, "thread_length", params.thread_length);
  params.head_type =
      read_optional_string(source, "head_type").value_or(params.head_type);
  params.drive_type =
      read_optional_string(source, "drive_type").value_or(params.drive_type);
  params.thread_representation =
      read_optional_string(source, "thread_representation")
          .value_or(params.thread_representation);
  return params;
}

inline polysmith::core::MoveFeatureParameters read_move_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::MoveFeatureParameters params{};
  params.target_body_id =
      read_optional_string(source, "target_body_id").value_or(params.target_body_id);
  params.translation_x =
      read_optional_dimension(source, "translation_x", params.translation_x);
  params.translation_y =
      read_optional_dimension(source, "translation_y", params.translation_y);
  params.translation_z =
      read_optional_dimension(source, "translation_z", params.translation_z);
  params.rotation_x_degrees =
      read_optional_dimension(source, "rotation_x_degrees", params.rotation_x_degrees);
  params.rotation_y_degrees =
      read_optional_dimension(source, "rotation_y_degrees", params.rotation_y_degrees);
  params.rotation_z_degrees =
      read_optional_dimension(source, "rotation_z_degrees", params.rotation_z_degrees);
  if (source.contains("is_pending") && source.at("is_pending").is_boolean()) {
    params.is_pending = source.at("is_pending").get<bool>();
  }
  return params;
}

inline polysmith::core::ExtrudeFeatureParameters::SideParameters read_extrude_side(
    const polysmith::protocol::json& payload,
    const char* key,
    const polysmith::core::ExtrudeFeatureParameters::SideParameters& fallback) {
  polysmith::core::ExtrudeFeatureParameters::SideParameters side = fallback;
  if (!payload.contains(key) || !payload.at(key).is_object()) {
    return side;
  }
  const auto& side_payload = payload.at(key);
  side.extent_type =
      read_optional_string(side_payload, "extent_type").value_or(side.extent_type);
  side.distance =
      read_optional_dimension(side_payload, "distance", side.distance);
  side.start_offset =
      read_optional_dimension(side_payload, "start_offset", side.start_offset);
  side.taper_angle_degrees =
      read_optional_dimension(side_payload, "taper_angle_degrees",
                              side.taper_angle_degrees);
  side.target_reference_id =
      read_optional_string(side_payload, "target_reference_id");
  return side;
}

inline std::optional<polysmith::core::ExtrudeFeatureParameters>
read_optional_extrude_parameters(
    const polysmith::protocol::json& payload, double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id) {
  const bool has_parameter_fields =
      payload.contains("extent_mode") || payload.contains("side1") ||
      payload.contains("side2") || payload.contains("thin") ||
      payload.contains("operation") || payload.contains("intersect_result");
  if (!has_parameter_fields && !payload.contains("parameters")) {
    return std::nullopt;
  }

  if (payload.contains("parameters") && payload.at("parameters").is_object() &&
      payload.at("parameters").contains("sketch_feature_id")) {
    return polysmith::protocol::extrude_parameters_from_payload(
        payload.at("parameters"));
  }

  const auto& parameter_payload =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  polysmith::core::ExtrudeFeatureParameters params{};
  params.depth = depth;
  params.mode = mode;
  params.operation =
      read_optional_string(parameter_payload, "operation")
          .value_or(mode.empty() ? "new_body" : mode);
  params.target_body_id = target_body_id;
  params.extent_mode =
      read_optional_string(parameter_payload, "extent_mode").value_or("one_side");
  params.side1.distance = std::abs(depth);
  params.side1 = read_extrude_side(parameter_payload, "side1", params.side1);
  if (parameter_payload.contains("side2") &&
      parameter_payload.at("side2").is_object()) {
    params.side2 =
        read_extrude_side(parameter_payload, "side2", params.side1);
  }
  if (parameter_payload.contains("thin") &&
      parameter_payload.at("thin").is_object()) {
    const auto& thin = parameter_payload.at("thin");
    if (thin.contains("enabled") && thin.at("enabled").is_boolean()) {
      params.thin.enabled = thin.at("enabled").get<bool>();
    }
    params.thin.thickness =
        read_optional_dimension(thin, "thickness", params.thin.thickness);
    params.thin.placement =
        read_optional_string(thin, "placement").value_or(params.thin.placement);
  }
  params.intersect_result =
      read_optional_string(parameter_payload, "intersect_result")
          .value_or(params.intersect_result);
  return params;
}

// True when the ONLY differences between two sketch states are the ones a
// `sketch_entities_changed` event can carry: line endpoints and vertex
// coordinates.
//
// The partial event was designed for the high-frequency edit path (vertex /
// line drags) and its payload has no room for arcs, circles, fillets,
// chamfers, texts, constraints... yet earlier it was emitted for every
// command whose DIMENSION list happened to be unchanged. That left the
// client's copy of those entities stale — the classic symptom being the
// planegcs bridge refusing a drag with "sketch object vertex-N not found"
// because a stale arc still referenced a vertex the patch had removed.
//
// Comparing the serialized non-line state is exact (every field is covered,
// including fields added later) and cheaper than the O(n²) id scans it
// replaces on a dense sketch.
inline bool sketch_partial_event_safe(
    const polysmith::core::FeatureEntry& before_feature,
    const polysmith::core::FeatureEntry& after_feature) {
  const auto normalized = [](const polysmith::core::FeatureEntry& feature) {
    const auto payload = polysmith::protocol::to_payload(feature);
    if (!payload.contains("sketch_parameters") ||
        !payload.at("sketch_parameters").is_object()) {
      return polysmith::protocol::json{};
    }
    auto params = payload.at("sketch_parameters");
    // The partial event transports these (and only these); everything else
    // must be identical.
    params.erase("lines");
    params.erase("vertices");
    params.erase("profiles");
    params.erase("profile_count");
    params.erase("vertex_count");
    // Derived/excluded from the event and meaningless for the comparison.
    params.erase("next_vertex_index");
    return params;
  };
  return normalized(before_feature) == normalized(after_feature);
}

// Emits the phase-D1/D2 reply for a command that changed sketch entities.
// Returns true when the event was written (the caller stops); false means
// "send the full state". Moved out of `app/impl/sketch_entities_changed.inc`
// so promoted sketch handlers can share it.
inline bool emit_sketch_entities_changed(
    const std::string& command_id,
    const std::optional<polysmith::core::DocumentState>& before,
    const polysmith::core::DocumentState& after) {
  const std::string active_id = after.active_sketch_feature_id.value_or("");
  if (!before.has_value() || active_id.empty()) {
    return false;
  }
  const polysmith::core::FeatureEntry* before_feature = nullptr;
  const polysmith::core::FeatureEntry* after_feature = nullptr;
  for (const auto& feature : before->feature_history) {
    if (feature.id == active_id) {
      before_feature = &feature;
    }
  }
  for (const auto& feature : after.feature_history) {
    if (feature.id == active_id) {
      after_feature = &feature;
    }
  }
  if (before_feature == nullptr || after_feature == nullptr ||
      !before_feature->sketch_parameters.has_value() ||
      !after_feature->sketch_parameters.has_value()) {
    return false;
  }
  const auto& before_params = before_feature->sketch_parameters.value();
  const auto& after_params = after_feature->sketch_parameters.value();

  if (before_params.dimensions.size() != after_params.dimensions.size()) {
    return false;
  }
  for (std::size_t i = 0; i < after_params.dimensions.size(); ++i) {
    if (before_params.dimensions[i].id != after_params.dimensions[i].id) {
      return false;
    }
  }

  // Anything the event cannot carry (arcs, circles, fillets, chamfers,
  // texts, constraints, ...) must be byte-identical; otherwise the client
  // would keep a stale copy of it. See `sketch_partial_event_safe`.
  if (!sketch_partial_event_safe(*before_feature, *after_feature)) {
    return false;
  }

  polysmith::protocol::json changed_vertices = polysmith::protocol::json::array();
  polysmith::protocol::json changed_lines = polysmith::protocol::json::array();
  polysmith::protocol::json removed_ids = polysmith::protocol::json::array();

  for (const auto& vertex : after_params.vertices) {
    const polysmith::core::SketchVertex* previous = nullptr;
    for (const auto& candidate : before_params.vertices) {
      if (candidate.vertex_id == vertex.vertex_id) {
        previous = &candidate;
        break;
      }
    }
    if (previous == nullptr || previous->x != vertex.x ||
        previous->y != vertex.y) {
      changed_vertices.push_back({
          {"vertex_id", vertex.vertex_id},
          {"x", vertex.x},
          {"y", vertex.y},
      });
    }
  }
  for (const auto& vertex : before_params.vertices) {
    bool still_there = false;
    for (const auto& candidate : after_params.vertices) {
      if (candidate.vertex_id == vertex.vertex_id) {
        still_there = true;
        break;
      }
    }
    if (!still_there) {
      removed_ids.push_back(vertex.vertex_id);
    }
  }
  for (const auto& line : after_params.lines) {
    const polysmith::core::SketchLine* previous = nullptr;
    for (const auto& candidate : before_params.lines) {
      if (candidate.id == line.id) {
        previous = &candidate;
        break;
      }
    }
    if (previous == nullptr || previous->start_x != line.start_x ||
        previous->start_y != line.start_y || previous->end_x != line.end_x ||
        previous->end_y != line.end_y) {
      changed_lines.push_back({
          {"line_id", line.id},
          {"start_x", line.start_x},
          {"start_y", line.start_y},
          {"end_x", line.end_x},
          {"end_y", line.end_y},
      });
    }
  }
  for (const auto& line : before_params.lines) {
    bool still_there = false;
    for (const auto& candidate : after_params.lines) {
      if (candidate.id == line.id) {
        still_there = true;
        break;
      }
    }
    if (!still_there) {
      removed_ids.push_back(line.id);
    }
  }

  if (changed_vertices.empty() && changed_lines.empty() && removed_ids.empty()) {
    return false;
  }
  polysmith::protocol::write_message(
      polysmith::protocol::make_sketch_entities_changed_event(
          command_id,
          polysmith::protocol::json{
              {"revision", after.revision},
              {"feature_id", active_id},
              {"lines", changed_lines},
              {"vertices", changed_vertices},
              {"removed", removed_ids},
              {"derived",
               polysmith::protocol::json{
                   {"profile_count", after_params.profiles.size()},
                   {"vertex_count", after_params.vertices.size()},
               }},
          }));
  return true;
}

inline polysmith::core::SketchFeatureParameters::SketchPlaneFrame
read_plane_frame(const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_object()) {
    throw std::runtime_error(
        std::string("Command payload is missing object field '") + key + "'");
  }
  const auto& frame = payload.at(key);
  const auto read_vector = [&](const char* vector_key) {
    if (!frame.contains(vector_key) || !frame.at(vector_key).is_object()) {
      throw std::runtime_error(
          std::string("Command payload is missing plane frame field '") +
          vector_key + "'");
    }
    const auto& vector = frame.at(vector_key);
    return polysmith::protocol::json{
        {"x", vector.at("x").get<double>()},
        {"y", vector.at("y").get<double>()},
        {"z", vector.at("z").get<double>()},
    };
  };
  const auto origin = read_vector("origin");
  const auto x_axis = read_vector("x_axis");
  const auto y_axis = read_vector("y_axis");
  const auto normal = read_vector("normal");
  return polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = origin.at("x").get<double>(),
      .origin_y = origin.at("y").get<double>(),
      .origin_z = origin.at("z").get<double>(),
      .x_axis_x = x_axis.at("x").get<double>(),
      .x_axis_y = x_axis.at("y").get<double>(),
      .x_axis_z = x_axis.at("z").get<double>(),
      .y_axis_x = y_axis.at("x").get<double>(),
      .y_axis_y = y_axis.at("y").get<double>(),
      .y_axis_z = y_axis.at("z").get<double>(),
      .normal_x = normal.at("x").get<double>(),
      .normal_y = normal.at("y").get<double>(),
      .normal_z = normal.at("z").get<double>(),
  };
}

inline std::string read_optional_string_value(
    const polysmith::protocol::json& payload, const char* key,
    const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(
        std::string("Command payload field is not string '") + key + "'");
  }
  return payload.at(key).get<std::string>();
}

inline double read_optional_number_value(
    const polysmith::protocol::json& payload, const char* key, double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(
        std::string("Command payload field is not numeric '") + key + "'");
  }
  return payload.at(key).get<double>();
}

inline int read_optional_int_value(const polysmith::protocol::json& payload,
                                   const char* key, int fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number_integer()) {
    throw std::runtime_error(
        std::string("Command payload field is not integer '") + key + "'");
  }
  return payload.at(key).get<int>();
}

inline std::vector<polysmith::core::PluginProfilePoint>
read_optional_plugin_profile_points(const polysmith::protocol::json& payload,
                                    const char* key) {
  std::vector<polysmith::core::PluginProfilePoint> points;
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return points;
  }
  if (!payload.at(key).is_array()) {
    throw std::runtime_error(
        std::string("Command payload field is not array '") + key + "'");
  }
  for (const auto& point_payload : payload.at(key)) {
    if (!point_payload.is_object()) {
      throw std::runtime_error("Plugin profile point must be an object");
    }
    points.push_back(polysmith::core::PluginProfilePoint{
        .u = read_optional_number_value(point_payload, "u", 0.0),
        .v = read_optional_number_value(point_payload, "v", 0.0),
    });
  }
  return points;
}

inline polysmith::core::PluginGeometryOperation read_plugin_geometry_operation(
    const polysmith::protocol::json& payload) {
  if (!payload.is_object()) {
    throw std::runtime_error("Plugin geometry operation must be an object");
  }
  polysmith::core::PluginGeometryOperation operation{};
  operation.operation = read_optional_string_value(payload, "operation", "add");
  operation.primitive = read_optional_string_value(payload, "primitive", "box");
  operation.x = read_optional_number_value(payload, "x", operation.x);
  operation.y = read_optional_number_value(payload, "y", operation.y);
  operation.z = read_optional_number_value(payload, "z", operation.z);
  operation.width = read_optional_number_value(payload, "width", operation.width);
  operation.depth = read_optional_number_value(payload, "depth", operation.depth);
  operation.height = read_optional_number_value(payload, "height", operation.height);
  operation.radius = read_optional_number_value(payload, "radius", operation.radius);
  operation.top_width =
      read_optional_number_value(payload, "top_width", operation.top_width);
  operation.top_depth =
      read_optional_number_value(payload, "top_depth", operation.top_depth);
  operation.top_radius =
      read_optional_number_value(payload, "top_radius", operation.top_radius);
  operation.top_offset_x =
      read_optional_number_value(payload, "top_offset_x", operation.top_offset_x);
  operation.top_offset_y =
      read_optional_number_value(payload, "top_offset_y", operation.top_offset_y);
  operation.profile_plane =
      read_optional_string_value(payload, "profile_plane", operation.profile_plane);
  operation.extrude_x =
      read_optional_number_value(payload, "extrude_x", operation.extrude_x);
  operation.extrude_y =
      read_optional_number_value(payload, "extrude_y", operation.extrude_y);
  operation.extrude_z =
      read_optional_number_value(payload, "extrude_z", operation.extrude_z);
  operation.path_width =
      read_optional_number_value(payload, "path_width", operation.path_width);
  operation.path_depth =
      read_optional_number_value(payload, "path_depth", operation.path_depth);
  operation.path_radius =
      read_optional_number_value(payload, "path_radius", operation.path_radius);
  operation.profile_points =
      read_optional_plugin_profile_points(payload, "profile_points");
  return operation;
}

inline polysmith::core::PluginFeatureParameters
read_plugin_feature_parameters(const polysmith::protocol::json& payload) {
  polysmith::core::PluginFeatureParameters parameters{};
  parameters.plugin_id = read_string(payload, "plugin_id");
  parameters.feature_type = read_string(payload, "feature_type");
  parameters.schema_version =
      read_optional_int_value(payload, "schema_version", parameters.schema_version);
  parameters.display_name =
      read_optional_string_value(payload, "display_name", parameters.display_name);
  parameters.parameters_summary =
      read_optional_string_value(payload, "parameters_summary",
                                 parameters.parameters_summary);
  if (payload.contains("parameters") && !payload.at("parameters").is_null()) {
    parameters.parameters_json = payload.at("parameters").dump();
  }
  if (!payload.contains("geometry") || !payload.at("geometry").is_array()) {
    throw std::runtime_error("Plugin feature command is missing geometry");
  }
  for (const auto& operation_payload : payload.at("geometry")) {
    parameters.geometry.push_back(
        read_plugin_geometry_operation(operation_payload));
  }
  return parameters;
}

}  // namespace polysmith
