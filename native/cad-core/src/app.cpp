#include "app.h"

#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include <BRepAdaptor_Surface.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"
#include "core/sketch/formula_eval.h"
#include "core/diagnostics/logger.h"
#include "core/sketch/trim_engine.h"
#include "core/viewport/viewport.h"
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
using polysmith::core::MoveFeatureParameters;
using polysmith::core::PluginFeatureParameters;
using polysmith::core::PluginGeometryOperation;
using polysmith::core::ThreadFeatureParameters;
using polysmith::protocol::CommandMessage;

#include "app/impl/command_readers.inc"
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

#include "app/impl/document_session_commands.inc"
#include "app/impl/feature_selection_appearance_commands.inc"
#include "app/impl/feature_operation_commands.inc"
#include "app/impl/sketch_edit_commands.inc"
#include "app/impl/solid_feature_commands.inc"
#include "app/impl/sketch_create_project_commands.inc"
#include "app/impl/parameter_filter_trim_commands.inc"
#include "app/impl/trim_preview_commands.inc"
#include "app/impl/cam_commands.inc"
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
