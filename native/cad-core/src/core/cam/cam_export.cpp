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
  const auto type = post_processor_type(document);

  // Collect the exportable operations first so the program-end footer
  // lands on the LAST operation that actually renders — an M2/M30
  // mid-file halts most controllers.
  struct PendingOp {
    std::string op_name;
    double spindle_rpm = 8000.0;
    std::array<double, 3> wcs_origin = {0.0, 0.0, 0.0};
    std::optional<LaserCutParameters> laser;
    const CamSetup* setup = nullptr;  // the op's setup (multi-setup)
    Toolpath toolpath;
    ToolEntry tool;
    std::string skip_message;  // non-empty → op skipped, comment only
  };
  std::vector<PendingOp> pending;
  for (const auto& op : document.cam.operations) {
    if (!op.enabled) {
      continue;
    }
    // Multi-setup: every operation exports through ITS setup.
    const CamSetup* setup = setup_for(document, op);
    if (setup == nullptr) {
      polysmith::core::log_warn(
          "cam", "gcode export: skipping '" + op.name +
                     "': the document has no CAM setup");
      continue;
    }
    PendingOp entry;
    entry.op_name = op.name;
    entry.spindle_rpm = op.parameters.spindle_rpm;
    entry.setup = setup;
    entry.wcs_origin = setup->wcs_origin.position.value_or(
        std::array<double, 3>{0.0, 0.0, 0.0});
    entry.laser =
        op.type == "laser_cut" ? op.parameters.laser : std::nullopt;

    // Use the cached path when it is current; generate on demand
    // otherwise.  Export has no side effects on the cache.
    const Toolpath* cached =
        cam_runtime::cached_toolpath(document, op.op_id);
    if (cached != nullptr) {
      entry.toolpath = *cached;
    } else {
      const auto outcome = generate_operation_toolpath(document, op.op_id,
                                                       /*preview=*/false);
      if (!outcome.found || !outcome.result.ok) {
        entry.skip_message =
            outcome.result.ok ? "operation not found"
                              : outcome.result.error_message;
      } else {
        entry.toolpath = outcome.result.toolpath;
      }
    }

    const ToolEntry* tool = nullptr;
    for (const auto& candidate : document.cam.tool_library) {
      if (candidate.tool_id == op.tool_id) {
        tool = &candidate;
        break;
      }
    }
    if (tool == nullptr && entry.skip_message.empty()) {
      entry.skip_message = "the tool no longer exists";
    }
    entry.tool = tool != nullptr ? *tool : ToolEntry{};
    pending.push_back(std::move(entry));
  }

  // The footer op: the LAST entry without a skip message.
  size_t footerIndex = pending.size();
  while (footerIndex > 0 && !pending[footerIndex - 1].skip_message.empty()) {
    --footerIndex;
  }

  std::vector<std::string> all_lines;
  for (size_t i = 0; i < pending.size(); ++i) {
    const auto& entry = pending[i];
    all_lines.push_back("");
    all_lines.push_back("(operation: " + entry.op_name + ")");
    if (!entry.skip_message.empty()) {
      const std::string message = entry.skip_message;
      polysmith::core::log_warn("cam", "gcode export: skipping '" +
                                           entry.op_name + "': " + message);
      all_lines.push_back("(skipped '" + entry.op_name + "': " + message +
                          ")");
      continue;
    }
    PostContext context{
        .toolpath = entry.toolpath,
        .setup = *entry.setup,
        .tool = entry.tool,
        .op_name = entry.op_name,
        .spindle_rpm = entry.spindle_rpm,
        .wcs_origin = entry.wcs_origin,
        .laser = entry.laser,
    };
    const auto lines = post_process(type, context,
                                    /*include_footer=*/i + 1 == footerIndex);
    if (lines.empty()) {
      polysmith::core::log_warn(
          "cam", "gcode export: skipping '" + entry.op_name +
                     "': unknown post processor '" + type + "'");
      all_lines.push_back("(skipped '" + entry.op_name +
                          "': unknown post processor '" + type + "')");
      continue;
    }
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
