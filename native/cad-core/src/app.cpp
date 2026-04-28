#include "app.h"

#include <exception>
#include <string>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include "core/document.h"
#include "core/viewport.h"
#include "protocol/ipc.h"
#include "protocol/serialization.h"

namespace polysmith {
namespace {

using polysmith::core::DocumentManager;
using polysmith::core::BoxFeatureParameters;
using polysmith::core::CylinderFeatureParameters;
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

std::string read_string(const polysmith::protocol::json& payload,
                        const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload is missing string field '") +
                             key + "'");
  }

  return payload.at(key).get<std::string>();
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
  polysmith::protocol::write_log("Starting OCCT smoke test...");

  const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

  if (box.IsNull()) {
    polysmith::protocol::write_log("OCCT smoke test failed: shape is null");
    return;
  }

  polysmith::protocol::write_log("OCCT box created successfully");
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
    const auto document = document_manager().update_sketch_dimension(
        read_string(command.payload, "dimension_id"),
        read_dimension(command.payload, "value"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_profile") {
    const auto document = document_manager().select_sketch_profile(
        read_string(command.payload, "profile_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "extrude_profile") {
    const auto document = document_manager().extrude_profile(
        read_string(command.payload, "profile_id"),
        read_dimension(command.payload, "depth"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_line") {
    const auto document = document_manager().add_sketch_line(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_rectangle") {
    const auto document = document_manager().add_sketch_rectangle(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle") {
    const auto document = document_manager().add_sketch_circle(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_sketch_point") {
    const auto document = document_manager().select_sketch_point(
        read_string(command.payload, "point_id"));

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
    const auto document = document_manager().select_sketch_entity(
        read_string(command.payload, "entity_id"));

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

  if (command.type == "clear_selection") {
    const auto document = document_manager().clear_selection();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "shutdown") {
    throw std::runtime_error("__POLYSMITH_SHUTDOWN__");
  }

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
        break;
      }

      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_COMMAND", error.what()));
    } catch (const std::exception& error) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_JSON", error.what()));
    }
  }
}

}  // namespace polysmith
