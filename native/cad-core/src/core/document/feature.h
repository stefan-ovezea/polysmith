#pragma once

#include <optional>
#include <string>

#include "core/construction/construction_types.h"
#include "core/document/body_feature_types.h"
#include "core/document/plane_frame.h"
#include "core/extrude/extrude_types.h"
#include "core/loft/loft_types.h"
#include "core/plugin/plugin_types.h"
#include "core/primitive/primitive_types.h"
#include "core/revolve/revolve_types.h"
#include "core/sketch/sketch_types.h"
#include "core/sweep/sweep_types.h"

namespace polysmith::core {

struct FeatureEntry {
  std::string id;
  std::string kind;
  std::string name;
  std::string status;
  std::string parameters_summary;
  // When true, the feature is excluded from body compilation and from
  // legacy primitive emission. The feature still appears in the
  // timeline / hierarchy (rendered dimmed by the UI) and can be
  // unsuppressed later. Downstream features that reference a
  // suppressed parent (e.g. an extrude whose sketch is suppressed)
  // silently no-op via the existing "missing input" fallbacks.
  bool suppressed = false;
  // Set by `refresh_history_dependencies` when this feature references
  // upstream geometry (a face-based sketch plane, an extrude on a
  // sketch, etc.) that can no longer be resolved against the current
  // document state — e.g. the original face was consumed by a later
  // boolean cut. The frame stays at its last-known value so the UI
  // still has something to render; the timeline surfaces the warning
  // via this flag plus the message below.
  bool dependency_broken = false;
  // Human-readable explanation of the broken dependency (shown as the
  // tooltip on the warning-coloured timeline button). Empty when
  // `dependency_broken` is false.
  std::string dependency_warning;
  std::optional<BoxFeatureParameters> box_parameters;
  std::optional<CylinderFeatureParameters> cylinder_parameters;
  std::optional<ExtrudeFeatureParameters> extrude_parameters;
  std::optional<SketchFeatureParameters> sketch_parameters;
  std::optional<FilletFeatureParameters> fillet_parameters;
  std::optional<ChamferFeatureParameters> chamfer_parameters;
  std::optional<ShellFeatureParameters> shell_parameters;
  std::optional<ConstructionPlaneFeatureParameters> construction_plane_parameters;
  std::optional<ConstructionAxisFeatureParameters> construction_axis_parameters;
  std::optional<ConstructionPointFeatureParameters> construction_point_parameters;
  std::optional<LoftFeatureParameters> loft_parameters;
  std::optional<RevolveFeatureParameters> revolve_parameters;
  std::optional<SweepFeatureParameters> sweep_parameters;
  std::optional<HoleFeatureParameters> hole_parameters;
  std::optional<HelixFeatureParameters> helix_parameters;
  std::optional<ThreadFeatureParameters> thread_parameters;
  std::optional<FastenerFeatureParameters> fastener_parameters;
  std::optional<MoveFeatureParameters> move_parameters;
  std::optional<BodyCopyFeatureParameters> body_copy_parameters;
  std::optional<PluginFeatureParameters> plugin_parameters;
  std::optional<MeshImportFeatureParameters> mesh_import_parameters;
  std::optional<MeshToBodyFeatureParameters> mesh_to_body_parameters;
  std::optional<StepImportFeatureParameters> step_import_parameters;
};

}  // namespace polysmith::core
