#include "app/cam_command_handlers.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/command_context.h"
#include "core/cam/cam_generate.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/cam/cam_resolution.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/cam_types.h"
#include "core/cam/machine_library.h"
#include "core/cam/post_processor.h"
#include "core/cam/tool_library.h"
#include "core/cam/tool_table_io.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith {

namespace {

using polysmith::protocol::json;

void parse_tool_content(const std::string& format, const std::string& text,
                        std::vector<polysmith::core::ToolEntry>& tools,
                        std::vector<std::string>& warnings,
                        std::string& error) {
  if (format == "linuxcnc_tbl") {
    polysmith::core::parse_linuxcnc_tool_table(text, tools, warnings, error);
  } else if (format == "polysmith_json") {
    tools = polysmith::core::parse_polysmith_tools_json(text, warnings, error);
  } else {
    error = "unknown tool format: " + format;
  }
}

std::string tool_format_for_path(const std::string& path) {
  if (path.size() >= 4 && path.substr(path.size() - 4) == ".tbl") {
    return std::string("linuxcnc_tbl");
  }
  if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") {
    return std::string("polysmith_json");
  }
  return std::string();
}

void send_tool_parse_result(
    const std::string& command_id, const std::string& format,
    const std::vector<polysmith::core::ToolEntry>& tools,
    const std::vector<std::string>& warnings) {
  json payload_tools = json::array();
  for (const auto& tool : tools) {
    payload_tools.push_back(polysmith::protocol::to_payload(tool));
  }
  json payload_warnings = json::array();
  for (const auto& warning : warnings) {
    payload_warnings.push_back(warning);
  }
  polysmith::protocol::write_message(
      polysmith::protocol::make_cam_tool_parse_result_event(
          command_id,
          json{{"format", format},
               {"tools", payload_tools},
               {"warnings", payload_warnings}}));
}

std::string read_tool_file(const std::string& path) {
  std::ifstream stream(path);
  if (!stream) {
    return std::string();
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

}  // namespace

bool handle_cam_command(const polysmith::protocol::CommandMessage& command,
                        polysmith::core::DocumentManager& manager) {
  if (command.type == "cam_setup_create") {
    const auto setup = polysmith::protocol::cam_setup_from_payload(command.payload);
    const auto document = manager.cam_setup_create(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_setup_get") {
    const auto setup = manager.cam_setup_get();
    if (!setup.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_cam_setup_event(
        command.id, polysmith::protocol::to_payload(setup.value())));
    return true;
  }

  if (command.type == "cam_setup_update") {
    const auto setup = polysmith::protocol::cam_setup_from_payload(command.payload);
    const auto document = manager.cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_setup_delete") {
    const auto document =
        manager.cam_setup_delete(read_string(command.payload, "setup_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_machine_settings_set") {
    const auto settings =
        polysmith::protocol::laser_machine_settings_from_payload(command.payload);
    const auto document = manager.cam_machine_settings_set(settings);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_capture_face_reference") {
    const std::string face_id = command.payload.value("face_id", "");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const auto separator = face_id.rfind(":face:");
    if (separator == std::string::npos) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return true;
    }
    const std::string body_id = face_id.substr(0, separator);
    int face_index = 0;
    try {
      face_index = std::stoi(face_id.substr(separator + 6));
    } catch (const std::exception&) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return true;
    }
    const auto bodies =
        polysmith::core::compile_bodies(document.value(), /*include_meshes=*/false);
    const polysmith::core::CompiledBody* body = nullptr;
    for (const auto& candidate : bodies.bodies) {
      if (candidate.id == body_id) {
        body = &candidate;
        break;
      }
    }
    if (body == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_NOT_FOUND",
          "The body of the selected face no longer exists."));
      return true;
    }
    const auto captured =
        polysmith::core::capture_face_reference(body_id, body->shape, face_index);
    if (!captured.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_CAPTURE_FAILED",
          "The face witness could not be captured."));
      return true;
    }
    polysmith::core::FaceAttestation attestation;
    attestation.area = captured->capturedArea;
    attestation.normal = captured->capturedNormal;
    attestation.sample_points = captured->samplePoints;
    for (const auto& point : captured->samplePoints) {
      attestation.bounds.min_x = std::min(attestation.bounds.min_x, point[0]);
      attestation.bounds.min_y = std::min(attestation.bounds.min_y, point[1]);
      attestation.bounds.min_z = std::min(attestation.bounds.min_z, point[2]);
      attestation.bounds.max_x = std::max(attestation.bounds.max_x, point[0]);
      attestation.bounds.max_y = std::max(attestation.bounds.max_y, point[1]);
      attestation.bounds.max_z = std::max(attestation.bounds.max_z, point[2]);
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_face_attestation_event(
            command.id,
            {{"persistent_id", face_id},
             {"attestation", polysmith::protocol::to_payload(attestation)}}));
    return true;
  }

  if (command.type == "cam_capture_edge_reference") {
    const std::string edge_id = command.payload.value("edge_id", "");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const auto separator = edge_id.rfind(":edge:");
    if (separator == std::string::npos) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_EDGE_ID", "The edge id is malformed: " + edge_id));
      return true;
    }
    const std::string body_id = edge_id.substr(0, separator);
    int edge_index = 0;
    try {
      edge_index = std::stoi(edge_id.substr(separator + 6));
    } catch (const std::exception&) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_EDGE_ID", "The edge id is malformed: " + edge_id));
      return true;
    }
    const auto bodies =
        polysmith::core::compile_bodies(document.value(), /*include_meshes=*/false);
    const polysmith::core::CompiledBody* body = nullptr;
    for (const auto& candidate : bodies.bodies) {
      if (candidate.id == body_id) {
        body = &candidate;
        break;
      }
    }
    if (body == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "EDGE_NOT_FOUND",
          "The body of the selected edge no longer exists."));
      return true;
    }
    const auto captured =
        polysmith::core::capture_edge_reference(body_id, body->shape, edge_index);
    if (!captured.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "EDGE_CAPTURE_FAILED",
          "The edge witness could not be captured (partial arcs and "
          "unsupported curves are not drilling inputs)."));
      return true;
    }
    polysmith::core::EdgeAttestation attestation;
    attestation.start_point = captured->startPoint;
    attestation.end_point = captured->endPoint;
    attestation.length = captured->length;
    attestation.tangent = captured->tangent;
    attestation.center = captured->center;
    attestation.axis = captured->axis;
    attestation.radius = captured->radius;
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_edge_attestation_event(
            command.id,
            {{"persistent_id", edge_id},
             {"attestation", polysmith::protocol::to_payload(attestation)}}));
    return true;
  }

  if (command.type == "cam_capture_point") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const auto point = std::array<double, 3>{
        command.payload.value("x", 0.0),
        command.payload.value("y", 0.0),
        command.payload.value("z", 0.0),
    };
    const auto ref = manager.cam_capture_point(point);
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_attestation_event(
            command.id, polysmith::protocol::to_payload(ref)));
    return true;
  }

  if (command.type == "cam_wcs_set_face") {
    const std::string face_id = command.payload.value("face_id", "");
    const std::string setup_id = command.payload.value("setup_id", "");
    const auto document = manager.get_document();
    if (!document.has_value() || document->cam.setups.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_CAM_SETUP", "Create a CAM setup first"));
      return true;
    }
    const auto resolved_setup = manager.cam_setup_find(setup_id);
    if (!resolved_setup.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "SETUP_NOT_FOUND", "CAM setup not found"));
      return true;
    }
    if (face_id.rfind("stock:", 0) == 0) {
      const std::string stock_face = face_id.substr(6);
      static const char* const kStockFaces[] = {
          "top", "bottom", "front", "back", "left", "right"};
      bool known = false;
      for (const char* face_name : kStockFaces) {
        if (stock_face == face_name) {
          known = true;
          break;
        }
      }
      if (!known) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "BAD_FACE_ID",
            "The stock face is unknown: " + face_id));
        return true;
      }
      auto setup = resolved_setup.value();
      setup.wcs_origin.anchor = "stock_face";
      setup.wcs_origin.stock_face = stock_face;
      setup.wcs_origin.feature_id.clear();
      setup.wcs_origin.face_reference = polysmith::core::GeometryReference{};
      setup.wcs_origin.position.reset();
      const auto updated = manager.cam_setup_update(setup);
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(updated)));
      return true;
    }
    const auto separator = face_id.rfind(":face:");
    if (separator == std::string::npos) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return true;
    }
    const std::string body_id = face_id.substr(0, separator);
    int face_index = 0;
    try {
      face_index = std::stoi(face_id.substr(separator + 6));
    } catch (const std::exception&) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "BAD_FACE_ID", "The face id is malformed: " + face_id));
      return true;
    }
    const auto bodies =
        polysmith::core::compile_bodies(document.value(), /*include_meshes=*/false);
    const polysmith::core::CompiledBody* body = nullptr;
    for (const auto& candidate : bodies.bodies) {
      if (candidate.id == body_id) {
        body = &candidate;
        break;
      }
    }
    if (body == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_NOT_FOUND",
          "The body of the selected face no longer exists."));
      return true;
    }
    const auto captured =
        polysmith::core::capture_face_reference(body_id, body->shape, face_index);
    if (!captured.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "FACE_CAPTURE_FAILED",
          "The WCS face witness could not be captured."));
      return true;
    }
    polysmith::core::FaceAttestation attestation;
    attestation.area = captured->capturedArea;
    attestation.normal = captured->capturedNormal;
    attestation.sample_points = captured->samplePoints;
    auto setup = resolved_setup.value();
    setup.wcs_origin.anchor = "face";
    setup.wcs_origin.feature_id = body_id;
    setup.wcs_origin.face_reference.persistent_id = face_id;
    setup.wcs_origin.face_reference.attestation = attestation;
    const auto updated = manager.cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return true;
  }

  if (command.type == "cam_stock_set") {
    const auto document = manager.get_document();
    if (!document.has_value() || document->cam.setups.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_CAM_SETUP", "Create a CAM setup first"));
      return true;
    }
    auto setup = document->cam.setups[0];
    setup.stock = polysmith::protocol::stock_definition_from_payload(command.payload);
    const auto updated = manager.cam_setup_update(setup);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return true;
  }

  if (command.type == "cam_stock_get") {
    const auto setup = manager.cam_setup_get();
    if (!setup.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_cam_stock_event(
        command.id, polysmith::protocol::to_payload(setup->stock)));
    return true;
  }

  if (command.type == "cam_tool_add") {
    const auto tool = polysmith::protocol::tool_entry_from_payload(command.payload);
    const auto document = manager.cam_tool_add(tool);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_tool_update") {
    const auto tool = polysmith::protocol::tool_entry_from_payload(command.payload);
    const auto document = manager.cam_tool_update(
        read_string(command.payload, "tool_id"), tool);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_tool_delete") {
    const auto document =
        manager.cam_tool_delete(read_string(command.payload, "tool_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_tool_list") {
    const auto tools = manager.cam_tool_list();
    json payload = json::array();
    for (const auto& tool : tools) {
      payload.push_back(polysmith::protocol::to_payload(tool));
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_tool_list_event(command.id, payload));
    return true;
  }

  if (command.type == "cam_operation_create") {
    auto op = polysmith::protocol::cam_operation_from_payload(command.payload);
    if (op.geometry_references.machining_regions.empty() &&
        op.type != "drilling") {
      const auto document = manager.get_document();
      if (!document.has_value()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
        return true;
      }
      if (!polysmith::core::capture_profile_references_from_selection(
              document.value(), op)) {
        if (op.type != "laser_cut" && op.type != "contour_2d" &&
            op.type != "engrave") {
          polysmith::protocol::write_message(polysmith::protocol::make_error_event(
              command.id, "NO_PROFILE_SELECTION",
              "Select sketch profiles for the CAM operation first"));
          return true;
        }
      } else {
        op.geometry_scope = document->selected_sketch_profile_ids.empty()
                                ? "sketch"
                                : "selected";
      }
    }
    const auto document = manager.cam_operation_add(op);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_operation_update") {
    const std::string op_id = read_string(command.payload, "op_id");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const polysmith::core::CamOperation* existing = nullptr;
    for (const auto& op : document->cam.operations) {
      if (op.op_id == op_id) {
        existing = &op;
        break;
      }
    }
    if (existing == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_OPERATION_NOT_FOUND",
          "CAM operation not found: " + op_id));
      return true;
    }
    auto merged = *existing;
    const auto& p = command.payload;
    if (p.contains("name") && !p.at("name").is_null()) {
      merged.name = p.at("name").get<std::string>();
    }
    if (p.contains("type") && !p.at("type").is_null()) {
      merged.type = p.at("type").get<std::string>();
    }
    if (p.contains("enabled") && p.at("enabled").is_boolean()) {
      merged.enabled = p.at("enabled").get<bool>();
    }
    if (p.contains("tool_id") && !p.at("tool_id").is_null()) {
      merged.tool_id = p.at("tool_id").get<std::string>();
    }
    if (p.contains("geometry_references")) {
      merged.geometry_references =
          polysmith::protocol::cam_geometry_references_from_payload(
              p.at("geometry_references"));
      if (merged.geometry_references.machining_regions.empty() &&
          (merged.type == "laser_cut" || merged.type == "contour_2d" ||
           merged.type == "engrave")) {
        std::string layer_feature_id;
        if (p.contains("layer_feature_id") && p.at("layer_feature_id").is_string()) {
          layer_feature_id = p.at("layer_feature_id").get<std::string>();
        }
        if (!layer_feature_id.empty()) {
          const auto attestation =
              polysmith::core::pcb_layer_attestation_from_feature(
                  document.value(), layer_feature_id);
          if (!attestation.has_value()) {
            polysmith::protocol::write_message(polysmith::protocol::make_error_event(
                command.id, "PCB_LAYER_NOT_FOUND",
                "No PCB layer matches " + layer_feature_id + " in this document."));
            return true;
          }
          polysmith::core::GeometryReference reference;
          reference.persistent_id = layer_feature_id;
          reference.attestation = attestation.value();
          merged.geometry_references.machining_regions = {reference};
          merged.geometry_scope = "selected";
        } else {
          std::vector<std::string> profile_ids;
          if (p.contains("selected_profile_ids") &&
              p.at("selected_profile_ids").is_array()) {
            for (const auto& value : p.at("selected_profile_ids")) {
              if (value.is_string()) {
                profile_ids.push_back(value.get<std::string>());
              }
            }
          } else {
            profile_ids = document->selected_sketch_profile_ids;
          }
          if (profile_ids.empty() ||
              !polysmith::core::capture_profile_references_from_profile_ids(
                  document.value(), profile_ids, merged)) {
            polysmith::protocol::write_message(polysmith::protocol::make_error_event(
                command.id, "NO_PROFILE_SELECTION",
                "Select sketch profiles for the CAM operation first"));
            return true;
          }
          merged.geometry_scope = "selected";
        }
      }
    }
    if (p.contains("parameters")) {
      merged.parameters = polysmith::protocol::cam_operation_parameters_from_payload(
          p.at("parameters"));
    }
    if (p.contains("dependencies")) {
      merged.dependencies =
          polysmith::protocol::cam_operation_dependencies_from_payload(
              p.at("dependencies"));
    }
    const auto updated = manager.cam_operation_update(op_id, merged);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated)));
    return true;
  }

  if (command.type == "cam_operation_delete") {
    const auto document =
        manager.cam_operation_delete(read_string(command.payload, "op_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_operation_set_scope") {
    const auto document = manager.cam_operation_set_scope(
        read_string(command.payload, "op_id"),
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_post_processor_set") {
    const auto post = polysmith::protocol::post_processor_from_payload(command.payload);
    const auto document = manager.cam_post_processor_set(post);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_post_list" || command.type == "cam_post_import") {
    std::string error;
    if (command.type == "cam_post_import") {
      const std::string imported = polysmith::core::import_post_processor(
          read_string(command.payload, "source_path"), error);
      if (!error.empty()) {
        polysmith::core::log_warn("cam", "post import failed: " + error);
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_POST_IMPORT_FAILED", error));
        return true;
      }
      polysmith::core::log_info("cam", "post processor imported: " + imported);
    }
    const auto posts = polysmith::core::list_post_processors();
    json payload = json::array();
    for (const auto& entry : posts) {
      payload.push_back({{"name", entry.name}, {"path", entry.path}});
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_post_list_event(command.id, payload));
    return true;
  }

  if (command.type == "cam_machine_list" || command.type == "cam_machine_save") {
    if (command.type == "cam_machine_save") {
      const auto machine =
          polysmith::protocol::machine_definition_from_payload(command.payload);
      std::string error;
      const std::string saved = polysmith::core::save_machine_definition(machine, error);
      if (!error.empty()) {
        polysmith::core::log_warn("cam", "machine save failed: " + error);
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_MACHINE_SAVE_FAILED", error));
        return true;
      }
      polysmith::core::log_info("cam", "machine definition saved: " + saved);
    }
    const auto machines = polysmith::core::load_machine_library();
    json payload = json::array();
    for (const auto& machine : machines) {
      payload.push_back(polysmith::protocol::to_payload(machine));
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_machine_list_event(command.id, payload));
    return true;
  }

  if (command.type == "cam_tool_parse_text" ||
      command.type == "cam_tool_parse_file") {
    const std::string format = read_string(command.payload, "format");
    const std::string text = read_string(command.payload, "text");
    if (command.type == "cam_tool_parse_file") {
      const std::string path = read_string(command.payload, "source_path");
      if (path.empty()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_TOOL_PARSE_FAILED", "No source path given"));
        return true;
      }
      const std::string detected = tool_format_for_path(path);
      if (detected.empty()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_TOOL_PARSE_FAILED",
            "Unsupported tool file extension (expected .tbl or .json): " + path));
        return true;
      }
      const std::string content = read_tool_file(path);
      if (content.empty()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_TOOL_PARSE_FAILED",
            "Could not read the tool file: " + path));
        return true;
      }
      std::vector<polysmith::core::ToolEntry> tools;
      std::vector<std::string> warnings;
      std::string error;
      parse_tool_content(detected, content, tools, warnings, error);
      if (!error.empty()) {
        polysmith::core::log_warn("cam", "tool parse failed: " + error);
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_TOOL_PARSE_FAILED", error));
        return true;
      }
      send_tool_parse_result(command.id, detected, tools, warnings);
      return true;
    }
    std::vector<polysmith::core::ToolEntry> tools;
    std::vector<std::string> warnings;
    std::string error;
    parse_tool_content(format, text, tools, warnings, error);
    if (!error.empty()) {
      polysmith::core::log_warn("cam", "tool parse failed: " + error);
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_PARSE_FAILED", error));
      return true;
    }
    send_tool_parse_result(command.id, format, tools, warnings);
    return true;
  }

  if (command.type == "cam_tool_export_text") {
    const std::string format = read_string(command.payload, "format");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const json requested = command.payload.contains("tool_numbers")
                               ? command.payload.at("tool_numbers")
                               : json(nullptr);
    std::vector<polysmith::core::ToolEntry> tools;
    for (const auto& tool : document->cam.tool_library) {
      if (requested.is_array() && !requested.empty()) {
        bool wanted = false;
        for (const auto& number : requested) {
          if (number.is_number_integer() &&
              number.get<int>() == tool.tool_number) {
            wanted = true;
            break;
          }
        }
        if (!wanted) {
          continue;
        }
      }
      tools.push_back(tool);
    }
    std::string text;
    if (format == "linuxcnc_tbl") {
      text = polysmith::core::serialize_linuxcnc_tool_table(tools);
    } else if (format == "polysmith_json") {
      text = polysmith::core::serialize_polysmith_tools_json(tools);
    } else {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_EXPORT_FAILED",
          "unknown tool format: " + format));
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_tool_export_text_result_event(
            command.id, json{{"format", format}, {"text", text}}));
    return true;
  }

  if (command.type == "cam_tool_import_file") {
    const std::string path = read_string(command.payload, "source_path");
    const std::string mode = command.payload.contains("mode")
                                 ? read_string(command.payload, "mode")
                                 : std::string("renumber");
    const std::string format = tool_format_for_path(path);
    if (format.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_PARSE_FAILED",
          "Unsupported tool file extension (expected .tbl or .json): " + path));
      return true;
    }
    const std::string content = read_tool_file(path);
    if (content.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_PARSE_FAILED",
          "Could not read the tool file: " + path));
      return true;
    }
    std::vector<polysmith::core::ToolEntry> tools;
    std::vector<std::string> warnings;
    std::string error;
    parse_tool_content(format, content, tools, warnings, error);
    if (!error.empty()) {
      polysmith::core::log_warn("cam", "tool import parse failed: " + error);
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_PARSE_FAILED", error));
      return true;
    }
    for (const auto& warning : warnings) {
      polysmith::core::log_warn("cam", "tool import: " + warning);
    }
    const auto document = manager.cam_tool_import_tools(tools, mode);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cam_tool_export_file") {
    const std::string path = read_string(command.payload, "file_path");
    const std::string format = tool_format_for_path(path);
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    std::string text;
    if (format == "linuxcnc_tbl") {
      text = polysmith::core::serialize_linuxcnc_tool_table(
          document->cam.tool_library);
    } else if (format == "polysmith_json") {
      text = polysmith::core::serialize_polysmith_tools_json(
          document->cam.tool_library);
    } else {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_EXPORT_FAILED",
          "Unsupported tool file extension (expected .tbl or .json): " + path));
      return true;
    }
    std::ofstream stream(path);
    if (!stream) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_TOOL_EXPORT_FAILED",
          "Could not write the tool file: " + path));
      return true;
    }
    stream << text;
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_tool_export_file_result_event(
            command.id,
            json{{"file_path", path},
                 {"exported_count",
                  static_cast<int>(document->cam.tool_library.size())}}));
    return true;
  }

  if (command.type == "cam_tool_library_list" ||
      command.type == "cam_tool_library_save") {
    if (command.type == "cam_tool_library_save") {
      const auto tool = polysmith::protocol::tool_entry_from_payload(command.payload);
      std::string error;
      const std::string saved = polysmith::core::save_tool_entry(tool, error);
      if (!error.empty()) {
        polysmith::core::log_warn("cam", "tool save failed: " + error);
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "CAM_TOOL_SAVE_FAILED", error));
        return true;
      }
      polysmith::core::log_info("cam", "tool saved: " + saved);
    }
    const auto tools = polysmith::core::load_tool_library();
    json payload = json::array();
    for (const auto& tool : tools) {
      payload.push_back(polysmith::protocol::to_payload(tool));
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_tool_list_event(command.id, payload));
    return true;
  }

  if (command.type == "cam_export_gcode") {
    const auto export_result = manager.export_cam_gcode(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id,
            json{{"file_path", export_result.file_path},
                 {"format", export_result.format},
                 {"exported_feature_count", export_result.exported_feature_count}}));
    return true;
  }

  if (command.type == "cam_export_gcode_text") {
    const auto text_result = manager.export_cam_gcode_text();
    polysmith::protocol::write_message(
        polysmith::protocol::make_cam_export_gcode_text_result_event(
            command.id,
            json{{"text", text_result.text},
                 {"format", text_result.format},
                 {"exported_feature_count", text_result.exported_feature_count}}));
    return true;
  }

  if (command.type == "cam_operation_generate" ||
      command.type == "cam_operation_preview") {
    const bool preview = command.type == "cam_operation_preview";
    const std::string op_id = read_string(command.payload, "op_id");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }

    const auto outcome = polysmith::core::generate_operation_toolpath(
        document.value(), op_id, preview, [&](int percent) {
          polysmith::protocol::write_message(
              polysmith::protocol::make_cam_generation_progress_event(
                  command.id, op_id, percent));
        });

    if (!outcome.found) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_OPERATION_NOT_FOUND", outcome.result.error_message));
      return true;
    }
    if (!outcome.result.ok) {
      polysmith::core::log_warn("cam", "generation failed for '" + op_id +
                                           "': " + outcome.result.error_message);
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "CAM_GENERATION_FAILED", outcome.result.error_message));
      if (!preview) {
        polysmith::protocol::write_message(
            polysmith::protocol::make_cam_generation_result_event(
                command.id, op_id, /*ok=*/false, outcome.result.error_message,
                /*warnings=*/{}));
      }
      return true;
    }
    for (const auto& warning : outcome.result.warnings) {
      polysmith::core::log_warn("cam", warning);
    }
    if (preview) {
      polysmith::core::cam_runtime::store_preview(
          document.value(), op_id, outcome.result.toolpath);
    } else {
      const auto current = manager.get_document();
      for (const auto& op : current->cam.operations) {
        if (op.op_id == op_id) {
          auto updated = op;
          polysmith::core::ToolpathCache cache;
          cache.toolpath_id = "tp:" + op_id;
          cache.status = "up_to_date";
          cache.bounds = outcome.result.toolpath.bounds;
          cache.total_length_mm = outcome.result.toolpath.total_length_mm;
          cache.estimated_time_seconds =
              outcome.result.toolpath.estimated_time_seconds;
          cache.num_moves =
              static_cast<int>(outcome.result.toolpath.moves.size());
          cache.num_rapids = outcome.result.toolpath.num_rapids;
          cache.num_feeds = outcome.result.toolpath.num_feeds;
          updated.toolpath_cache = cache;
          manager.cam_operation_set_generated(op_id, updated, outcome.result.toolpath);
          break;
        }
      }
    }
    const auto updated = manager.get_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(updated.value())));
    if (!preview) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_cam_generation_result_event(
              command.id, op_id, /*ok=*/true, /*error_message=*/"",
              outcome.result.warnings));
    }
    return true;
  }

  return false;
}

}  // namespace polysmith
