#include "app.h"

#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include "core/document.h"
#include "core/formula_eval.h"
#include "core/logger.h"
#include "core/snap_engine.h"
#include "core/trim_engine.h"
#include "core/viewport.h"
#include "protocol/ipc.h"
#include "protocol/serialization.h"

namespace polysmith {
namespace {

using polysmith::core::DocumentManager;
using polysmith::core::BoxFeatureParameters;
using polysmith::core::CylinderFeatureParameters;
using polysmith::core::ExtrudeFeatureParameters;
using polysmith::core::FastenerFeatureParameters;
using polysmith::core::HelixFeatureParameters;
using polysmith::core::HoleFeatureParameters;
using polysmith::core::ImageImportFeatureParameters;
using polysmith::core::MoveFeatureParameters;
using polysmith::core::PlaneFrame;
using polysmith::core::SvgImportFeatureParameters;
using polysmith::core::ThreadFeatureParameters;
using polysmith::protocol::CommandMessage;

DocumentManager& document_manager() {
  static DocumentManager manager;
  return manager;
}

double read_dimension(const polysmith::protocol::json& payload,
                      const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload is missing numeric field '") +
                             key + "'");
  }

  return payload.at(key).get<double>();
}

int read_int(const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("Command payload is missing integer field '") +
                             key + "'");
  }

  return payload.at(key).get<int>();
}

std::string read_string(const polysmith::protocol::json& payload,
                        const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload is missing string field '") +
                             key + "'");
  }

  return payload.at(key).get<std::string>();
}

std::optional<std::string> read_optional_string(
    const polysmith::protocol::json& payload,
    const char* key) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload field is not a string '") +
                             key + "'");
  }
  return payload.at(key).get<std::string>();
}

double read_optional_dimension(const polysmith::protocol::json& payload,
                               const char* key,
                               double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload field is not numeric '") +
                             key + "'");
  }
  return payload.at(key).get<double>();
}

ExtrudeFeatureParameters::SideParameters read_extrude_side(
    const polysmith::protocol::json& payload,
    const char* key,
    const ExtrudeFeatureParameters::SideParameters& fallback) {
  ExtrudeFeatureParameters::SideParameters side = fallback;
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
      read_optional_dimension(side_payload,
                              "taper_angle_degrees",
                              side.taper_angle_degrees);
  side.target_reference_id =
      read_optional_string(side_payload, "target_reference_id");
  return side;
}

std::optional<ExtrudeFeatureParameters> read_optional_extrude_parameters(
    const polysmith::protocol::json& payload,
    double depth,
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
  ExtrudeFeatureParameters params{};
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

PlaneFrame read_plane_frame(const polysmith::protocol::json& payload) {
  PlaneFrame frame{};
  frame.origin_x = payload.at("origin").at("x").get<double>();
  frame.origin_y = payload.at("origin").at("y").get<double>();
  frame.origin_z = payload.at("origin").at("z").get<double>();
  frame.x_axis_x = payload.at("x_axis").at("x").get<double>();
  frame.x_axis_y = payload.at("x_axis").at("y").get<double>();
  frame.x_axis_z = payload.at("x_axis").at("z").get<double>();
  frame.y_axis_x = payload.at("y_axis").at("x").get<double>();
  frame.y_axis_y = payload.at("y_axis").at("y").get<double>();
  frame.y_axis_z = payload.at("y_axis").at("z").get<double>();
  frame.normal_x = payload.at("normal").at("x").get<double>();
  frame.normal_y = payload.at("normal").at("y").get<double>();
  frame.normal_z = payload.at("normal").at("z").get<double>();
  return frame;
}

ImageImportFeatureParameters read_import_placement(
    const polysmith::protocol::json& payload) {
  ImageImportFeatureParameters params{};
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && payload.at("plane_frame").is_object()) {
    params.plane_frame = read_plane_frame(payload.at("plane_frame"));
  }
  params.asset_id = read_optional_string(payload, "asset_id").value_or("");
  params.asset_path = read_optional_string(payload, "asset_path").value_or("");
  params.relative_asset_path =
      read_optional_string(payload, "relative_asset_path").value_or("");
  params.file_name = read_optional_string(payload, "file_name").value_or("");
  params.media_type = read_optional_string(payload, "media_type").value_or("");
  params.source_width =
      read_optional_dimension(payload, "source_width", params.source_width);
  params.source_height =
      read_optional_dimension(payload, "source_height", params.source_height);
  params.offset_u_mm =
      read_optional_dimension(payload, "offset_u_mm", params.offset_u_mm);
  params.offset_v_mm =
      read_optional_dimension(payload, "offset_v_mm", params.offset_v_mm);
  params.rotation_degrees =
      read_optional_dimension(payload, "rotation_degrees", params.rotation_degrees);
  params.width_mm = read_optional_dimension(payload, "width_mm", params.width_mm);
  params.height_mm =
      read_optional_dimension(payload, "height_mm", params.height_mm);
  if (payload.contains("lock_aspect") && payload.at("lock_aspect").is_boolean()) {
    params.lock_aspect = payload.at("lock_aspect").get<bool>();
  }
  if (payload.contains("is_pending") && payload.at("is_pending").is_boolean()) {
    params.is_pending = payload.at("is_pending").get<bool>();
  }
  return params;
}

HoleFeatureParameters read_hole_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  HoleFeatureParameters params{};
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

HelixFeatureParameters read_helix_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  HelixFeatureParameters params{};
  params.radius = read_optional_dimension(source, "radius", params.radius);
  params.pitch = read_optional_dimension(source, "pitch", params.pitch);
  params.height = read_optional_dimension(source, "height", params.height);
  params.handedness =
      read_optional_string(source, "handedness").value_or(params.handedness);
  params.start_angle_degrees = read_optional_dimension(
      source, "start_angle_degrees", params.start_angle_degrees);
  return params;
}

ThreadFeatureParameters read_thread_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  ThreadFeatureParameters params{};
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

FastenerFeatureParameters read_fastener_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  FastenerFeatureParameters params{};
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

MoveFeatureParameters read_move_parameters(
    const polysmith::protocol::json& payload) {
  const auto& source =
      payload.contains("parameters") && payload.at("parameters").is_object()
          ? payload.at("parameters")
          : payload;
  MoveFeatureParameters params{};
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

polysmith::core::SketchFeatureParameters::SketchPlaneFrame read_plane_frame(
    const polysmith::protocol::json& payload,
    const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_object()) {
    throw std::runtime_error(std::string("Command payload is missing object field '") +
                             key + "'");
  }

  const auto& frame = payload.at(key);
  const auto read_vector = [&](const char* vector_key) {
    if (!frame.contains(vector_key) || !frame.at(vector_key).is_object()) {
      throw std::runtime_error(std::string("Command payload is missing plane frame field '") +
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

}  // namespace

void CadCoreApp::init_occt() const {
  polysmith::core::log_info("cad_core", "Starting OCCT smoke test...");

  const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

  if (box.IsNull()) {
    polysmith::core::log_error("cad_core", "OCCT smoke test failed: shape is null");
    return;
  }

  polysmith::core::log_info("cad_core", "OCCT box created successfully");
}

void CadCoreApp::handle_command_line(const std::string& line) {
  const CommandMessage command = polysmith::protocol::parse_command(line);

  if (command.type == "ping") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_pong_event(command.id));
    return;
  }

  if (command.type == "create_document") {
    const auto document = document_manager().create_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_created_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "get_document_state") {
    const auto document = document_manager().get_document();

    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return;
  }

  if (command.type == "get_session_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_session_state_event(
            command.id,
            polysmith::protocol::to_payload(
                document_manager().get_session_state())));
    return;
  }

  if (command.type == "get_viewport_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_viewport_state_event(
            command.id,
            polysmith::protocol::to_payload(polysmith::core::build_viewport_state(
                document_manager().get_document()))));
    return;
  }

  if (command.type == "export_document") {
    const auto export_result = document_manager().export_document_as_step(
        read_string(command.payload, "file_path"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id,
            polysmith::protocol::json{
                {"file_path", export_result.file_path},
                {"format", export_result.format},
                {"exported_feature_count", export_result.exported_feature_count},
            }));
    return;
  }

  if (command.type == "export_document_stl") {
    const auto export_result = document_manager().export_document_as_stl(
        read_string(command.payload, "file_path"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id,
            polysmith::protocol::json{
                {"file_path", export_result.file_path},
                {"format", export_result.format},
                {"exported_feature_count", export_result.exported_feature_count},
            }));
    return;
  }

  if (command.type == "export_body_stl") {
    const auto export_result = document_manager().export_body_as_stl(
        read_string(command.payload, "file_path"),
        read_string(command.payload, "body_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id,
            polysmith::protocol::json{
                {"file_path", export_result.file_path},
                {"format", export_result.format},
                {"exported_feature_count", export_result.exported_feature_count},
            }));
    return;
  }

  if (command.type == "save_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    document_manager().save_document_to_path(file_path);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_saved_event(command.id, file_path));
    return;
  }

  if (command.type == "load_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    const auto document =
        document_manager().load_document_from_path(file_path);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_image_import") {
    const auto document = document_manager().create_image_import(
        read_import_placement(command.payload),
        read_string(command.payload, "source_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_image_import") {
    const auto document = document_manager().update_image_import(
        read_string(command.payload, "feature_id"),
        read_import_placement(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_image_import") {
    const auto document = document_manager().confirm_image_import(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cancel_image_import") {
    const auto document = document_manager().cancel_image_import(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_svg_import") {
    const auto document = document_manager().create_svg_import(
        read_import_placement(command.payload),
        read_string(command.payload, "source_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_svg_import") {
    const auto document = document_manager().update_svg_import(
        read_string(command.payload, "feature_id"),
        read_import_placement(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_svg_import") {
    const auto document = document_manager().confirm_svg_import(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cancel_svg_import") {
    const auto document = document_manager().cancel_svg_import(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_box_feature") {
    const auto document = document_manager().add_box_feature(BoxFeatureParameters{
        .width = read_dimension(command.payload, "width"),
        .height = read_dimension(command.payload, "height"),
        .depth = read_dimension(command.payload, "depth"),
    });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_cylinder_feature") {
    const auto document =
        document_manager().add_cylinder_feature(CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_box_feature") {
    const auto document = document_manager().update_box_feature(
        read_string(command.payload, "feature_id"),
        BoxFeatureParameters{
            .width = read_dimension(command.payload, "width"),
            .height = read_dimension(command.payload, "height"),
            .depth = read_dimension(command.payload, "depth"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_cylinder_feature") {
    const auto document = document_manager().update_cylinder_feature(
        read_string(command.payload, "feature_id"),
        CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_depth") {
    const auto document = document_manager().update_extrude_depth(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "depth"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "rename_feature") {
    const auto document = document_manager().rename_feature(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "name"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_feature_suppressed") {
    const auto document = document_manager().set_feature_suppressed(
        read_string(command.payload, "feature_id"),
        command.payload.at("suppressed").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_feature") {
    const auto document =
        document_manager().delete_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "undo") {
    const auto document = document_manager().undo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "redo") {
    const auto document = document_manager().redo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_timeline_cursor") {
    const auto document = document_manager().set_timeline_cursor(
        read_int(command.payload, "included_action_count"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_feature") {
    const auto document =
        document_manager().select_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_reference") {
    const auto document = document_manager().select_reference(
        read_string(command.payload, "reference_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_face") {
    const auto document =
        document_manager().select_face(read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_edge") {
    // `additive` toggles the edge into / out of the multi-select set
    // (shift-click). It defaults to false so old payloads (which only
    // sent `edge_id`) keep behaving as a plain replace-style select.
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_edge(
        read_string(command.payload, "edge_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_vertex") {
    // `additive` toggles the vertex into the multi-select set
    // (shift-click). Defaults to false for back-compat with payloads
    // that only carry `vertex_id`.
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_vertex(
        read_string(command.payload, "vertex_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_body_color") {
    const auto document = document_manager().set_body_color(
        read_string(command.payload, "body_id"),
        read_string(command.payload, "color"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_face_color") {
    const auto document = document_manager().set_face_color(
        read_string(command.payload, "face_id"),
        read_string(command.payload, "color"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_body_color") {
    const auto document = document_manager().clear_body_color(
        read_string(command.payload, "body_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_face_color") {
    const auto document = document_manager().clear_face_color(
        read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_appearance_overrides") {
    const auto document = document_manager().clear_appearance_overrides();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_fillet") {
    // Accept either a single `edge_id` (legacy single-edge payload)
    // or an `edge_ids` array (multi-select). Reading both so old
    // clients that haven't migrated still work.
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    } else {
      edge_ids.push_back(read_string(command.payload, "edge_id"));
    }
    const auto document = document_manager().create_fillet(
        edge_ids, read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fillet_edges") {
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_fillet_edges(
        read_string(command.payload, "feature_id"), edge_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fillet_radius") {
    const auto document = document_manager().update_fillet_radius(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_fillet") {
    const auto document = document_manager().confirm_fillet(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_chamfer") {
    // Same dual-payload handling as create_fillet — see comment there.
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    } else {
      edge_ids.push_back(read_string(command.payload, "edge_id"));
    }
    const auto document = document_manager().create_chamfer(
        edge_ids, read_dimension(command.payload, "distance"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_chamfer_edges") {
    std::vector<std::string> edge_ids;
    if (command.payload.contains("edge_ids") &&
        command.payload.at("edge_ids").is_array()) {
      for (const auto& entry : command.payload.at("edge_ids")) {
        if (entry.is_string()) {
          edge_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_chamfer_edges(
        read_string(command.payload, "feature_id"), edge_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_chamfer_distance") {
    const auto document = document_manager().update_chamfer_distance(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "distance"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_chamfer") {
    const auto document = document_manager().confirm_chamfer(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_shell") {
    const auto document = document_manager().create_shell(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_shell_thickness") {
    const auto document = document_manager().update_shell_thickness(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "thickness"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_shell") {
    const auto document = document_manager().confirm_shell(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_offset_plane") {
    const auto document = document_manager().create_offset_plane(
        read_string(command.payload, "source_plane_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_midplane") {
    std::vector<std::string> source_plane_ids;
    if (command.payload.contains("source_plane_ids") &&
        command.payload.at("source_plane_ids").is_array()) {
      for (const auto& id : command.payload.at("source_plane_ids")) {
        if (id.is_string()) {
          source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (source_plane_ids.size() != 2) {
      throw std::runtime_error("create_midplane requires two source_plane_ids");
    }
    const auto document = document_manager().create_midplane(
        source_plane_ids[0], source_plane_ids[1]);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_tangent_plane") {
    const auto document = document_manager().create_tangent_plane(
        read_string(command.payload, "source_face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_angle_plane") {
    const auto document = document_manager().create_angle_plane(
        read_string(command.payload, "source_plane_id"),
        read_string(command.payload, "source_axis_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_axis") {
    const auto document = document_manager().create_construction_axis(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_construction_point") {
    const auto document = document_manager().create_construction_point(
        read_string(command.payload, "source_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_hole") {
    const auto document = document_manager().create_hole(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "center_z"),
        read_hole_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_hole_parameters") {
    const auto document = document_manager().update_hole_parameters(
        read_string(command.payload, "feature_id"),
        read_hole_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_hole") {
    const auto document = document_manager().confirm_hole(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_helix") {
    const auto document = document_manager().create_helix(
        read_string(command.payload, "axis_source_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_helix_parameters") {
    const auto document = document_manager().update_helix_parameters(
        read_string(command.payload, "feature_id"),
        read_helix_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_thread") {
    const auto document = document_manager().create_thread(
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_thread_parameters") {
    const auto document = document_manager().update_thread_parameters(
        read_string(command.payload, "feature_id"),
        read_thread_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_thread") {
    const auto document = document_manager().confirm_thread(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_fastener") {
    const auto document = document_manager().create_fastener(
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_fastener_parameters") {
    const auto document = document_manager().update_fastener_parameters(
        read_string(command.payload, "feature_id"),
        read_fastener_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_move") {
    const auto document = document_manager().create_move(
        read_string(command.payload, "target_body_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_move_parameters") {
    const auto document = document_manager().update_move_parameters(
        read_string(command.payload, "feature_id"),
        read_move_parameters(command.payload));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "confirm_move") {
    const auto document = document_manager().confirm_move(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "create_body_copy") {
    const auto document = document_manager().create_body_copy(
        read_string(command.payload, "source_body_id"),
        read_optional_string(command.payload, "copy_mode").value_or("linked"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "unlink_body_copy") {
    const auto document = document_manager().unlink_body_copy(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_offset_plane") {
    const auto document = document_manager().update_offset_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "offset"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_angle_plane") {
    const auto document = document_manager().update_angle_plane(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "start_sketch_on_plane") {
    const auto document = document_manager().start_sketch_on_plane(
        read_string(command.payload, "reference_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "start_sketch_on_face") {
    const auto document = document_manager().start_sketch_on_face(
        read_string(command.payload, "face_id"),
        read_plane_frame(command.payload, "plane_frame"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_tool") {
    const auto document =
        document_manager().set_sketch_tool(read_string(command.payload, "tool"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_line") {
    const auto document = document_manager().update_sketch_line(
        read_string(command.payload, "line_id"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_point") {
    const auto document = document_manager().update_sketch_point(
        read_string(command.payload, "point_id"),
        read_dimension(command.payload, "x"),
        read_dimension(command.payload, "y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_line_constraint") {
    const std::string constraint = read_string(command.payload, "constraint");
    const auto document = document_manager().set_sketch_line_constraint(
        read_string(command.payload, "line_id"),
        constraint == "none" ? std::nullopt
                              : std::make_optional(constraint));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_sketch_line_constraints") {
    const auto document = document_manager().clear_sketch_line_constraints(
        read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_equal_length_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_equal_length_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_perpendicular_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_perpendicular_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "start_mirror_preview") {
    const auto document = document_manager().start_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_axis") {
    // Empty axis_line_id is a valid clear. read_string allows
    // empty strings; the core treats empty as "no axis".
    const auto document = document_manager().update_mirror_preview_axis(
        read_string(command.payload, "axis_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_objects") {
    std::vector<std::string> object_ids;
    if (command.payload.contains("object_ids") &&
        command.payload.at("object_ids").is_array()) {
      for (const auto& entry : command.payload.at("object_ids")) {
        if (entry.is_string()) {
          object_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().update_mirror_preview_objects(object_ids);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "commit_mirror_preview") {
    const auto document = document_manager().commit_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cancel_mirror_preview") {
    const auto document = document_manager().cancel_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_tangent_constraint") {
    const auto document = document_manager().set_sketch_tangent_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "circle_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_parallel_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_parallel_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_coincident_constraint") {
    const auto document = document_manager().set_sketch_coincident_constraint(
        read_string(command.payload, "point_id"),
        read_string(command.payload, "other_point_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_circle") {
    const auto document = document_manager().update_sketch_circle(
        read_string(command.payload, "circle_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension") {
    // Accept either a numeric value (backward compatible) or a string
    // expression like "width * 2". If it's a string, evaluate against
    // the current parameter table, then pass the resolved value and
    // expression to the core for storage.
    double resolved_value = 0.0;
    std::optional<std::string> expression;

    if (command.payload.contains("value")) {
      if (command.payload.at("value").is_number()) {
        resolved_value = command.payload.at("value").get<double>();
      } else if (command.payload.at("value").is_string()) {
        std::string expr_str =
            command.payload.at("value").get<std::string>();
        expression = expr_str;

        // Evaluate against current parameters
        const auto doc = document_manager().get_document();
        std::vector<core::ParameterEntry> params;
        if (doc.has_value()) {
          params = doc->parameters;
        }

        // Determine whether the target dimension is an angle kind so we
        // can type-check parameter references and apply the degrees->radians
        // conversion once, in a single pass.
        bool dim_is_angle = false;
        bool dim_is_circle_radius = false;
        if (doc.has_value() &&
            doc->active_sketch_feature_id.has_value()) {
          const auto& features = doc->feature_history;
          const std::string dim_id =
              read_string(command.payload, "dimension_id");
          const auto feat_it = std::find_if(
              features.begin(), features.end(),
              [&](const core::FeatureEntry& f) {
                return f.id == doc->active_sketch_feature_id.value();
              });
          if (feat_it != features.end() &&
              feat_it->sketch_parameters.has_value()) {
            const auto& dims = feat_it->sketch_parameters->dimensions;
            const auto dim_it = std::find_if(
                dims.begin(), dims.end(),
                [&](const core::SketchDimension& d) {
                  return d.id == dim_id;
                });
            if (dim_it != dims.end() &&
                (dim_it->kind == "angle" ||
                 dim_it->kind == "line_angle")) {
              dim_is_angle = true;
            }
            if (dim_it != dims.end() && dim_it->kind == "circle_radius") {
              dim_is_circle_radius = true;
            }
          }
        }

        auto resolver = [&params,
                          dim_is_angle](const std::string& name) -> double {
          for (const auto& p : params) {
            if (p.name == name) {
              if (p.has_error) {
                throw std::runtime_error("Parameter '" + name +
                                         "' has an unresolved expression");
              }
              // Angle-type parameter referenced in a non-angle (length)
              // dimension - the numeric value would be misinterpreted.
              if (p.kind == "angle" && !dim_is_angle) {
                throw std::runtime_error(
                    "Angle parameter '" + name +
                    "' cannot be used in a length dimension");
              }
              return p.resolved_value;
            }
          }
          throw std::runtime_error("Unknown parameter: '" + name + "'");
        };

        try {
          resolved_value = core::evaluate_formula(expr_str, resolver);
          // Angle dimensions store radians internally, but expressions
          // authored by the user are in degrees (matching how plain
          // numeric angle edits are converted by the UI).  If this
          // dimension is an angle kind, convert degrees → radians so
          // the stored value matches what reify_dimension_expressions
          // produces.
          if (dim_is_angle) {
            resolved_value = resolved_value * (M_PI / 180.0);
          } else if (dim_is_circle_radius) {
            resolved_value = resolved_value / 2.0;
          }
        } catch (const std::exception& e) {
          throw std::runtime_error(
              std::string("Dimension expression error: ") + e.what());
        }
      } else {
        throw std::runtime_error(
            "Dimension 'value' must be a number or string expression");
      }
    } else {
      throw std::runtime_error("Dimension command missing 'value' field");
    }

    const auto document = document_manager().update_sketch_dimension(
        read_string(command.payload, "dimension_id"),
        resolved_value,
        expression);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_angle_dimension") {
    const auto document = document_manager().add_sketch_angle_dimension(
        read_string(command.payload, "first_line_id"),
        read_string(command.payload, "second_line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_distance_dimension") {
    const auto document = document_manager().add_sketch_distance_dimension(
        read_string(command.payload, "first_entity_id"),
        read_string(command.payload, "second_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_label_position") {
    const auto document =
        document_manager().update_sketch_dimension_label_position(
            read_string(command.payload, "dimension_id"),
            read_dimension(command.payload, "label_x"),
            read_dimension(command.payload, "label_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_line_length_dimension") {
    const auto document =
        document_manager().add_sketch_line_length_dimension(
            read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle_radius_dimension") {
    std::optional<std::string> display_as;
    if (command.payload.contains("display_as") &&
        command.payload.at("display_as").is_string()) {
      display_as = command.payload.at("display_as").get<std::string>();
    }
    const auto document =
        document_manager().add_sketch_circle_radius_dimension(
            read_string(command.payload, "circle_id"), display_as);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_point_distance_dimension") {
    const auto document =
        document_manager().add_sketch_point_distance_dimension(
            read_string(command.payload, "point_a_id"),
            read_string(command.payload, "point_b_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_polygon_radius_dimension") {
    const auto document =
        document_manager().add_sketch_polygon_radius_dimension(
            read_string(command.payload, "polygon_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_profile") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_sketch_profile(
        read_string(command.payload, "profile_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "extrude_profile") {
    // Optional `mode` payload field selects boolean composition behavior.
    // When absent, the core picks Join / Cut / New Body from geometry.
    // Optional `target_body_id` picks which existing body cut/join targets;
    // when absent the body compiler falls back to the most recent body.
    std::string mode;
    if (command.payload.contains("mode") &&
        command.payload.at("mode").is_string()) {
      mode = command.payload.at("mode").get<std::string>();
    }
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const bool has_open_entity_ids =
        command.payload.contains("open_entity_ids") &&
        command.payload.at("open_entity_ids").is_array();
    if (profile_ids.empty() && !has_open_entity_ids) {
      profile_ids.push_back(read_string(command.payload, "profile_id"));
    }
    const double depth = read_dimension(command.payload, "depth");
    const auto parameters =
        read_optional_extrude_parameters(command.payload,
                                         depth,
                                         mode,
                                         target_body_id);
    if (profile_ids.empty() && has_open_entity_ids) {
      std::vector<std::string> entity_ids;
      for (const auto& id : command.payload.at("open_entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
      const auto document = document_manager().extrude_open_entities(
          entity_ids, depth, mode, target_body_id, parameters);
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return;
    }
    const auto document = document_manager().extrude_profiles(
        profile_ids, depth, mode, target_body_id, parameters);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "extrude_face") {
    std::string mode;
    if (command.payload.contains("mode") &&
        command.payload.at("mode").is_string()) {
      mode = command.payload.at("mode").get<std::string>();
    }
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    const double depth = read_dimension(command.payload, "depth");
    const auto parameters =
        read_optional_extrude_parameters(command.payload,
                                         depth,
                                         mode,
                                         target_body_id);
    const auto document = document_manager().extrude_face(
        read_string(command.payload, "face_id"),
        depth,
        mode,
        target_body_id,
        parameters);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_mode") {
    const auto document = document_manager().update_extrude_mode(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "mode"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_target_body") {
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    const auto document = document_manager().update_extrude_target_body(
        read_string(command.payload, "feature_id"), target_body_id);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_parameters") {
    const auto parameters =
        command.payload.contains("parameters") &&
                command.payload.at("parameters").is_object()
            ? polysmith::protocol::extrude_parameters_from_payload(
                  command.payload.at("parameters"))
            : polysmith::protocol::extrude_parameters_from_payload(
                  command.payload);
    const auto document = document_manager().update_extrude_parameters(
        read_string(command.payload, "feature_id"), parameters);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_extrude_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_extrude_profiles(
        read_string(command.payload, "feature_id"), profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "loft_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    bool ruled = false;
    if (command.payload.contains("ruled") &&
        command.payload.at("ruled").is_boolean()) {
      ruled = command.payload.at("ruled").get<bool>();
    }
    const auto document = document_manager().loft_profiles(profile_ids, ruled);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_loft_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document = document_manager().update_loft_profiles(
        read_string(command.payload, "feature_id"), profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_loft_ruled") {
    if (!command.payload.contains("ruled") ||
        !command.payload.at("ruled").is_boolean()) {
      throw std::runtime_error("Command payload is missing bool field 'ruled'");
    }
    const auto document = document_manager().update_loft_ruled(
        read_string(command.payload, "feature_id"),
        command.payload.at("ruled").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "revolve_profile") {
    double angle_degrees = 360.0;
    if (command.payload.contains("angle_degrees") &&
        command.payload.at("angle_degrees").is_number()) {
      angle_degrees = command.payload.at("angle_degrees").get<double>();
    }
    const auto document = document_manager().revolve_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "axis_entity_id"),
        angle_degrees);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_profile") {
    const auto document = document_manager().update_revolve_profile(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_axis") {
    const auto document = document_manager().update_revolve_axis(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "axis_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_revolve_angle") {
    const auto document = document_manager().update_revolve_angle(
        read_string(command.payload, "feature_id"),
        read_dimension(command.payload, "angle_degrees"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "sweep_profile") {
    const auto document = document_manager().sweep_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "path_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sweep_profile") {
    const auto document = document_manager().update_sweep_profile(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sweep_path") {
    const auto document = document_manager().update_sweep_path(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "path_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_line") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_line(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_midpoint_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    const auto document = document_manager().set_sketch_midpoint_anchor(
        read_string(command.payload, "point_id"), host_line_id);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_point_line_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    double t = 0.5;
    if (command.payload.contains("t") &&
        command.payload.at("t").is_number()) {
      t = command.payload.at("t").get<double>();
    }
    const auto document = document_manager().set_sketch_point_line_anchor(
        read_string(command.payload, "point_id"), host_line_id, t);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_line_construction") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().set_sketch_line_construction(
        read_string(command.payload, "line_id"), is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_rectangle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_rectangle(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_arc") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_arc(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        read_dimension(command.payload, "anchor_x"),
        read_dimension(command.payload, "anchor_y"),
        read_string(command.payload, "mode"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_circle(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_polygon") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = document_manager().add_sketch_polygon(
        static_cast<int>(read_dimension(command.payload, "sides")),
        read_string(command.payload, "mode"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        is_construction);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_fillet") {
    const auto document = document_manager().add_sketch_fillet(
        read_string(command.payload, "corner_point_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_fillet_radius") {
    const auto document = document_manager().update_sketch_fillet_radius(
        read_string(command.payload, "fillet_id"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_fillet") {
    const auto document = document_manager().delete_sketch_fillet(
        read_string(command.payload, "fillet_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_dimension") {
    const auto document = document_manager().delete_sketch_dimension(
        read_string(command.payload, "dimension_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_display") {
    const auto document = document_manager().update_sketch_dimension_display(
        read_string(command.payload, "dimension_id"),
        read_string(command.payload, "display_as"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_selection") {
    std::vector<std::string> entity_ids;
    std::vector<std::string> point_ids;
    std::vector<std::string> profile_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& id : command.payload.at("entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("point_ids") &&
        command.payload.at("point_ids").is_array()) {
      for (const auto& id : command.payload.at("point_ids")) {
        if (id.is_string()) {
          point_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().delete_sketch_selection(
            entity_ids, point_ids, profile_ids);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_point") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_sketch_point(
        read_string(command.payload, "point_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_point_fixed") {
    const auto document = document_manager().set_sketch_point_fixed(
        read_string(command.payload, "point_id"),
        command.payload.at("is_fixed").get<bool>());

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_entity") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = document_manager().select_sketch_entity(
        read_string(command.payload, "entity_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_dimension") {
    const auto document = document_manager().select_sketch_dimension(
        read_string(command.payload, "dimension_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "finish_sketch") {
    const auto document = document_manager().finish_sketch();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "reenter_sketch") {
    const auto document = document_manager().reenter_sketch(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_face_into_sketch") {
    const auto document = document_manager().project_face_into_sketch(
        read_string(command.payload, "face_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_profile_into_sketch") {
    const auto document = document_manager().project_profile_into_sketch(
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_edge_into_sketch") {
    const auto document = document_manager().project_edge_into_sketch(
        read_string(command.payload, "edge_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "project_vertex_into_sketch") {
    const auto document = document_manager().project_vertex_into_sketch(
        read_string(command.payload, "vertex_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_selection") {
    const auto document = document_manager().clear_selection();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = document_manager().add_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"),
        kind);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = document_manager().update_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"),
        kind);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_selection_filter") {
    auto filter = polysmith::core::SelectionFilter{};
    // Read booleans from payload with defaults from the struct
    if (command.payload.contains("select_curves") &&
        command.payload.at("select_curves").is_boolean()) {
      filter.select_curves = command.payload.at("select_curves").get<bool>();
    }
    if (command.payload.contains("select_points") &&
        command.payload.at("select_points").is_boolean()) {
      filter.select_points = command.payload.at("select_points").get<bool>();
    }
    if (command.payload.contains("select_construction") &&
        command.payload.at("select_construction").is_boolean()) {
      filter.select_construction = command.payload.at("select_construction").get<bool>();
    }
    if (command.payload.contains("select_constraints") &&
        command.payload.at("select_constraints").is_boolean()) {
      filter.select_constraints = command.payload.at("select_constraints").get<bool>();
    }
    if (command.payload.contains("snap_endpoint") &&
        command.payload.at("snap_endpoint").is_boolean()) {
      filter.snap_endpoint = command.payload.at("snap_endpoint").get<bool>();
    }
    if (command.payload.contains("snap_midpoint") &&
        command.payload.at("snap_midpoint").is_boolean()) {
      filter.snap_midpoint = command.payload.at("snap_midpoint").get<bool>();
    }
    if (command.payload.contains("snap_center") &&
        command.payload.at("snap_center").is_boolean()) {
      filter.snap_center = command.payload.at("snap_center").get<bool>();
    }
    if (command.payload.contains("snap_intersection") &&
        command.payload.at("snap_intersection").is_boolean()) {
      filter.snap_intersection = command.payload.at("snap_intersection").get<bool>();
    }
    if (command.payload.contains("snap_nearest") &&
        command.payload.at("snap_nearest").is_boolean()) {
      filter.snap_nearest = command.payload.at("snap_nearest").get<bool>();
    }
    if (command.payload.contains("snap_quadrant") &&
        command.payload.at("snap_quadrant").is_boolean()) {
      filter.snap_quadrant = command.payload.at("snap_quadrant").get<bool>();
    }
    if (command.payload.contains("snap_perpendicular") &&
        command.payload.at("snap_perpendicular").is_boolean()) {
      filter.snap_perpendicular = command.payload.at("snap_perpendicular").get<bool>();
    }
    if (command.payload.contains("snap_parallel") &&
        command.payload.at("snap_parallel").is_boolean()) {
      filter.snap_parallel = command.payload.at("snap_parallel").get<bool>();
    }
    if (command.payload.contains("snap_tangent") &&
        command.payload.at("snap_tangent").is_boolean()) {
      filter.snap_tangent = command.payload.at("snap_tangent").get<bool>();
    }
    if (command.payload.contains("snap_grid") &&
        command.payload.at("snap_grid").is_boolean()) {
      filter.snap_grid = command.payload.at("snap_grid").get<bool>();
    }
    if (command.payload.contains("snap_grid_line") &&
        command.payload.at("snap_grid_line").is_boolean()) {
      filter.snap_grid_line = command.payload.at("snap_grid_line").get<bool>();
    }
    if (command.payload.contains("snap_polar") &&
        command.payload.at("snap_polar").is_boolean()) {
      filter.snap_polar = command.payload.at("snap_polar").get<bool>();
    }
    if (command.payload.contains("magnetic_pull") &&
        command.payload.at("magnetic_pull").is_boolean()) {
      filter.magnetic_pull = command.payload.at("magnetic_pull").get<bool>();
    }
    if (command.payload.contains("tolerance_px") &&
        command.payload.at("tolerance_px").is_number()) {
      filter.tolerance_px = command.payload.at("tolerance_px").get<int>();
    }

    const auto document = document_manager().update_selection_filter(filter);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_parameter") {
    const auto document = document_manager().delete_parameter(
        read_string(command.payload, "name"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "trim_sketch_entity") {
    const auto document = document_manager().trim_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "trim_preview") {
    const std::string entity_id = read_string(command.payload, "entity_id");
    const double cursor_x = read_dimension(command.payload, "cursor_x");
    const double cursor_y = read_dimension(command.payload, "cursor_y");

    const auto doc_opt = document_manager().get_document();
    if (!doc_opt.has_value() ||
        !doc_opt->active_sketch_feature_id.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto feature_it = std::find_if(
        doc_opt->feature_history.begin(),
        doc_opt->feature_history.end(),
        [&](const auto& f) {
          return f.id == doc_opt->active_sketch_feature_id.value();
        });

    if (feature_it == doc_opt->feature_history.end() ||
        !feature_it->sketch_parameters.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto& params = feature_it->sketch_parameters.value();
    nlohmann::json result;
    result["entity_id"] = entity_id;

    // Find and preview the entity.
    const auto line_it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == entity_id; });
    if (line_it != params.lines.end()) {
      auto isects = polysmith::core::find_all_intersections(*line_it, params);
      if (isects.empty()) {
        polysmith::protocol::write_message(
            { { "id", command.id },
              { "type", "trim_preview_result" },
              { "payload", nullptr } });
        return;
      }
      auto segs = polysmith::core::split_line_at_intersections(*line_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *line_it, cursor_x, cursor_y);
      result["entity_kind"] = "line";
      result["hovered_index"] = idx;
      auto& jsegs = result["segments"] = nlohmann::json::array();
      for (const auto& s : segs) {
        nlohmann::json seg;
        seg["start"] = {s.start_x, s.start_y};
        seg["end"]   = {s.end_x,   s.end_y};
        jsegs.push_back(std::move(seg));
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", result } });
      return;
    }

    const auto circle_it = std::find_if(
        params.circles.begin(), params.circles.end(),
        [&](const auto& c) { return c.id == entity_id; });
    if (circle_it != params.circles.end()) {
      auto isects = polysmith::core::find_all_intersections(*circle_it, params);
      auto r = nlohmann::json::object();
      r["entity_id"] = entity_id;
      r["entity_kind"] = "circle";
      if (isects.empty()) {
        r["hovered_index"] = 0;
        r["full_circle"] = true;
      } else {
        auto segs = polysmith::core::split_circle_at_intersections(
            *circle_it, isects);
        int idx = polysmith::core::select_clicked_segment(
            segs, *circle_it, cursor_x, cursor_y);
        r["hovered_index"] = idx;
        r["full_circle"] = false;
        // Return segment angles so the UI can sample.
        auto& jsegs = r["segments"] = nlohmann::json::array();
        for (const auto& s : segs) {
          nlohmann::json seg;
          seg["param_start"] = s.param_start;
          seg["param_end"]   = s.param_end;
          jsegs.push_back(std::move(seg));
        }
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", r } });
      return;
    }

    const auto arc_it = std::find_if(
        params.arcs.begin(), params.arcs.end(),
        [&](const auto& a) { return a.id == entity_id; });
    if (arc_it != params.arcs.end()) {
      auto isects = polysmith::core::find_all_intersections(*arc_it, params);
      auto r = nlohmann::json::object();
      r["entity_id"] = entity_id;
      r["entity_kind"] = "arc";
      if (isects.empty()) {
        r["hovered_index"] = 0;
        r["full_arc"] = true;
      } else {
        auto segs = polysmith::core::split_arc_at_intersections(
            *arc_it, isects);
        int idx = polysmith::core::select_clicked_segment(
            segs, *arc_it, cursor_x, cursor_y);
        r["hovered_index"] = idx;
        r["full_arc"] = false;
        auto& jsegs = r["segments"] = nlohmann::json::array();
        for (const auto& s : segs) {
          nlohmann::json seg;
          seg["param_start"] = s.param_start;
          seg["param_end"]   = s.param_end;
          jsegs.push_back(std::move(seg));
        }
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", r } });
      return;
    }

    polysmith::protocol::write_message(
        { { "id", command.id },
          { "type", "trim_preview_result" },
          { "payload", nullptr } });
    return;
  }

  if (command.type == "resolve_draft_snap") {
    const double cursor_x = read_dimension(command.payload, "cursor_x");
    const double cursor_y = read_dimension(command.payload, "cursor_y");
    const double start_x = read_dimension(command.payload, "start_x");
    const double start_y = read_dimension(command.payload, "start_y");

    const auto doc_opt = document_manager().get_document();
    if (!doc_opt.has_value() ||
        !doc_opt->active_sketch_feature_id.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "draft_snap_resolved" },
            { "payload", nullptr } });
      return;
    }

    const auto feature_it = std::find_if(
        doc_opt->feature_history.begin(),
        doc_opt->feature_history.end(),
        [&](const auto& f) {
          return f.id == doc_opt->active_sketch_feature_id.value();
        });

    if (feature_it == doc_opt->feature_history.end() ||
        !feature_it->sketch_parameters.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "draft_snap_resolved" },
            { "payload", nullptr } });
      return;
    }

    const double tolerance = 0.5;
    const auto snap = polysmith::core::resolve_snap(
        cursor_x, cursor_y,
        feature_it->sketch_parameters.value(),
        doc_opt->selection_filter,
        tolerance,
        start_x, start_y);

    if (snap.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "draft_snap_resolved" },
            { "payload",
              { { "snap_x", snap->local_x },
                { "snap_y", snap->local_y },
                { "snap_kind", snap->kind },
                { "snap_label", snap->label },
                { "host_entity_id", snap->entity_id },
                { "host_point_id", snap->point_id },
                { "host_param_t", snap->param_t } } } });
    } else {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "draft_snap_resolved" },
            { "payload", nullptr } });
    }
    return;
  }

  if (command.type == "shutdown") {
    throw std::runtime_error("__POLYSMITH_SHUTDOWN__");
  }

  polysmith::core::log_error("cad_core", "Unknown command: " + command.type);
  polysmith::protocol::write_message(polysmith::protocol::make_error_event(
      command.id, "UNKNOWN_COMMAND", "Unknown command: " + command.type));
}

void CadCoreApp::run() {
  init_occt();
  polysmith::protocol::write_message(polysmith::protocol::make_hello_event());

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }

    try {
      handle_command_line(line);
    } catch (const std::runtime_error& error) {
      if (std::string(error.what()) == "__POLYSMITH_SHUTDOWN__") {
        polysmith::core::log_info("cad_core", "Shutdown requested");
        break;
      }

      polysmith::core::log_error("cad_core", error.what());
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_COMMAND", error.what()));
    } catch (const std::exception& error) {
      polysmith::core::log_error("cad_core", error.what());
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_JSON", error.what()));
    }
  }
}

}  // namespace polysmith
