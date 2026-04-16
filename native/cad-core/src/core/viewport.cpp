#include "core/viewport.h"

#include <algorithm>

namespace polysmith::core {
namespace {

constexpr double kBoxSpacing = 10.0;
constexpr double kReferencePlaneSize = 120.0;
constexpr double kSketchPlaneOffset = 0.2;

ViewportSketchLinePrimitive make_sketch_line_primitive(const SketchLine& line,
                                                       const std::string& plane_id,
                                                       bool is_selected) {
  if (plane_id == "ref-plane-xy") {
    return ViewportSketchLinePrimitive{
        .line_id = line.id,
        .plane_id = plane_id,
        .start_x = line.start_x,
        .start_y = kSketchPlaneOffset,
        .start_z = line.start_y,
        .end_x = line.end_x,
        .end_y = kSketchPlaneOffset,
        .end_z = line.end_y,
        .is_selected = is_selected,
        .constraint_hint = line.constraint_hint,
    };
  }

  if (plane_id == "ref-plane-yz") {
    return ViewportSketchLinePrimitive{
        .line_id = line.id,
        .plane_id = plane_id,
        .start_x = kSketchPlaneOffset,
        .start_y = line.start_x,
        .start_z = line.start_y,
        .end_x = kSketchPlaneOffset,
        .end_y = line.end_x,
        .end_z = line.end_y,
        .is_selected = is_selected,
        .constraint_hint = line.constraint_hint,
    };
  }

  return ViewportSketchLinePrimitive{
      .line_id = line.id,
      .plane_id = plane_id,
      .start_x = line.start_x,
      .start_y = line.start_y,
      .start_z = kSketchPlaneOffset,
      .end_x = line.end_x,
      .end_y = line.end_y,
      .end_z = kSketchPlaneOffset,
      .is_selected = is_selected,
      .constraint_hint = line.constraint_hint,
  };
}

ViewportSketchCirclePrimitive make_sketch_circle_primitive(
    const SketchCircle& circle, const std::string& plane_id, bool is_selected) {
  if (plane_id == "ref-plane-xy") {
    return ViewportSketchCirclePrimitive{
        .circle_id = circle.id,
        .plane_id = plane_id,
        .center_x = circle.center_x,
        .center_y = kSketchPlaneOffset,
        .center_z = circle.center_y,
        .radius = circle.radius,
        .is_selected = is_selected,
    };
  }

  if (plane_id == "ref-plane-yz") {
    return ViewportSketchCirclePrimitive{
        .circle_id = circle.id,
        .plane_id = plane_id,
        .center_x = kSketchPlaneOffset,
        .center_y = circle.center_x,
        .center_z = circle.center_y,
        .radius = circle.radius,
        .is_selected = is_selected,
    };
  }

  return ViewportSketchCirclePrimitive{
      .circle_id = circle.id,
      .plane_id = plane_id,
      .center_x = circle.center_x,
      .center_y = circle.center_y,
      .center_z = kSketchPlaneOffset,
      .radius = circle.radius,
      .is_selected = is_selected,
  };
}

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
  if (!document.has_value()) {
    return ViewportState{
        .has_active_document = false,
        .boxes = {},
        .cylinders = {},
        .reference_planes = {},
        .reference_axes = {},
        .sketch_lines = {},
        .sketch_circles = {},
        .scene_width = 0.0,
        .scene_height = 0.0,
        .scene_depth = 0.0,
        .scene_bounds =
            ViewportSceneBounds{
                .center_x = 0.0,
                .center_y = 0.0,
                .center_z = 0.0,
                .width = 0.0,
                .height = 0.0,
                .depth = 0.0,
                .max_dimension = 0.0,
            },
    };
  }

  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  double current_x_offset = 0.0;
  double max_height = 0.0;
  double max_depth = 0.0;

  for (const auto& feature : document->feature_history) {
    const bool is_selected =
        document->selected_feature_id.has_value() &&
        document->selected_feature_id.value() == feature.id;

    if (feature.kind == "box" && feature.box_parameters.has_value()) {
      const auto& parameters = feature.box_parameters.value();
      boxes.push_back(ViewportBoxPrimitive{
          .id = feature.id,
          .label = feature.name,
          .width = parameters.width,
          .height = parameters.height,
          .depth = parameters.depth,
          .x_offset = current_x_offset,
          .center_x = current_x_offset + parameters.width / 2.0,
          .center_y = parameters.height / 2.0,
          .center_z = parameters.depth / 2.0,
          .is_selected = is_selected,
      });

      current_x_offset += parameters.width + kBoxSpacing;
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (parameters.depth > max_depth) {
        max_depth = parameters.depth;
      }
      continue;
    }

    if (feature.kind == "cylinder" && feature.cylinder_parameters.has_value()) {
      const auto& parameters = feature.cylinder_parameters.value();
      const double diameter = parameters.radius * 2.0;

      cylinders.push_back(ViewportCylinderPrimitive{
          .id = feature.id,
          .label = feature.name,
          .radius = parameters.radius,
          .height = parameters.height,
          .x_offset = current_x_offset,
          .center_x = current_x_offset + diameter / 2.0,
          .center_y = parameters.height / 2.0,
          .center_z = diameter / 2.0,
          .is_selected = is_selected,
      });

      current_x_offset += diameter + kBoxSpacing;
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (diameter > max_depth) {
        max_depth = diameter;
      }
      continue;
    }

    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      for (const auto& line : feature.sketch_parameters->lines) {
        const bool is_selected_sketch_entity =
            document->selected_sketch_entity_id.has_value() &&
            document->selected_sketch_entity_id.value() == line.id;
        sketch_lines.push_back(
            make_sketch_line_primitive(line,
                                       feature.sketch_parameters->plane_id,
                                       is_selected_sketch_entity));
      }
      for (const auto& circle : feature.sketch_parameters->circles) {
        const bool is_selected_sketch_entity =
            document->selected_sketch_entity_id.has_value() &&
            document->selected_sketch_entity_id.value() == circle.id;
        sketch_circles.push_back(make_sketch_circle_primitive(
            circle,
            feature.sketch_parameters->plane_id,
            is_selected_sketch_entity));
      }
    }
  }

  const double scene_width =
      (boxes.empty() && cylinders.empty()) ? 0.0 : current_x_offset - kBoxSpacing;
  const double reference_extent =
      std::max({kReferencePlaneSize, scene_width, max_height, max_depth, 1.0});
  const double scene_width_with_references = std::max(scene_width, reference_extent);
  const double scene_height_with_references = std::max(max_height, reference_extent);
  const double scene_depth_with_references = std::max(max_depth, reference_extent);

  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-xy",
      .label = "XY Plane",
      .orientation = "xy",
      .center_x = reference_extent / 2.0,
      .center_y = 0.0,
      .center_z = reference_extent / 2.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = document->selected_reference_id.has_value() &&
                     document->selected_reference_id.value() == "ref-plane-xy",
      .is_active_sketch_plane = document->active_sketch_plane_id.has_value() &&
                                document->active_sketch_plane_id.value() ==
                                    "ref-plane-xy",
  });
  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-yz",
      .label = "YZ Plane",
      .orientation = "yz",
      .center_x = 0.0,
      .center_y = reference_extent / 2.0,
      .center_z = reference_extent / 2.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = document->selected_reference_id.has_value() &&
                     document->selected_reference_id.value() == "ref-plane-yz",
      .is_active_sketch_plane = document->active_sketch_plane_id.has_value() &&
                                document->active_sketch_plane_id.value() ==
                                    "ref-plane-yz",
  });
  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-xz",
      .label = "XZ Plane",
      .orientation = "xz",
      .center_x = reference_extent / 2.0,
      .center_y = reference_extent / 2.0,
      .center_z = 0.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = document->selected_reference_id.has_value() &&
                     document->selected_reference_id.value() == "ref-plane-xz",
      .is_active_sketch_plane = document->active_sketch_plane_id.has_value() &&
                                document->active_sketch_plane_id.value() ==
                                    "ref-plane-xz",
  });

  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-x",
      .label = "X",
      .axis = "x",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = reference_extent,
      .end_y = 0.0,
      .end_z = 0.0,
  });
  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-y",
      .label = "Y",
      .axis = "y",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = 0.0,
      .end_y = reference_extent,
      .end_z = 0.0,
  });
  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-z",
      .label = "Z",
      .axis = "z",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = 0.0,
      .end_y = 0.0,
      .end_z = reference_extent,
  });

  const ViewportSceneBounds scene_bounds = {
      .center_x = scene_width_with_references / 2.0,
      .center_y = scene_height_with_references / 2.0,
      .center_z = scene_depth_with_references / 2.0,
      .width = scene_width_with_references,
      .height = scene_height_with_references,
      .depth = scene_depth_with_references,
      .max_dimension = std::max({scene_width_with_references,
                                 scene_height_with_references,
                                 scene_depth_with_references}),
  };

  return ViewportState{
      .has_active_document = true,
      .boxes = boxes,
      .cylinders = cylinders,
      .reference_planes = reference_planes,
      .reference_axes = reference_axes,
      .sketch_lines = sketch_lines,
      .sketch_circles = sketch_circles,
      .scene_width = scene_width_with_references,
      .scene_height = scene_height_with_references,
      .scene_depth = scene_depth_with_references,
      .scene_bounds = scene_bounds,
  };
}

}  // namespace polysmith::core
