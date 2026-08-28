#include "core/cam/cam_export.h"

#include <fstream>
#include <optional>
#include <sstream>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/post_processor.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"

namespace polysmith::core {

namespace {

PostProcessorOptions export_options(const DocumentState& document) {
  if (document.cam.post_processor.has_value()) {
    return document.cam.post_processor->options;
  }
  return PostProcessorOptions{};
}

std::string post_processor_type(const DocumentState& document) {
  if (document.cam.post_processor.has_value() &&
      !document.cam.post_processor->type.empty()) {
    return document.cam.post_processor->type;
  }
  return "grbl";
}

}  // namespace

CamExportResult export_cam_gcode(const DocumentState& document,
                                 const std::string& file_path) {
  CamExportResult result;
  result.file_path = file_path;

  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }
  if (document.cam.setups.empty()) {
    throw std::runtime_error("Create a CAM setup before exporting G-code");
  }
  const CamSetup& setup = document.cam.setups[0];

  const auto options = export_options(document);
  const auto type = post_processor_type(document);

  std::vector<std::string> all_lines;
  for (const auto& op : document.cam.operations) {
    if (!op.enabled) {
      continue;
    }
    all_lines.push_back("");
    all_lines.push_back("(operation: " + op.name + ")");

    // Use the cached path when it is current; generate on demand
    // otherwise.  Export has no side effects on the cache.
    Toolpath toolpath;
    const Toolpath* cached =
        cam_runtime::cached_toolpath(document, op.op_id);
    if (cached != nullptr) {
      toolpath = *cached;
    } else {
      const auto outcome = generate_operation_toolpath(document, op.op_id,
                                                       /*preview=*/false);
      if (!outcome.found || !outcome.result.ok) {
        const std::string message =
            outcome.result.ok ? "operation not found"
                              : outcome.result.error_message;
        polysmith::core::log_warn(
            "cam", "gcode export: skipping '" + op.name + "': " + message);
        all_lines.push_back("(skipped '" + op.name + "': " + message + ")");
        continue;
      }
      toolpath = outcome.result.toolpath;
    }

    const ToolEntry* tool = nullptr;
    for (const auto& candidate : document.cam.tool_library) {
      if (candidate.tool_id == op.tool_id) {
        tool = &candidate;
        break;
      }
    }
    if (tool == nullptr) {
      all_lines.push_back("(skipped '" + op.name +
                          "': the tool no longer exists)");
      continue;
    }

    PostContext context{
        .toolpath = toolpath,
        .options = options,
        .setup = setup,
        .tool = *tool,
        .op_name = op.name,
        .spindle_rpm = op.parameters.spindle_rpm,
        .wcs_origin = setup.wcs_origin.position.value_or(
            std::array<double, 3>{0.0, 0.0, 0.0}),
        .laser = op.type == "laser_cut" ? op.parameters.laser
                                        : std::nullopt,
    };
    const auto lines = post_process(type, context);
    all_lines.insert(all_lines.end(), lines.begin(), lines.end());
    ++result.exported_feature_count;
  }

  std::ostringstream buffer;
  for (const auto& line : all_lines) {
    buffer << line << "\n";
  }

  std::ofstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for writing: " + file_path);
  }
  stream << buffer.str();
  if (!stream.good()) {
    throw std::runtime_error("Failed to write G-code to: " + file_path);
  }

  return result;
}

}  // namespace polysmith::core
