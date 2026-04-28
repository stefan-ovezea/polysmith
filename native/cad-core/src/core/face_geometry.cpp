#include "core/face_geometry.h"

#include <algorithm>

#include "core/document.h"

namespace polysmith::core {
namespace {

struct ParsedFaceId {
  std::string owner_id;
  std::string suffix;
};

std::optional<ParsedFaceId> parse_face_id(const std::string& face_id) {
  // Expected layout: "{owner_id}:face:{suffix}".
  const std::string separator = ":face:";
  const auto pos = face_id.find(separator);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  return ParsedFaceId{
      .owner_id = face_id.substr(0, pos),
      .suffix = face_id.substr(pos + separator.size()),
  };
}

const FeatureEntry* find_feature(const DocumentState& document,
                                 const std::string& feature_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id == feature_id) {
      return &feature;
    }
  }
  return nullptr;
}

// Compute a world-space point from a plane frame's local (u, v) coordinates
// plus an offset along the plane's normal.
FaceOutlinePoint plane_to_world(const PlaneFrame& frame,
                                double u,
                                double v,
                                double w) {
  return FaceOutlinePoint{
      .x = frame.origin_x + frame.x_axis_x * u + frame.y_axis_x * v +
           frame.normal_x * w,
      .y = frame.origin_y + frame.x_axis_y * u + frame.y_axis_y * v +
           frame.normal_y * w,
      .z = frame.origin_z + frame.x_axis_z * u + frame.y_axis_z * v +
           frame.normal_z * w,
  };
}

std::optional<FaceOutline> outline_for_extrude(
    const ExtrudeFeatureParameters& parameters,
    const std::string& suffix) {
  if (!parameters.plane_frame.has_value()) {
    // Legacy origin-plane extrudes (no plane_frame) are not supported by
    // the projection helper yet.
    return std::nullopt;
  }
  const PlaneFrame& frame = parameters.plane_frame.value();
  const double depth = parameters.depth;

  if (parameters.profile_kind == "rectangle") {
    const double u0 = parameters.start_x;
    const double v0 = parameters.start_y;
    const double u1 = parameters.start_x + parameters.width;
    const double v1 = parameters.start_y + parameters.height;

    auto rectangle_at_offset = [&](double offset) {
      FaceOutline outline{};
      outline.kind = "rectangle";
      outline.rectangle_corners = {
          plane_to_world(frame, u0, v0, offset),
          plane_to_world(frame, u1, v0, offset),
          plane_to_world(frame, u1, v1, offset),
          plane_to_world(frame, u0, v1, offset),
      };
      return outline;
    };

    if (suffix == "base") {
      return rectangle_at_offset(0.0);
    }
    if (suffix == "top") {
      return rectangle_at_offset(depth);
    }

    // Side faces of a rectangular extrude. Each side spans one base edge
    // (length L) and the depth axis (height D), so the four corners are:
    //   (edge_start, 0), (edge_end, 0), (edge_end, depth), (edge_start, depth)
    // expressed in the (u/v base plane, normal) frame.
    auto side_face = [&](double su, double sv, double eu, double ev) {
      FaceOutline outline{};
      outline.kind = "rectangle";
      outline.rectangle_corners = {
          plane_to_world(frame, su, sv, 0.0),
          plane_to_world(frame, eu, ev, 0.0),
          plane_to_world(frame, eu, ev, depth),
          plane_to_world(frame, su, sv, depth),
      };
      return outline;
    };

    if (suffix == "front") {
      return side_face(u0, v0, u1, v0);
    }
    if (suffix == "back") {
      return side_face(u0, v1, u1, v1);
    }
    if (suffix == "left") {
      return side_face(u0, v0, u0, v1);
    }
    if (suffix == "right") {
      return side_face(u1, v0, u1, v1);
    }

    return std::nullopt;
  }

  if (parameters.profile_kind == "circle") {
    if (suffix != "top" && suffix != "base") {
      return std::nullopt;
    }
    const double offset = suffix == "top" ? depth : 0.0;
    FaceOutline outline{};
    outline.kind = "circle";
    outline.circle_center =
        plane_to_world(frame, parameters.start_x, parameters.start_y, offset);
    outline.circle_axis = FaceOutlinePoint{
        .x = frame.normal_x,
        .y = frame.normal_y,
        .z = frame.normal_z,
    };
    outline.circle_radius = parameters.radius;
    return outline;
  }

  // Polygon profiles are not yet supported by Project.
  return std::nullopt;
}

}  // namespace

std::optional<FaceOutline> compute_face_outline(const DocumentState& document,
                                                const std::string& face_id) {
  const auto parsed = parse_face_id(face_id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const FeatureEntry* feature = find_feature(document, parsed->owner_id);
  if (feature == nullptr) {
    return std::nullopt;
  }

  if (feature->kind == "extrude" && feature->extrude_parameters.has_value()) {
    return outline_for_extrude(feature->extrude_parameters.value(),
                               parsed->suffix);
  }

  // Box and cylinder source features are placed in viewport-space using a
  // running x-offset and are not supported by the projection helper yet.
  return std::nullopt;
}

}  // namespace polysmith::core
