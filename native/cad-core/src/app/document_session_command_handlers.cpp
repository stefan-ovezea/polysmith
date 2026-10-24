#include "app/document_session_command_handlers.h"

#include "app/command_context.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace polysmith {

bool handle_document_session_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "ping") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_pong_event(command.id));
    return true;
  }

  if (command.type == "create_document") {
    const auto document = manager.create_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_created_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "get_document_state") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return true;
  }

  if (command.type == "get_session_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_session_state_event(
            command.id,
            polysmith::protocol::to_payload(manager.get_session_state())));
    return true;
  }

  if (command.type == "get_sketch_profiles") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    for (const auto& feature : document->feature_history) {
      if (feature.id != feature_id) {
        continue;
      }
      if (!feature.sketch_parameters.has_value()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "NOT_A_SKETCH",
            "Feature '" + feature_id + "' is not a sketch"));
        return true;
      }
      const auto feature_payload = polysmith::protocol::to_payload(
          feature, /*include_opaque=*/false, /*include_vertices=*/false,
          /*include_profiles=*/true);
      const auto& params = feature_payload.at("sketch_parameters");
      polysmith::protocol::write_message(
          polysmith::protocol::make_sketch_profiles_event(
              command.id,
              polysmith::protocol::json{
                  {"feature_id", feature.id},
                  {"profile_count", params.at("profile_count")},
                  {"profiles", params.at("profiles")},
              }));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_error_event(
        command.id, "FEATURE_NOT_FOUND", "No feature '" + feature_id + "'"));
    return true;
  }

  if (command.type == "get_sketch_diagnostics") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    for (const auto& feature : document->feature_history) {
      if (feature.id != feature_id) {
        continue;
      }
      if (!feature.sketch_parameters.has_value()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "NOT_A_SKETCH",
            "Feature '" + feature_id + "' is not a sketch"));
        return true;
      }
      // S5: loose ends, from the same vertex table the profile walk uses.
      const polysmith::core::SketchDiagnostics diagnostics =
          polysmith::core::compute_sketch_diagnostics(
              feature.sketch_parameters.value());
      polysmith::protocol::json open = polysmith::protocol::json::array();
      for (const auto& end : diagnostics.open) {
        open.push_back({
            {"vertex_id", end.vertex_id},
            {"x", end.x},
            {"y", end.y},
            {"owner_ids", end.owner_ids},
        });
      }
      polysmith::protocol::json untied = polysmith::protocol::json::array();
      for (const auto& pair : diagnostics.untied) {
        untied.push_back({
            {"vertex_a_id", pair.vertex_a_id},
            {"vertex_b_id", pair.vertex_b_id},
            {"gap", pair.gap},
        });
      }
      polysmith::protocol::write_message(
          polysmith::protocol::make_sketch_diagnostics_event(
              command.id,
              polysmith::protocol::json{
                  {"feature_id", feature.id},
                  {"open_count", open.size()},
                  {"untied_count", untied.size()},
                  {"open", open},
                  {"untied", untied},
              }));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_error_event(
        command.id, "FEATURE_NOT_FOUND", "No feature '" + feature_id + "'"));
    return true;
  }

  if (command.type == "preview_auto_constraints") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    // Read-only: this asks the detector what it WOULD propose. Nothing is
    // applied here (S11's apply pass is a separate command).
    std::string feature_id =
        read_optional_string(command.payload, "feature_id").value_or("");
    if (feature_id.empty() && document->active_sketch_feature_id.has_value()) {
      feature_id = document->active_sketch_feature_id.value();
    }
    if (feature_id.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NOT_A_SKETCH", "No sketch is active or named"));
      return true;
    }
    const double tolerance =
        read_optional_dimension(command.payload, "tolerance", 0.5);
    const double angle_tolerance =
        read_optional_dimension(command.payload, "angle_tolerance", 1.0);
    for (const auto& feature : document->feature_history) {
      if (feature.id != feature_id) {
        continue;
      }
      if (!feature.sketch_parameters.has_value()) {
        polysmith::protocol::write_message(
            polysmith::protocol::make_error_event(
                command.id, "NOT_A_SKETCH",
                "Feature '" + feature_id + "' is not a sketch"));
        return true;
      }
      const auto& parameters = feature.sketch_parameters.value();
      const auto candidates = polysmith::core::detect_auto_constraints(
          parameters, tolerance, angle_tolerance);
      polysmith::protocol::json entries = polysmith::protocol::json::array();
      for (const auto& candidate : candidates) {
        entries.push_back({
            {"kind", candidate.kind},
            {"target_ids", candidate.target_ids},
            {"error", candidate.error},
            {"reason", candidate.reason},
        });
      }
      polysmith::protocol::write_message(
          polysmith::protocol::make_auto_constraints_preview_event(
              command.id,
              polysmith::protocol::json{
                  {"feature_id", feature.id},
                  {"candidate_count", entries.size()},
                  {"candidates", entries},
                  // -1 means the solver has not run on this sketch; the caller
                  // must treat it as unknown, not as "fully constrained".
                  {"current_dofs", parameters.solver_dofs},
              }));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_error_event(
        command.id, "FEATURE_NOT_FOUND", "No feature '" + feature_id + "'"));
    return true;
  }

  if (command.type == "get_viewport_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_viewport_state_event(
            command.id,
            polysmith::protocol::to_payload(
                polysmith::core::build_viewport_state(manager.get_document()))));
    return true;
  }

  if (command.type == "export_document") {
    const auto result = manager.export_document_as_step(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_document_stl") {
    const auto result = manager.export_document_as_stl(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_body_stl") {
    const auto result = manager.export_body_as_stl(
        read_string(command.payload, "file_path"),
        read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_body_step") {
    const auto result = manager.export_body_as_step(
        read_string(command.payload, "file_path"),
        read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "save_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    manager.save_document_to_path(file_path);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_saved_event(command.id, file_path));
    return true;
  }

  if (command.type == "load_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    const auto document = manager.load_document_from_path(file_path);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
