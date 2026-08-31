#include "core/cam/cam_generate.h"

#include <variant>

#include "core/cam/cam_resolution.h"
#include "core/cam/toolpath_geometry.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

namespace {

void report(const std::function<void(int)>& progress, int percent) {
  if (progress) {
    progress(percent);
  }
}

}  // namespace

CamGenerateOutcome generate_operation_toolpath(
    const DocumentState& document, const std::string& op_id, bool preview,
    const std::function<void(int)>& progress) {
  CamGenerateOutcome outcome;

  const CamOperation* op = nullptr;
  for (const auto& candidate : document.cam.operations) {
    if (candidate.op_id == op_id) {
      op = &candidate;
      break;
    }
  }
  if (op == nullptr) {
    outcome.found = false;
    outcome.result.ok = false;
    outcome.result.error_message = "CAM operation not found: " + op_id;
    return outcome;
  }
  outcome.found = true;

  const CamGenerator* generator = find_cam_generator(op->type);
  if (generator == nullptr) {
    outcome.result.ok = false;
    outcome.result.error_message =
        "No toolpath generator for operation type " + op->type;
    return outcome;
  }

  const ToolEntry* tool = nullptr;
  for (const auto& candidate : document.cam.tool_library) {
    if (candidate.tool_id == op->tool_id) {
      tool = &candidate;
      break;
    }
  }
  if (tool == nullptr) {
    outcome.result.ok = false;
    outcome.result.error_message =
        "The tool used by this operation no longer exists.";
    return outcome;
  }
  // A laser operation needs a laser tool — an endmill would emit
  // nonsense power codes.
  if ((op->type == "laser_cut" || op->type == "laser_test_pattern") &&
      tool->type != "laser") {
    outcome.result.ok = false;
    outcome.result.error_message =
        "This operation requires a laser tool (the selected tool is a " +
        tool->type + ").";
    return outcome;
  }

  const CamSetup* setup = setup_for(document, *op);
  if (setup == nullptr) {
    outcome.result.ok = false;
    outcome.result.error_message =
        "Create a CAM setup before generating toolpaths.";
    return outcome;
  }

  // Resolve geometry references.  Profiles need their owning sketch
  // feature alongside the region pointer (the generator maps to world
  // space through the sketch's plane frame).
  CamGenerateContext context{
      .document = document,
      .setup = *setup,
      .operation = *op,
      .tool = *tool,
      .preview = preview,
  };
  report(progress, 5);

  CompiledBodies bodies;
  bool compiled = false;
  auto ensure_bodies = [&]() {
    if (!compiled) {
      bodies = compile_bodies(document, /*include_meshes=*/false);
      compiled = true;
    }
  };

  // Resolve geometry references through the shared resolver (the same
  // code the refresh pass uses — one source of truth for TNP
  // re-resolution).  Profiles need their owning sketch alongside the
  // region pointer (the generator maps to world space through the
  // sketch's plane frame).
  bool needsFaces = false;
  for (const auto& ref : op->geometry_references.machining_regions) {
    if (std::holds_alternative<FaceAttestation>(ref.attestation)) {
      needsFaces = true;
      break;
    }
  }
  if (needsFaces) {
    ensure_bodies();
  }
  for (const auto& ref : op->geometry_references.machining_regions) {
    std::string message;
    const bool ok = resolve_geometry_reference(
        ref, document, bodies,
        [&](const ResolvedProfileRef& resolved, const FeatureEntry& sketch) {
          context.geometry.profiles.push_back(resolved);
          context.geometry.sketches.push_back(
              &sketch.sketch_parameters.value());
        },
        [&](const ResolvedFaceRef& resolved) {
          context.geometry.faces.push_back(resolved);
        },
        message);
    if (!ok) {
      outcome.result.ok = false;
      outcome.result.error_message = message;
      return outcome;
    }
  }
  report(progress, 15);

  // A generator may omit its preview pass; fall back to the full
  // generate function (both are cheap in v1).
  const CamGenerateFn generate_fn =
      (preview && generator->preview) ? generator->preview
                                      : generator->generate;
  outcome.result = generate_fn(context);
  report(progress, 90);

  if (outcome.result.ok) {
    finalize_toolpath(outcome.result.toolpath);
    outcome.result.toolpath.op_id = op->op_id;
  }
  report(progress, 100);
  return outcome;
}

}  // namespace polysmith::core
