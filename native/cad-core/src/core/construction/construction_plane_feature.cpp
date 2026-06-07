#include "core/construction_plane_feature.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace polysmith::core {

namespace {

std::string make_parameters_summary(
    const ConstructionPlaneFeatureParameters& parameters) {
  std::ostringstream stream;
  if (parameters.plane_type == "midplane") {
    stream << "Between " << parameters.source_plane_ids.size() << " sources";
  } else if (parameters.plane_type == "tangent") {
    stream << "Tangent to face";
  } else if (parameters.plane_type == "angle") {
    stream << parameters.angle_degrees << " deg";
  } else {
    stream << parameters.source_plane_id << " · " << parameters.offset << " mm";
  }
  return stream.str();
}

struct Vec3 {
  double x;
  double y;
  double z;
};

double dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return Vec3{
      .x = a.y * b.z - a.z * b.y,
      .y = a.z * b.x - a.x * b.z,
      .z = a.x * b.y - a.y * b.x,
  };
}

double magnitude(const Vec3& value) {
  return std::sqrt(dot(value, value));
}

Vec3 normalize(const Vec3& value, const std::string& message) {
  const double length = magnitude(value);
  if (length <= 1e-9) {
    throw std::runtime_error(message);
  }
  return Vec3{.x = value.x / length, .y = value.y / length, .z = value.z / length};
}

Vec3 rotate_around_axis(const Vec3& value, const Vec3& axis, double radians) {
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  const Vec3 axis_cross_value = cross(axis, value);
  const double axis_dot_value = dot(axis, value);
  return Vec3{
      .x = value.x * c + axis_cross_value.x * s +
           axis.x * axis_dot_value * (1.0 - c),
      .y = value.y * c + axis_cross_value.y * s +
           axis.y * axis_dot_value * (1.0 - c),
      .z = value.z * c + axis_cross_value.z * s +
           axis.z * axis_dot_value * (1.0 - c),
  };
}

}  // namespace

PlaneFrame derive_offset_frame(const PlaneFrame& source_frame, double offset) {
  // The offset slides the source frame along its own normal. Basis
  // vectors stay aligned with the source so a sketch on the new
  // plane keeps a predictable orientation (common CAD tools do the same).
  return PlaneFrame{
      .origin_x = source_frame.origin_x + source_frame.normal_x * offset,
      .origin_y = source_frame.origin_y + source_frame.normal_y * offset,
      .origin_z = source_frame.origin_z + source_frame.normal_z * offset,
      .x_axis_x = source_frame.x_axis_x,
      .x_axis_y = source_frame.x_axis_y,
      .x_axis_z = source_frame.x_axis_z,
      .y_axis_x = source_frame.y_axis_x,
      .y_axis_y = source_frame.y_axis_y,
      .y_axis_z = source_frame.y_axis_z,
      .normal_x = source_frame.normal_x,
      .normal_y = source_frame.normal_y,
      .normal_z = source_frame.normal_z,
  };
}

PlaneFrame derive_midplane_frame(const PlaneFrame& first_frame,
                                 const PlaneFrame& second_frame) {
  const double dot = first_frame.normal_x * second_frame.normal_x +
                     first_frame.normal_y * second_frame.normal_y +
                     first_frame.normal_z * second_frame.normal_z;
  if (std::abs(dot) < 0.999) {
    throw std::runtime_error("Midplane sources must be parallel planes");
  }

  return PlaneFrame{
      .origin_x = (first_frame.origin_x + second_frame.origin_x) * 0.5,
      .origin_y = (first_frame.origin_y + second_frame.origin_y) * 0.5,
      .origin_z = (first_frame.origin_z + second_frame.origin_z) * 0.5,
      .x_axis_x = first_frame.x_axis_x,
      .x_axis_y = first_frame.x_axis_y,
      .x_axis_z = first_frame.x_axis_z,
      .y_axis_x = first_frame.y_axis_x,
      .y_axis_y = first_frame.y_axis_y,
      .y_axis_z = first_frame.y_axis_z,
      .normal_x = first_frame.normal_x,
      .normal_y = first_frame.normal_y,
      .normal_z = first_frame.normal_z,
  };
}

PlaneFrame derive_angle_plane_frame(const PlaneFrame& source_frame,
                                    const ConstructionAxisFrame& axis,
                                    double angle_degrees) {
  if (!std::isfinite(angle_degrees)) {
    throw std::runtime_error("Angle plane requires a finite angle");
  }

  const Vec3 axis_direction = normalize(
      Vec3{
          .x = axis.end_x - axis.start_x,
          .y = axis.end_y - axis.start_y,
          .z = axis.end_z - axis.start_z,
      },
      "Angle plane axis must have length");
  const Vec3 source_normal = normalize(
      Vec3{
          .x = source_frame.normal_x,
          .y = source_frame.normal_y,
          .z = source_frame.normal_z,
      },
      "Angle plane source normal is invalid");
  if (std::abs(dot(axis_direction, source_normal)) > 1e-5) {
    throw std::runtime_error(
        "Angle plane axis must be parallel to the source plane");
  }

  constexpr double kPi = 3.14159265358979323846;
  const double radians = angle_degrees * kPi / 180.0;
  const Vec3 rotated_normal = normalize(
      rotate_around_axis(source_normal, axis_direction, radians),
      "Angle plane normal is invalid");
  const Vec3 y_axis = normalize(cross(rotated_normal, axis_direction),
                                "Angle plane basis is invalid");

  return PlaneFrame{
      .origin_x = axis.start_x,
      .origin_y = axis.start_y,
      .origin_z = axis.start_z,
      .x_axis_x = axis_direction.x,
      .x_axis_y = axis_direction.y,
      .x_axis_z = axis_direction.z,
      .y_axis_x = y_axis.x,
      .y_axis_y = y_axis.y,
      .y_axis_z = y_axis.z,
      .normal_x = rotated_normal.x,
      .normal_y = rotated_normal.y,
      .normal_z = rotated_normal.z,
  };
}

FeatureEntry create_construction_plane_feature(
    int feature_index,
    const std::string& source_plane_id,
    double offset,
    const PlaneFrame& source_frame) {
  if (source_plane_id.empty()) {
    throw std::runtime_error("Construction plane requires a source plane id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "offset",
      .source_plane_id = source_plane_id,
      .source_plane_ids = {source_plane_id},
      .source_axis_id = "",
      .offset = offset,
      .angle_degrees = 0.0,
      .plane_frame = derive_offset_frame(source_frame, offset),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Offset Plane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_midplane_feature(int feature_index,
                                     const std::string& first_source_id,
                                     const std::string& second_source_id,
                                     const PlaneFrame& first_frame,
                                     const PlaneFrame& second_frame) {
  if (first_source_id.empty() || second_source_id.empty()) {
    throw std::runtime_error("Midplane requires two source plane ids");
  }
  if (first_source_id == second_source_id) {
    throw std::runtime_error("Midplane requires two different source planes");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "midplane",
      .source_plane_id = first_source_id,
      .source_plane_ids = {first_source_id, second_source_id},
      .source_axis_id = "",
      .offset = 0.0,
      .angle_degrees = 0.0,
      .plane_frame = derive_midplane_frame(first_frame, second_frame),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Midplane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_tangent_plane_feature(int feature_index,
                                          const std::string& source_face_id,
                                          const PlaneFrame& tangent_frame) {
  if (source_face_id.empty()) {
    throw std::runtime_error("Tangent plane requires a source face id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "tangent",
      .source_plane_id = source_face_id,
      .source_plane_ids = {source_face_id},
      .source_axis_id = "",
      .offset = 0.0,
      .angle_degrees = 0.0,
      .plane_frame = tangent_frame,
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Tangent Plane",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

FeatureEntry create_angle_plane_feature(int feature_index,
                                        const std::string& source_plane_id,
                                        const std::string& source_axis_id,
                                        double angle_degrees,
                                        const PlaneFrame& source_frame,
                                        const ConstructionAxisFrame& axis) {
  if (source_plane_id.empty()) {
    throw std::runtime_error("Angle plane requires a source plane id");
  }
  if (source_axis_id.empty()) {
    throw std::runtime_error("Angle plane requires an axis id");
  }

  ConstructionPlaneFeatureParameters parameters{
      .plane_type = "angle",
      .source_plane_id = source_plane_id,
      .source_plane_ids = {source_plane_id},
      .source_axis_id = source_axis_id,
      .offset = 0.0,
      .angle_degrees = angle_degrees,
      .plane_frame =
          derive_angle_plane_frame(source_frame, axis, angle_degrees),
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "construction_plane",
      .name = "Plane at Angle",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .construction_plane_parameters = parameters,
  };
}

void update_construction_plane(FeatureEntry& feature,
                               double offset,
                               const PlaneFrame& source_frame) {
  if (feature.kind != "construction_plane" ||
      !feature.construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "Only construction_plane features can be updated with an offset");
  }

  ConstructionPlaneFeatureParameters next =
      feature.construction_plane_parameters.value();
  next.offset = offset;
  next.plane_frame = derive_offset_frame(source_frame, offset);

  feature.parameters_summary = make_parameters_summary(next);
  feature.construction_plane_parameters = next;
}

void update_angle_plane(FeatureEntry& feature,
                        double angle_degrees,
                        const PlaneFrame& source_frame,
                        const ConstructionAxisFrame& axis) {
  if (feature.kind != "construction_plane" ||
      !feature.construction_plane_parameters.has_value()) {
    throw std::runtime_error(
        "Only construction_plane features can be updated with an angle");
  }

  ConstructionPlaneFeatureParameters next =
      feature.construction_plane_parameters.value();
  next.angle_degrees = angle_degrees;
  next.plane_frame =
      derive_angle_plane_frame(source_frame, axis, angle_degrees);

  feature.parameters_summary = make_parameters_summary(next);
  feature.construction_plane_parameters = next;
}

}  // namespace polysmith::core
