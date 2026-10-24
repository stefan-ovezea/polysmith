#include "app.h"

#include <cmath>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include <BRepAdaptor_Surface.hxx>
#include <Message.hxx>
#include <Message_PrinterOStream.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/cam/cam_generate.h"
#include "core/cam/cam_profile_reference.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/machine_library.h"
#include "core/cam/post_processor.h"
#include "core/cam/tool_library.h"
#include "core/cam/tool_table_io.h"
#include "core/document/document.h"
#include "core/document/format/journal.h"
#include "core/drawing/drawing_annotation_geometry.h"
#include "core/drawing/drawing_dimension_geometry.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_templates.h"
#include "core/sketch/formula_eval.h"
#include "core/diagnostics/logger.h"
#include "core/sketch/trim_engine.h"
#include "core/viewport/viewport.h"
#include "protocol/ipc.h"
#include "protocol/serialization.h"

#include "app/body_modifier_command_handlers.h"
#include "app/cam_command_handlers.h"
#include "app/command_context.h"
#include "app/construction_command_handlers.h"
#include "app/corner_trim_command_handlers.h"
#include "app/document_session_command_handlers.h"
#include "app/drawing_command_handlers.h"
#include "app/drawing_template_command_handlers.h"
#include "app/extrude_command_handlers.h"
#include "app/feature_command_handlers.h"
#include "app/fillet_command_handlers.h"
#include "app/import_command_handlers.h"
#include "app/pcb_command_handlers.h"
#include "app/sketch_create_command_handlers.h"
#include "app/sketch_edit_command_handlers.h"
#include "app/sketch_tool_command_handlers.h"
#include "app/sketch_utility_command_handlers.h"
#include "app/solid_feature_command_handlers.h"
#include "app/threaded_command_handlers.h"
#include "app/trim_preview_command_handlers.h"

namespace polysmith {
namespace {

using polysmith::core::DocumentManager;
using polysmith::core::BoxFeatureParameters;
using polysmith::core::CylinderFeatureParameters;
using polysmith::core::ExtrudeFeatureParameters;
using polysmith::core::FastenerFeatureParameters;
using polysmith::core::HelixFeatureParameters;
using polysmith::core::HoleFeatureParameters;
using polysmith::core::MoveFeatureParameters;
using polysmith::core::PluginFeatureParameters;
using polysmith::core::PluginGeometryOperation;
using polysmith::core::ThreadFeatureParameters;
using polysmith::protocol::CommandMessage;

bool should_journal(const std::string& type) {
  if (type == "ping") {
    return false;
  }
  if (type.rfind("get_", 0) == 0) {
    return false;
  }
  return type != "cam_setup_get" && type != "cam_stock_get" &&
         type != "cam_tool_list" && type != "cam_post_list" &&
         type != "cam_machine_list" && type != "cam_tool_library_list";
}

}  // namespace

void CadCoreApp::init_occt() const {
  // OCCT's default messenger prints internal warnings (e.g. the
  // Font_FontMgr "unable to find font ... is used instead" notice)
  // to std::cout. A bare text line on stdout breaks the JSON protocol
  // stream the Tauri bridge parses. This is a GUI app with no console,
  // so strip the stream printer entirely — diagnostics go through the
  // structured logger, not the messenger.
  Message::DefaultMessenger()->RemovePrinters(
      STANDARD_TYPE(Message_PrinterOStream));

  polysmith::core::log_info("cad_core", "Starting OCCT smoke test...");

  const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

  if (box.IsNull()) {
    polysmith::core::log_error("cad_core", "OCCT smoke test failed: shape is null");
    return;
  }

  polysmith::core::log_info("cad_core", "OCCT box created successfully");
}

bool CadCoreApp::handle_command_line(const CommandMessage& command) {
  if (handle_document_session_command(command, document_manager())) {
    return true;
  }
  if (handle_import_command(command, document_manager())) {
    return true;
  }
  if (handle_pcb_command(command, document_manager())) {
    return true;
  }
  if (handle_feature_command(command, document_manager())) {
    return true;
  }
  if (handle_fillet_command(command, document_manager())) {
    return true;
  }
  if (handle_body_modifier_command(command, document_manager())) {
    return true;
  }
  if (handle_threaded_command(command, document_manager())) {
    return true;
  }
  if (handle_construction_command(command, document_manager())) {
    return true;
  }
  if (handle_sketch_edit_command(command, document_manager())) {
    return true;
  }
  if (handle_extrude_command(command, document_manager())) {
    return true;
  }
  if (handle_solid_feature_command(command, document_manager())) {
    return true;
  }
  if (handle_sketch_create_command(command, document_manager())) {
    return true;
  }
  if (handle_sketch_utility_command(command, document_manager())) {
    return true;
  }
  if (handle_sketch_tool_command(command, document_manager())) {
    return true;
  }
  if (handle_corner_trim_command(command, document_manager())) {
    return true;
  }
  if (handle_trim_preview_command(command, document_manager())) {
    return true;
  }
  if (handle_cam_command(command, document_manager())) {
    return true;
  }
  if (handle_drawing_command(command, document_manager())) {
    return true;
  }
  if (handle_drawing_template_command(command, document_manager())) {
    return true;
  }
  return false;
}

void CadCoreApp::run() {
  // Send the hello handshake BEFORE any OCCT code.  With /DELAYLOAD
  // the OCCT DLLs load on the first API call (init_occt), and their
  // DllMain corrupts the CRT stdout layer.  We must send hello while
  // stdout is still clean.
  polysmith::protocol::write_message(polysmith::protocol::make_hello_event());
  init_occt();
  // CAM generators register after OCCT is up — never from static
  // initializers, whose order relative to OCCT is undefined.
  polysmith::core::register_builtin_cam_generators();
  // Visible in the Logs panel: verifies the running binary includes the CAM
  // generators (useful when an old core process lingers).  Names come from the
  // registry itself — the previous hardcoded list said "laser_cut,
  // face_milling" while nine generators were registered, which defeats the one
  // thing this line exists for.
  {
    const std::vector<std::string> generators =
        polysmith::core::cam_generator_types();
    std::string listed;
    for (const auto& type : generators) {
      listed += (listed.empty() ? "" : ", ") + type;
    }
    polysmith::core::log_info(
        "cad_core", "CAM generators registered (" +
                        std::to_string(generators.size()) + "): " + listed);
  }

  // Crash evidence. A thrown std::exception is handled below and answered with
  // an error event, so a process that DISAPPEARS is either an access violation
  // or a std::terminate (an exception escaping a noexcept path or a destructor).
  // Neither leaves anything in the UI, and the in-app log is gone with the
  // process. Write the reason to a file first: one reproduction then names the
  // cause instead of "it crashed".
  polysmith::core::install_crash_logger();

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }

    // Parse BEFORE the try so the catch can carry the command id into
    // the error event. The UI correlates responses by id — an error
    // event with an EMPTY id never settled the awaited promise, so a
    // failing command (e.g. vertex projection on an unresolvable id)
    // left the UI hanging with no feedback ("never comes back").
    polysmith::protocol::CommandMessage command;
    try {
      command = polysmith::protocol::parse_command(line);
      // An unhandled command used to fall through silently: no reply, no
      // log, and the UI's awaited promise never settled. Answer it instead,
      // carrying the real id so the caller can fail fast. The dispatch table
      // and protocol/schema/commands.schema.json are kept in step by
      // `pnpm lint:architecture`.
      if (!handle_command_line(command)) {
        const std::string message = "Unknown command: " + command.type;
        polysmith::core::log_error("cad_core", message);
        polysmith::protocol::write_message(
            polysmith::protocol::make_error_event(
                command.id, "UNKNOWN_COMMAND", message));
        continue;
      }
      if (should_journal(command.type)) {
        const auto document = document_manager().get_document();
        if (document.has_value()) {
          polysmith::core::journal_append(document->id, line);
        }
      }
    } catch (const std::runtime_error& error) {
      if (std::string(error.what()) == "__POLYSMITH_SHUTDOWN__") {
        polysmith::core::log_info("cad_core", "Shutdown requested");
        break;
      }

      polysmith::core::log_error("cad_core", error.what());
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_COMMAND", error.what()));
    } catch (const std::exception& error) {
      polysmith::core::log_error("cad_core", error.what());
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_JSON", error.what()));
    }
  }
}

}  // namespace polysmith
