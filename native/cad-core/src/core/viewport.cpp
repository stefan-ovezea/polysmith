#include "core/viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <GProp_GProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
#include "core/appearance.h"
#include "core/dof_counter.h"
#include "core/feature_shape.h"
#include "core/refresh_dependents.h"
#include "core/sketch_profile.h"

namespace polysmith::core {
namespace {

constexpr double kBoxSpacing = 10.0;
constexpr double kReferencePlaneSize = 120.0;
constexpr double kSketchPlaneOffset = 0.2;
constexpr double kLineDimensionOffset = 12.0;
constexpr double kCircleDimensionOffset = 8.0;
constexpr double kConstraintBadgeOffset = 3.5;
constexpr double kPi = 3.14159265358979323846264338327950288;

struct WorldPoint {
  double x;
  double y;
  double z;
};

struct WorldVector {
  double x;
  double y;
  double z;
};

struct BodyBounds {
  double center_x = 0.0;
  double center_y = 0.0;
  double center_z = 0.0;
  double width = 0.0;
  double height = 0.0;
  double depth = 0.0;
};

BodyBounds bounds_for_shape(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return BodyBounds{};
  }
  try {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) {
      return BodyBounds{};
    }
    Standard_Real min_x = 0.0;
    Standard_Real min_y = 0.0;
    Standard_Real min_z = 0.0;
    Standard_Real max_x = 0.0;
    Standard_Real max_y = 0.0;
    Standard_Real max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);
    return BodyBounds{
        .center_x = (min_x + max_x) * 0.5,
        .center_y = (min_y + max_y) * 0.5,
        .center_z = (min_z + max_z) * 0.5,
        .width = max_x - min_x,
        .height = max_y - min_y,
        .depth = max_z - min_z,
    };
  } catch (const std::exception&) {
    return BodyBounds{};
  }
}

WorldVector subtract_points(const WorldPoint& left, const WorldPoint& right) {
  return WorldVector{
      .x = left.x - right.x,
      .y = left.y - right.y,
      .z = left.z - right.z,
  };
}

WorldVector cross_product(const WorldVector& left, const WorldVector& right) {
  return WorldVector{
      .x = left.y * right.z - left.z * right.y,
      .y = left.z * right.x - left.x * right.z,
      .z = left.x * right.y - left.y * right.x,
  };
}

double vector_length(const WorldVector& vector) {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y +
                   vector.z * vector.z);
}

WorldVector normalize_vector(const WorldVector& vector) {
  const double length = vector_length(vector);
  if (length <= 0.0) {
    return WorldVector{.x = 0.0, .y = 0.0, .z = 0.0};
  }

  return WorldVector{
      .x = vector.x / length,
      .y = vector.y / length,
      .z = vector.z / length,
  };
}

double dot_product(const WorldVector& left, const WorldVector& right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

std::vector<double> make_cosmetic_thread_points(
    double start_x,
    double start_y,
    double start_z,
    double end_x,
    double end_y,
    double end_z,
    double radius,
    double pitch,
    double length,
    double start_offset,
    bool right_handed) {
  if (radius <= 0.0 || pitch <= 0.0 || length <= 0.0) {
    return {};
  }
  const WorldPoint start{.x = start_x, .y = start_y, .z = start_z};
  const WorldPoint end{.x = end_x, .y = end_y, .z = end_z};
  const WorldVector axis = subtract_points(end, start);
  const WorldVector dir = normalize_vector(axis);
  if (vector_length(dir) <= 0.0) {
    return {};
  }

  WorldVector seed{.x = 0.0, .y = 1.0, .z = 0.0};
  if (std::abs(dot_product(dir, seed)) > 0.9) {
    seed = WorldVector{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  WorldVector u = normalize_vector(cross_product(dir, seed));
  if (vector_length(u) <= 0.0) {
    return {};
  }
  const WorldVector v = normalize_vector(cross_product(dir, u));

  const double direction_sign = right_handed ? 1.0 : -1.0;
  const int steps = std::max(
      24, static_cast<int>(std::ceil((length / pitch) * 32.0)));
  std::vector<double> points;
  points.reserve(static_cast<size_t>(steps + 1) * 3);
  for (int i = 0; i <= steps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(steps);
    const double along = start_offset + length * t;
    const double angle = direction_sign * 2.0 * kPi * (along / pitch);
    points.push_back(start.x + dir.x * along + u.x * std::cos(angle) * radius +
                     v.x * std::sin(angle) * radius);
    points.push_back(start.y + dir.y * along + u.y * std::cos(angle) * radius +
                     v.y * std::sin(angle) * radius);
    points.push_back(start.z + dir.z * along + u.z * std::cos(angle) * radius +
                     v.z * std::sin(angle) * radius);
  }
  return points;
}

std::optional<std::string> body_appearance_color(
    const DocumentState& document,
    const std::string& body_id) {
  const auto entry = std::find_if(
      document.appearance.body_colors.begin(),
      document.appearance.body_colors.end(),
      [&](const BodyAppearanceOverride& override_entry) {
        return override_entry.body_id == body_id;
      });
  if (entry == document.appearance.body_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}

std::optional<std::string> face_appearance_color(
    const DocumentState& document,
    const std::string& face_id,
    const std::string& signature) {
  const auto entry = std::find_if(
      document.appearance.face_colors.begin(),
      document.appearance.face_colors.end(),
      [&](const FaceAppearanceOverride& override_entry) {
        return override_entry.face_id == face_id &&
               override_entry.signature == signature;
      });
  if (entry == document.appearance.face_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}

ViewportSolidFace make_solid_face(
    const std::string& owner_id,
    const std::string& owner_kind,
    const std::string& face_suffix,
    const std::string& label,
    const std::string& sketchability,
    double center_x,
    double center_y,
    double center_z,
    double normal_x,
    double normal_y,
    double normal_z,
    const ViewportSolidFace::PlaneFrame& plane_frame,
    double width,
    double height,
    double radius,
    bool is_selected) {
  return ViewportSolidFace{
      .face_id = owner_id + ":face:" + face_suffix,
      .owner_id = owner_id,
      .owner_kind = owner_kind,
      .label = label,
      .sketchability = sketchability,
      .center_x = center_x,
      .center_y = center_y,
      .center_z = center_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
      .plane_frame = plane_frame,
      .width = width,
      .height = height,
      .radius = radius,
      .is_selected = is_selected,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(double origin_x,
                                               double origin_y,
                                               double origin_z,
                                               double x_axis_x,
                                               double x_axis_y,
                                               double x_axis_z,
                                               double y_axis_x,
                                               double y_axis_y,
                                               double y_axis_z,
                                               double normal_x,
                                               double normal_y,
                                               double normal_z) {
  return ViewportSolidFace::PlaneFrame{
      .origin_x = origin_x,
      .origin_y = origin_y,
      .origin_z = origin_z,
      .x_axis_x = x_axis_x,
      .x_axis_y = x_axis_y,
      .x_axis_z = x_axis_z,
      .y_axis_x = y_axis_x,
      .y_axis_y = y_axis_y,
      .y_axis_z = y_axis_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return make_plane_frame(frame.origin_x,
                          frame.origin_y,
                          frame.origin_z,
                          frame.x_axis_x,
                          frame.x_axis_y,
                          frame.x_axis_z,
                          frame.y_axis_x,
                          frame.y_axis_y,
                          frame.y_axis_z,
                          frame.normal_x,
                          frame.normal_y,
                          frame.normal_z);
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const PlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

WorldPoint to_world_point(const std::string& plane_id,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (plane_id == "ref-plane-xy") {
    return WorldPoint{.x = local_x, .y = offset, .z = local_y};
  }

  if (plane_id == "ref-plane-yz") {
    return WorldPoint{.x = offset, .y = local_x, .z = local_y};
  }

  return WorldPoint{.x = local_x, .y = local_y, .z = offset};
}

WorldPoint to_world_point(const SketchFeatureParameters& parameters,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  }

  return to_world_point(parameters.plane_id, local_x, local_y, offset);
}

WorldPoint to_world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

WorldPoint to_world_point(const PlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

ViewportSolidFace::PlaneFrame make_face_frame_for_plane(const std::string& plane_id,
                                                        double origin_x,
                                                        double origin_y,
                                                        double origin_z) {
  if (plane_id == "ref-plane-xy") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            0.0,
                            1.0,
                            0.0);
  }

  if (plane_id == "ref-plane-yz") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            0.0,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            1.0,
                            0.0,
                            0.0);
  }

  return make_plane_frame(origin_x,
                          origin_y,
                          origin_z,
                          1.0,
                          0.0,
                          0.0,
                          0.0,
                          1.0,
                          0.0,
                          0.0,
                          0.0,
                          1.0);
}

ViewportSketchLinePrimitive make_sketch_line_primitive(const SketchLine& line,
                                                       const SketchFeatureParameters& parameters,
                                                       bool is_selected) {
  const WorldPoint start = to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint end = to_world_point(parameters, line.end_x, line.end_y);

  return ViewportSketchLinePrimitive{
      .line_id = line.id,
      .start_point_id = line.start_point_id,
      .end_point_id = line.end_point_id,
      .plane_id = parameters.plane_id,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .is_selected = is_selected,
      .constraint = line.constraint,
      .is_construction = line.is_construction,
  };
}

ViewportSketchPolygonPrimitive make_sketch_polygon_primitive(
    const SketchPolygon& polygon,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, polygon.center_x, polygon.center_y);

  ViewportSketchPolygonPrimitive primitive{
      .polygon_id = polygon.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .sides = polygon.sides,
      .mode = polygon.mode,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = polygon.radius,
      .is_selected = is_selected,
      .is_construction = polygon.is_construction,
  };

  // Compute world-space corners
  const int n = polygon.sides;
  // Align first vertex / edge midpoint with the click direction.
  double angle_offset = -M_PI / 2.0;
  if (polygon.mode == "inscribed") {
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x);
  } else if (polygon.mode == "circumscribed") {
    // Click is edge midpoint; rotate so edge 0 sits there.
    angle_offset = std::atan2(polygon.end_y - polygon.center_y, polygon.end_x - polygon.center_x) + M_PI / n;
  } else if (polygon.mode == "edge") {
    angle_offset = std::atan2(polygon.start_y - polygon.center_y, polygon.start_x - polygon.center_x);
  }
  for (int i = 0; i < n; ++i) {
    double angle = angle_offset + 2.0 * M_PI * i / n;
    double local_x = polygon.center_x + polygon.radius * std::cos(angle);
    double local_y = polygon.center_y + polygon.radius * std::sin(angle);
    if (polygon.mode == "circumscribed") {
      double r = polygon.radius / std::cos(M_PI / n);
      local_x = polygon.center_x + r * std::cos(angle);
      local_y = polygon.center_y + r * std::sin(angle);
    }
    const WorldPoint corner = to_world_point(parameters, local_x, local_y);
    primitive.corner_x.push_back(corner.x);
    primitive.corner_y.push_back(corner.y);
    primitive.corner_z.push_back(corner.z);
  }
  return primitive;
}

ViewportSketchCirclePrimitive make_sketch_circle_primitive(
    const SketchCircle& circle,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, circle.center_x, circle.center_y);

  return ViewportSketchCirclePrimitive{
      .circle_id = circle.id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = circle.radius,
      .is_selected = is_selected,
      .is_construction = circle.is_construction,
  };
}

ViewportSketchArcPrimitive make_sketch_arc_primitive(
    const SketchArc& arc,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, arc.center_x, arc.center_y);
  const WorldPoint start = to_world_point(parameters, arc.start_x, arc.start_y);
  const WorldPoint end = to_world_point(parameters, arc.end_x, arc.end_y);

  return ViewportSketchArcPrimitive{
      .arc_id = arc.id,
      .start_point_id = arc.start_point_id,
      .end_point_id = arc.end_point_id,
      .plane_id = parameters.plane_id,
      .plane_frame = parameters.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(
                                   parameters.plane_frame.value()))
                         : std::nullopt,
      .center_x = center.x,
      .center_y = center.y,
      .center_z = center.z,
      .radius = arc.radius,
      .start_x = start.x,
      .start_y = start.y,
      .start_z = start.z,
      .end_x = end.x,
      .end_y = end.y,
      .end_z = end.z,
      .ccw = arc.ccw,
      .is_selected = is_selected,
      .is_construction = arc.is_construction,
  };
}

ViewportSketchPointPrimitive make_sketch_point_primitive(
    const SketchPoint& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint world = to_world_point(parameters, point.x, point.y);

  return ViewportSketchPointPrimitive{
      .point_id = point.id,
      .plane_id = parameters.plane_id,
      .kind = point.kind,
      .position_x = world.x,
      .position_y = world.y,
      .position_z = world.z,
      .is_fixed = point.is_fixed,
      .is_selected = is_selected,
  };
}

// Render a dimension value as a short, human-friendly string with no
// trailing zeros: 12 -> "12", 12.5 -> "12.5", 12.50 -> "12.5",
// 12.345678 -> "12.35". std::to_string on a double produces six
// trailing decimals which makes the canvas-rendered dimension labels
// noisy ("12.500000 mm"). Two decimals of precision is enough for the
// viewport readout; the underlying value the core stores is unchanged.
std::string format_dimension_value(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text == "-0") {
    text = "0";
  }
  return text;
}

// Centroid (in sketch-local 2D coords) of every line midpoint and
// circle center in the sketch. Used as the "inside" reference when
// placing line dimensions so the dimension line lands on the side of
// the segment that points away from the rest of the sketch — i.e. on
// the outside of a closed profile such as a rectangle. Falls back to
// the line's own midpoint when the sketch has no other entities, in
// which case the offset direction does not matter.
struct SketchCentroid2D {
  double x;
  double y;
  bool has_value;
};

SketchCentroid2D compute_sketch_centroid_2d(
    const SketchFeatureParameters& parameters) {
  double sum_x = 0.0;
  double sum_y = 0.0;
  int count = 0;
  for (const auto& other_line : parameters.lines) {
    sum_x += (other_line.start_x + other_line.end_x) / 2.0;
    sum_y += (other_line.start_y + other_line.end_y) / 2.0;
    count += 1;
  }
  for (const auto& other_circle : parameters.circles) {
    sum_x += other_circle.center_x;
    sum_y += other_circle.center_y;
    count += 1;
  }
  if (count == 0) {
    return SketchCentroid2D{.x = 0.0, .y = 0.0, .has_value = false};
  }
  return SketchCentroid2D{
      .x = sum_x / static_cast<double>(count),
      .y = sum_y / static_cast<double>(count),
      .has_value = true,
  };
}

ViewportSketchDimensionPrimitive make_line_dimension_primitive(
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  double normal_x = length > 0.0 ? -dy / length : 0.0;
  double normal_y = length > 0.0 ? dx / length : 1.0;

  // Flip the normal so it points *away* from the centroid of the rest
  // of the sketch. For a rectangle (or any closed profile) this puts
  // every edge's dimension line on the outside of the shape.
  const double midpoint_x = (line.start_x + line.end_x) / 2.0;
  const double midpoint_y = (line.start_y + line.end_y) / 2.0;
  const SketchCentroid2D centroid = compute_sketch_centroid_2d(parameters);
  const double to_outside_x = centroid.has_value ? midpoint_x - centroid.x : 0.0;
  const double to_outside_y = centroid.has_value ? midpoint_y - centroid.y : 0.0;
  const double to_outside_mag = std::sqrt(
      to_outside_x * to_outside_x + to_outside_y * to_outside_y);
  if (centroid.has_value) {
    if (normal_x * to_outside_x + normal_y * to_outside_y < 0.0) {
      normal_x = -normal_x;
      normal_y = -normal_y;
    }
  }

  // After the centroid flip, ensure the length dimension sits on the
  // opposite side of the line from the angle arc — but only as a
  // tiebreaker when the centroid is ambiguous (the line's midpoint
  // is near the centroid, i.e. isolated line or single entity).
  // When the centroid is well-defined (multi-entity sketch), let
  // the centroid-aware flip handle collision avoidance.
  // The angle arc sweeps from the +X reference (horizontal right)
  // to the line direction. For dy > 0 (up-right line), the arc is at
  // sin(θ) ≤ sin(line_angle) → arc is BELOW the line → normal already
  // above → no flip needed.
  // For dy < 0 (down-right line), the arc is at sin(θ) ≥ sin(line_angle)
  // → arc is ABOVE the line → normal also points above → flip to below.
  if (dy < 0.0 && to_outside_mag < 0.5) {
    normal_x = -normal_x;
    normal_y = -normal_y;
  }

  const WorldPoint anchor_start = to_world_point(parameters, line.start_x, line.start_y);
  const WorldPoint anchor_end = to_world_point(parameters, line.end_x, line.end_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      line.start_x + normal_x * kLineDimensionOffset,
      line.start_y + normal_y * kLineDimensionOffset);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      line.end_x + normal_x * kLineDimensionOffset,
      line.end_y + normal_y * kLineDimensionOffset);
  const WorldPoint label = to_world_point(
      parameters,
      midpoint_x + normal_x * (kLineDimensionOffset + 1.5),
      midpoint_y + normal_y * (kLineDimensionOffset + 1.5));

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_length",
      .entity_id = line.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
}

ViewportSketchDimensionPrimitive make_line_angle_dimension_primitive(
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  // Render the line-angle dimension as an angle arc from the horizontal
  // reference (+X axis) to the line's direction, pivoted at the line's
  // start point. This makes it visually distinct from a line_length
  // dimension — the UI renders angle kinds as arcs with arrowheads.
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);

  // Reference ray: horizontal right (+X axis).
  const double ref_ux = 1.0;
  const double ref_uy = 0.0;

  // Line direction ray (from start to end).
  double line_ux = 1.0;
  double line_uy = 0.0;
  if (length > 0.0) {
    line_ux = dx / length;
    line_uy = dy / length;
  }

  const double pivot_x = line.start_x;
  const double pivot_y = line.start_y;

  // Bisector direction (sum of unit vectors).
  double bx = ref_ux + line_ux;
  double by = ref_uy + line_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -ref_uy;
    by = ref_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  const double kArcRadius = std::max(8.0, std::min(length, 500.0));
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);
  const WorldPoint anchor_end = to_world_point(
      parameters,
      pivot_x + line_ux * kAnchorRadius,
      pivot_y + line_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      pivot_x + line_ux * kArcRadius,
      pivot_y + line_uy * kArcRadius);
  const WorldPoint label = to_world_point(
      parameters,
      pivot_x + bisector_ux * kLabelRadius,
      pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from reference direction to line direction.
  // Use cross-product sign to determine CCW vs CW.
  const double sweep_cross = ref_ux * line_uy - ref_uy * line_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(ref_uy, ref_ux);
  const double arc_end_angle = std::atan2(line_uy, line_ux);

  // Compute reference line endpoints: from pivot along reference direction.
  const WorldPoint ref_line_start = to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters,
      pivot_x + ref_ux * kArcRadius,
      pivot_y + ref_uy * kArcRadius);

  // Arc center in world space (the pivot).
  const WorldPoint arc_center_world = to_world_point(parameters, pivot_x, pivot_y);

  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_angle",
      .entity_id = line.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along reference direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
      .ref_line_end_x = ref_line_end.x,
      .ref_line_end_y = ref_line_end.y,
      .ref_line_end_z = ref_line_end.z,
  };
}

ViewportSketchDimensionPrimitive make_circle_dimension_primitive(
    const SketchCircle& circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint center = to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      circle.center_x - circle.radius,
      circle.center_y);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      circle.center_x + circle.radius,
      circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      circle.center_x + circle.radius + kCircleDimensionOffset,
      circle.center_y);

  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .label = "D " + format_dimension_value(dimension.value * 2.0) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = dimension_start.x,
      .anchor_start_y = dimension_start.y,
      .anchor_start_z = dimension_start.z,
      .anchor_end_x = dimension_end.x,
      .anchor_end_y = dimension_end.y,
      .anchor_end_z = dimension_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
}

// Build the viewport primitive for an angle dimension between two
// lines that share an endpoint. We emit a chord-shaped "dimension
// line" along the imaginary arc between the two outgoing rays so the
// existing UI dimension renderer can draw something sensible without
// arc support: anchor_start / anchor_end land on each line slightly
// past the pivot, dimension_start / dimension_end form a chord at a
// larger radius, and the label sits beyond that on the bisector.
ViewportSketchDimensionPrimitive make_angle_dimension_primitive(
    const SketchLine& line_a,
    const SketchLine& line_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  // Locate the shared endpoint or intersection point.  First try a
  // direct endpoint match; if that fails, check whether one line's
  // endpoint lies on the other line's segment.
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  auto point_on_seg = [](double px, double py,
                         double ax, double ay,
                         double bx, double by,
                         double tol) -> bool {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12)
      return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay)) <= tol;
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx, proj_y = ay + t * dy;
    return std::sqrt((px - proj_x) * (px - proj_x) +
                     (py - proj_y) * (py - proj_y)) <= tol;
  };

  auto dir_away = [](double px, double py,
                     double e0x, double e0y,
                     double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - px) * (e0x - px) + (e0y - py) * (e0y - py));
    double d1 = std::sqrt((e1x - px) * (e1x - px) + (e1y - py) * (e1y - py));
    double tx = d0 > d1 ? e0x : e1x, ty = d0 > d1 ? e0y : e1y;
    double dx = tx - px, dy = ty - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };

  constexpr double kPivotTol = 0.05;
  int a_pivot = -1, b_pivot = -1;

  // Pass 1: direct endpoint–endpoint match.
  for (int i = 0; i < 2 && a_pivot < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <= kPivotTol &&
          std::abs(a_ends[i].second - b_ends[j].second) <= kPivotTol) {
        a_pivot = i; b_pivot = j; break;
      }
    }
  }
  // Pass 2: endpoint-on-segment.
  if (a_pivot < 0) {
    for (int i = 0; i < 2 && a_pivot < 0; ++i) {
      if (point_on_seg(a_ends[i].first, a_ends[i].second,
                       b_ends[0].first, b_ends[0].second,
                       b_ends[1].first, b_ends[1].second, kPivotTol)) {
        a_pivot = i; b_pivot = -1; break;
      }
    }
  }
  if (a_pivot < 0) {
    for (int j = 0; j < 2 && a_pivot < 0; ++j) {
      if (point_on_seg(b_ends[j].first, b_ends[j].second,
                       a_ends[0].first, a_ends[0].second,
                       a_ends[1].first, a_ends[1].second, kPivotTol)) {
        a_pivot = -1; b_pivot = j; break;
      }
    }
  }

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;

  if (a_pivot >= 0 && b_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    a_dx = a_ends[1 - a_pivot].first - pivot_x;
    a_dy = a_ends[1 - a_pivot].second - pivot_y;
    b_dx = b_ends[1 - b_pivot].first - pivot_x;
    b_dy = b_ends[1 - b_pivot].second - pivot_y;
  } else if (a_pivot >= 0) {
    pivot_x = a_ends[a_pivot].first;
    pivot_y = a_ends[a_pivot].second;
    auto ad = dir_away(pivot_x, pivot_y,
                       a_ends[1 - a_pivot].first, a_ends[1 - a_pivot].second,
                       a_ends[a_pivot].first, a_ends[a_pivot].second);
    a_dx = ad.first; a_dy = ad.second;
    auto bd = dir_away(pivot_x, pivot_y,
                       b_ends[0].first, b_ends[0].second,
                       b_ends[1].first, b_ends[1].second);
    b_dx = bd.first; b_dy = bd.second;
  } else if (b_pivot >= 0) {
    pivot_x = b_ends[b_pivot].first;
    pivot_y = b_ends[b_pivot].second;
    auto ad = dir_away(pivot_x, pivot_y,
                       a_ends[0].first, a_ends[0].second,
                       a_ends[1].first, a_ends[1].second);
    a_dx = ad.first; a_dy = ad.second;
    auto bd = dir_away(pivot_x, pivot_y,
                       b_ends[1 - b_pivot].first, b_ends[1 - b_pivot].second,
                       b_ends[b_pivot].first, b_ends[b_pivot].second);
    b_dx = bd.first; b_dy = bd.second;
  } else {
    // Should not happen — add_sketch_angle_dimension validates first.
    pivot_x = a_ends[0].first;  pivot_y = a_ends[0].second;
    a_dx = 1.0; a_dy = 0.0;  b_dx = 0.0; b_dy = 1.0;
  }
  const double a_len = std::sqrt(a_dx * a_dx + a_dy * a_dy);
  const double b_len = std::sqrt(b_dx * b_dx + b_dy * b_dy);
  const double a_ux = a_len > 0.0 ? a_dx / a_len : 1.0;
  const double a_uy = a_len > 0.0 ? a_dy / a_len : 0.0;
  const double b_ux = b_len > 0.0 ? b_dx / b_len : 0.0;
  const double b_uy = b_len > 0.0 ? b_dy / b_len : 1.0;

  // Bisector direction (sum of unit vectors). Normalize defensively
  // because antiparallel rays sum to zero — fall back to A's normal.
  double bx = a_ux + b_ux;
  double by = a_uy + b_uy;
  double blen = std::sqrt(bx * bx + by * by);
  if (blen < 1e-6) {
    bx = -a_uy;
    by = a_ux;
    blen = 1.0;
  }
  const double bisector_ux = bx / blen;
  const double bisector_uy = by / blen;

  const double kArcRadius = std::max(8.0, std::min(std::min(a_len, b_len), 500.0));
  constexpr double kAnchorRadius = 4.0;
  const double kLabelRadius = kArcRadius;

  const WorldPoint anchor_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint anchor_end = to_world_point(
      parameters,
      pivot_x + b_ux * kAnchorRadius,
      pivot_y + b_uy * kAnchorRadius);
  const WorldPoint dimension_start = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);
  const WorldPoint dimension_end = to_world_point(
      parameters,
      pivot_x + b_ux * kArcRadius,
      pivot_y + b_uy * kArcRadius);
  const WorldPoint label = to_world_point(
      parameters,
      pivot_x + bisector_ux * kLabelRadius,
      pivot_y + bisector_uy * kLabelRadius);

  // Compute arc sweep: from line A direction to line B direction.
  const double sweep_cross = a_ux * b_uy - a_uy * b_ux;
  const bool arc_ccw = sweep_cross >= 0.0;
  const double arc_start_angle = std::atan2(a_uy, a_ux);
  const double arc_end_angle = std::atan2(b_uy, b_ux);

  // Reference line along line A direction from pivot
  const WorldPoint ref_line_start = to_world_point(parameters, pivot_x, pivot_y);
  const WorldPoint ref_line_end = to_world_point(
      parameters,
      pivot_x + a_ux * kArcRadius,
      pivot_y + a_uy * kArcRadius);

  // Arc center in world space (the pivot)
  const WorldPoint arc_center_world = to_world_point(parameters, pivot_x, pivot_y);

  // Render the value in degrees (CAD convention) with the same
  // formatter as length / radius so trailing-zero handling stays
  // consistent.
  const double degrees = dimension.value * 180.0 / 3.14159265358979323846;
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "angle",
      .entity_id = line_a.id,
      .label = format_dimension_value(degrees) + "\xc2\xb0",
      .is_selected = is_selected,
      .anchor_start_x = anchor_start.x,
      .anchor_start_y = anchor_start.y,
      .anchor_start_z = anchor_start.z,
      .anchor_end_x = anchor_end.x,
      .anchor_end_y = anchor_end.y,
      .anchor_end_z = anchor_end.z,
      .dimension_start_x = dimension_start.x,
      .dimension_start_y = dimension_start.y,
      .dimension_start_z = dimension_start.z,
      .dimension_end_x = dimension_end.x,
      .dimension_end_y = dimension_end.y,
      .dimension_end_z = dimension_end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,

      // Angle arc geometry
      .arc_center_x = arc_center_world.x,
      .arc_center_y = arc_center_world.y,
      .arc_center_z = arc_center_world.z,
      .arc_radius = kArcRadius,
      .arc_start_angle = arc_start_angle,
      .arc_end_angle = arc_end_angle,
      .arc_ccw = arc_ccw,

      // Reference line (along line A direction from pivot)
      .ref_line_start_x = ref_line_start.x,
      .ref_line_start_y = ref_line_start.y,
      .ref_line_start_z = ref_line_start.z,
      .ref_line_end_x = ref_line_end.x,
      .ref_line_end_y = ref_line_end.y,
      .ref_line_end_z = ref_line_end.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double raw_dx = *dimension.label_x - pivot_x;
    const double raw_dy = *dimension.label_y - pivot_y;
    const double dimension_radius =
        std::max(6.0, std::min(std::sqrt(raw_dx * raw_dx + raw_dy * raw_dy), 500.0));
    const double label_radius = dimension_radius;
    const WorldPoint saved_anchor_start = to_world_point(
        parameters,
        pivot_x + a_ux * kAnchorRadius,
        pivot_y + a_uy * kAnchorRadius);
    const WorldPoint saved_anchor_end = to_world_point(
        parameters,
        pivot_x + b_ux * kAnchorRadius,
        pivot_y + b_uy * kAnchorRadius);
    const WorldPoint saved_dimension_start = to_world_point(
        parameters,
        pivot_x + a_ux * dimension_radius,
        pivot_y + a_uy * dimension_radius);
    const WorldPoint saved_dimension_end = to_world_point(
        parameters,
        pivot_x + b_ux * dimension_radius,
        pivot_y + b_uy * dimension_radius);
    const WorldPoint saved_label = to_world_point(
        parameters,
        pivot_x + bisector_ux * label_radius,
        pivot_y + bisector_uy * label_radius);
    primitive.anchor_start_x = saved_anchor_start.x;
    primitive.anchor_start_y = saved_anchor_start.y;
    primitive.anchor_start_z = saved_anchor_start.z;
    primitive.anchor_end_x = saved_anchor_end.x;
    primitive.anchor_end_y = saved_anchor_end.y;
    primitive.anchor_end_z = saved_anchor_end.z;
    primitive.dimension_start_x = saved_dimension_start.x;
    primitive.dimension_start_y = saved_dimension_start.y;
    primitive.dimension_start_z = saved_dimension_start.z;
    primitive.dimension_end_x = saved_dimension_end.x;
    primitive.dimension_end_y = saved_dimension_end.y;
    primitive.dimension_end_z = saved_dimension_end.z;
    primitive.label_x = saved_label.x;
    primitive.label_y = saved_label.y;
    primitive.label_z = saved_label.z;
    primitive.arc_radius = dimension_radius;
    primitive.ref_line_end_x = saved_dimension_start.x;
    primitive.ref_line_end_y = saved_dimension_start.y;
    primitive.ref_line_end_z = saved_dimension_start.z;
  }
  return primitive;
}

ViewportSketchDimensionPrimitive make_circle_center_distance_dimension_primitive(
    const SketchCircle& driven_circle,
    const SketchCircle& reference_circle,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint start =
      to_world_point(parameters, reference_circle.center_x, reference_circle.center_y);
  const WorldPoint end =
      to_world_point(parameters, driven_circle.center_x, driven_circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (reference_circle.center_x + driven_circle.center_x) / 2.0,
      (reference_circle.center_y + driven_circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_center_distance",
      .entity_id = driven_circle.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length > 1e-6) {
      const double normal_x = -dy / length;
      const double normal_y = dx / length;
      const double label_x =
          (reference_circle.center_x + driven_circle.center_x) / 2.0;
      const double label_y =
          (reference_circle.center_y + driven_circle.center_y) / 2.0;
      const double raw_offset_x = *dimension.label_x - label_x;
      const double raw_offset_y = *dimension.label_y - label_y;
      const double along = raw_offset_x * normal_x + raw_offset_y * normal_y;
      const double offset_x = normal_x * along;
      const double offset_y = normal_y * along;
      const WorldPoint shifted_start = to_world_point(
          parameters,
          reference_circle.center_x + offset_x,
          reference_circle.center_y + offset_y);
      const WorldPoint shifted_end = to_world_point(
          parameters,
          driven_circle.center_x + offset_x,
          driven_circle.center_y + offset_y);
      const WorldPoint shifted_label =
          to_world_point(parameters, label_x + offset_x, label_y + offset_y);
      primitive.dimension_start_x = shifted_start.x;
      primitive.dimension_start_y = shifted_start.y;
      primitive.dimension_start_z = shifted_start.z;
      primitive.dimension_end_x = shifted_end.x;
      primitive.dimension_end_y = shifted_end.y;
      primitive.dimension_end_z = shifted_end.z;
      primitive.label_x = shifted_label.x;
      primitive.label_y = shifted_label.y;
      primitive.label_z = shifted_label.z;
    }
  }
  return primitive;
}

ViewportSketchDimensionPrimitive make_circle_line_distance_dimension_primitive(
    const SketchCircle& circle,
    const SketchLine& line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double ux = length > 0.0 ? dx / length : 1.0;
  const double uy = length > 0.0 ? dy / length : 0.0;
  const double t =
      ((circle.center_x - line.start_x) * ux +
       (circle.center_y - line.start_y) * uy);
  const double foot_x = line.start_x + ux * t;
  const double foot_y = line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, circle.center_x, circle.center_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + circle.center_x) / 2.0,
      (foot_y + circle.center_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "circle_line_distance",
      .entity_id = circle.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double label_x = (foot_x + circle.center_x) / 2.0;
    const double label_y = (foot_y + circle.center_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end = to_world_point(
        parameters,
        circle.center_x + offset_x,
        circle.center_y + offset_y);
    const WorldPoint shifted_label =
        to_world_point(parameters, label_x + offset_x, label_y + offset_y);
    primitive.dimension_start_x = shifted_start.x;
    primitive.dimension_start_y = shifted_start.y;
    primitive.dimension_start_z = shifted_start.z;
    primitive.dimension_end_x = shifted_end.x;
    primitive.dimension_end_y = shifted_end.y;
    primitive.dimension_end_z = shifted_end.z;
    primitive.label_x = shifted_label.x;
    primitive.label_y = shifted_label.y;
    primitive.label_z = shifted_label.z;
  }
  return primitive;
}

ViewportSketchDimensionPrimitive make_line_line_distance_dimension_primitive(
    const SketchLine& driven_line,
    const SketchLine& reference_line,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_length = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_length <= 1e-6) {
    return ViewportSketchDimensionPrimitive{};
  }
  const double ux = ref_dx / ref_length;
  const double uy = ref_dy / ref_length;
  const double midpoint_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double midpoint_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  const double t =
      (midpoint_x - reference_line.start_x) * ux +
      (midpoint_y - reference_line.start_y) * uy;
  const double foot_x = reference_line.start_x + ux * t;
  const double foot_y = reference_line.start_y + uy * t;
  const WorldPoint start = to_world_point(parameters, foot_x, foot_y);
  const WorldPoint end = to_world_point(parameters, midpoint_x, midpoint_y);
  const WorldPoint label = to_world_point(
      parameters,
      (foot_x + midpoint_x) / 2.0,
      (foot_y + midpoint_y) / 2.0);
  auto primitive = ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "line_line_distance",
      .entity_id = driven_line.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    const double label_x = (foot_x + midpoint_x) / 2.0;
    const double label_y = (foot_y + midpoint_y) / 2.0;
    const double raw_offset_x = *dimension.label_x - label_x;
    const double raw_offset_y = *dimension.label_y - label_y;
    const double along = raw_offset_x * ux + raw_offset_y * uy;
    const double offset_x = ux * along;
    const double offset_y = uy * along;
    const WorldPoint shifted_start =
        to_world_point(parameters, foot_x + offset_x, foot_y + offset_y);
    const WorldPoint shifted_end =
        to_world_point(parameters, midpoint_x + offset_x, midpoint_y + offset_y);
    const WorldPoint shifted_label =
        to_world_point(parameters, label_x + offset_x, label_y + offset_y);
    primitive.dimension_start_x = shifted_start.x;
    primitive.dimension_start_y = shifted_start.y;
    primitive.dimension_start_z = shifted_start.z;
    primitive.dimension_end_x = shifted_end.x;
    primitive.dimension_end_y = shifted_end.y;
    primitive.dimension_end_z = shifted_end.z;
    primitive.label_x = shifted_label.x;
    primitive.label_y = shifted_label.y;
    primitive.label_z = shifted_label.z;
  }
  return primitive;
}

ViewportSketchDimensionPrimitive make_point_distance_dimension_primitive(
    const SketchPoint& point_a,
    const SketchPoint& point_b,
    const SketchDimension& dimension,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint start = to_world_point(parameters, point_a.x, point_a.y);
  const WorldPoint end = to_world_point(parameters, point_b.x, point_b.y);
  const WorldPoint label = to_world_point(
      parameters,
      (point_a.x + point_b.x) / 2.0,
      (point_a.y + point_b.y) / 2.0);
  return ViewportSketchDimensionPrimitive{
      .dimension_id = dimension.id,
      .plane_id = parameters.plane_id,
      .kind = "point_distance",
      .entity_id = point_a.id,
      .label = format_dimension_value(dimension.value) + " mm",
      .is_selected = is_selected,
      .anchor_start_x = start.x,
      .anchor_start_y = start.y,
      .anchor_start_z = start.z,
      .anchor_end_x = end.x,
      .anchor_end_y = end.y,
      .anchor_end_z = end.z,
      .dimension_start_x = start.x,
      .dimension_start_y = start.y,
      .dimension_start_z = start.z,
      .dimension_end_x = end.x,
      .dimension_end_y = end.y,
      .dimension_end_z = end.z,
      .label_x = label.x,
      .label_y = label.y,
      .label_z = label.z,
  };
}

ViewportSketchConstraintPrimitive make_line_constraint_primitive(
    const SketchLine& line,
    const std::string& plane_id,
    const std::string& kind,
    const std::string& label,
    bool is_selected,
    const std::optional<std::string>& related_entity_id = std::nullopt,
    double badge_offset_multiplier = 1.0) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  const double normal_x = length > 0.0 ? -dy / length : 0.0;
  const double normal_y = length > 0.0 ? dx / length : 1.0;
  const double offset = kConstraintBadgeOffset * badge_offset_multiplier;
  const WorldPoint position = to_world_point(
      plane_id,
      (line.start_x + line.end_x) / 2.0 + normal_x * offset,
      (line.start_y + line.end_y) / 2.0 + normal_y * offset);

  return ViewportSketchConstraintPrimitive{
      .constraint_id =
          (kind == "equal_length" || kind == "perpendicular") &&
                  related_entity_id.has_value()
              ? "constraint-" + kind + "-" + line.id + "-" + related_entity_id.value()
              : "constraint-" + kind + "-" + line.id,
      .plane_id = plane_id,
      .kind = kind,
      .entity_id = line.id,
      .related_entity_id = related_entity_id,
      .label = label,
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
  };
}

ViewportSketchConstraintPrimitive make_point_constraint_primitive(
    const SketchPoint& point,
    const SketchFeatureParameters& parameters,
    bool is_selected) {
  const WorldPoint position = to_world_point(
      parameters, point.x, point.y, kSketchPlaneOffset + kConstraintBadgeOffset);

  return ViewportSketchConstraintPrimitive{
      .constraint_id = "constraint-fixed-" + point.id,
      .plane_id = parameters.plane_id,
      .kind = "fixed",
      .entity_id = point.id,
      .related_entity_id = std::nullopt,
      .label = "FIX",
      .is_selected = is_selected,
      .position_x = position.x,
      .position_y = position.y,
      .position_z = position.z,
  };
}

ViewportSketchProfilePrimitive make_rectangle_profile_primitive(
    const PolygonSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "polygon",
      .profile_points = profile.points,
      .inner_loops = profile.inner_loops,
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .is_selected = is_selected,
  };
}

ViewportSketchProfilePrimitive make_circle_profile_primitive(
    const CircleSketchProfile& profile, bool is_selected) {
  return ViewportSketchProfilePrimitive{
      .profile_id = profile.id,
      .plane_id = profile.plane_id,
      .plane_frame = profile.plane_frame.has_value()
                         ? std::optional<ViewportSketchPlaneFrame>(
                               make_sketch_plane_frame(profile.plane_frame.value()))
                         : std::nullopt,
      .profile_kind = "circle",
      .profile_points = {},
      .inner_loops = {},
      .start_x = profile.center_x,
      .start_y = profile.center_y,
      .width = 0.0,
      .height = 0.0,
      .radius = profile.radius,
      .is_selected = is_selected,
  };
}

void include_world_point_for_plane(const std::string& plane_id,
                                   double local_x,
                                   double local_y,
                                   double depth,
                                   double& min_x,
                                   double& max_x,
                                   double& min_y,
                                   double& max_y,
                                   double& min_z,
                                   double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  if (plane_id == "ref-plane-xy") {
    include_point(local_x, 0.0, local_y);
    include_point(local_x, depth, local_y);
    return;
  }

  if (plane_id == "ref-plane-yz") {
    include_point(0.0, local_x, local_y);
    include_point(depth, local_x, local_y);
    return;
  }

  include_point(local_x, local_y, 0.0);
  include_point(local_x, local_y, depth);
}

void include_world_point_for_frame(
    const PlaneFrame& frame,
    double local_x,
    double local_y,
    double depth,
    double& min_x,
    double& max_x,
    double& min_y,
    double& max_y,
    double& min_z,
    double& max_z) {
  const auto include_point =
      [&](double world_x, double world_y, double world_z) {
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_y = std::min(min_y, world_y);
        max_y = std::max(max_y, world_y);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      };

  const auto to_world = [&](double offset) {
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  };

  const WorldPoint start = to_world(0.0);
  const WorldPoint end = to_world(depth);
  include_point(start.x, start.y, start.z);
  include_point(end.x, end.y, end.z);
}

// Sample a single OCCT edge into a flat polyline (x0, y0, z0, x1, ...).
// Straight segments stay 2-point; everything else uses
// GCPnts_QuasiUniformDeflection with a small fixed deflection so curves
// look smooth in the viewport without ballooning the wire payload.
void sample_edge(const TopoDS_Edge& edge,
                 std::vector<double>& points,
                 std::string& kind) {
  points.clear();
  kind = "curve";

  if (edge.IsNull()) {
    return;
  }

  // BRepAdaptor_Curve internally pulls the curve from the edge and
  // applies the edge's location, so the sampled points are already in
  // world space relative to the body.
  BRepAdaptor_Curve adaptor;
  try {
    adaptor.Initialize(edge);
  } catch (const std::exception&) {
    return;
  }

  const GeomAbs_CurveType curve_type = adaptor.GetType();
  if (curve_type == GeomAbs_Line) {
    kind = "line";
    const double first = adaptor.FirstParameter();
    const double last = adaptor.LastParameter();
    const gp_Pnt start = adaptor.Value(first);
    const gp_Pnt end = adaptor.Value(last);
    points.push_back(start.X());
    points.push_back(start.Y());
    points.push_back(start.Z());
    points.push_back(end.X());
    points.push_back(end.Y());
    points.push_back(end.Z());
    return;
  }

  if (curve_type == GeomAbs_Circle) {
    kind = "circle";
  }

  // Generic curve sampler. The deflection of 0.05mm matches the
  // body_compiler tessellation budget closely enough that selectable
  // edges hug the rendered solid surface.
  try {
    GCPnts_QuasiUniformDeflection sampler(adaptor, /*Deflection=*/0.05);
    if (!sampler.IsDone() || sampler.NbPoints() < 2) {
      // Fall back to the curve's parametric endpoints so the edge is
      // at least pickable as a chord.
      const double first = adaptor.FirstParameter();
      const double last = adaptor.LastParameter();
      const gp_Pnt start = adaptor.Value(first);
      const gp_Pnt end = adaptor.Value(last);
      points.push_back(start.X());
      points.push_back(start.Y());
      points.push_back(start.Z());
      points.push_back(end.X());
      points.push_back(end.Y());
      points.push_back(end.Z());
      return;
    }
    for (int i = 1; i <= sampler.NbPoints(); ++i) {
      const gp_Pnt p = sampler.Value(i);
      points.push_back(p.X());
      points.push_back(p.Y());
      points.push_back(p.Z());
    }
  } catch (const std::exception&) {
    points.clear();
  }
}

bool edge_curve_type_is(const TopoDS_Edge& edge, GeomAbs_CurveType type) {
  try {
    BRepAdaptor_Curve curve(edge);
    return curve.GetType() == type;
  } catch (const std::exception&) {
    return false;
  }
}

bool is_nonsemantic_seam_line_edge(const TopoDS_Shape& body_shape,
                                   const TopoDS_Edge& edge) {
  if (body_shape.IsNull() || edge.IsNull() ||
      !edge_curve_type_is(edge, GeomAbs_Line)) {
    return false;
  }
  for (TopExp_Explorer explorer(body_shape, TopAbs_FACE); explorer.More();
       explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(explorer.Current());
    if (face.IsNull()) {
      continue;
    }
    try {
      if (BRep_Tool::IsClosed(edge, face)) {
        return true;
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  return false;
}

bool is_full_circle_edge(const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle) {
      return false;
    }
    const double span = std::abs(curve.LastParameter() - curve.FirstParameter());
    if (span >= 2.0 * kPi - 1e-4) {
      return true;
    }
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last, /*CumOri=*/false);
    if (first.IsNull() || last.IsNull()) {
      return false;
    }
    return BRep_Tool::Pnt(first).Distance(BRep_Tool::Pnt(last)) <= 1e-5;
  } catch (const std::exception&) {
    return false;
  }
}

bool edge_contains_vertex(const TopoDS_Edge& edge, const TopoDS_Vertex& vertex) {
  TopoDS_Vertex first;
  TopoDS_Vertex last;
  TopExp::Vertices(edge, first, last, /*CumOri=*/false);
  return (!first.IsNull() && first.IsSame(vertex)) ||
         (!last.IsNull() && last.IsSame(vertex));
}

bool is_nonsemantic_circle_seam_vertex(const TopoDS_Shape& body_shape,
                                       const TopoDS_Vertex& vertex) {
  if (body_shape.IsNull() || vertex.IsNull()) {
    return false;
  }
  TopTools_IndexedMapOfShape edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
  bool found_incident_edge = false;
  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (edge.IsNull() || !edge_contains_vertex(edge, vertex)) {
      continue;
    }
    found_incident_edge = true;
    if (!is_nonsemantic_seam_line_edge(body_shape, edge) &&
        !is_full_circle_edge(edge)) {
      return false;
    }
  }
  return found_incident_edge;
}

// Append every unique edge of `body_shape` to `out`, owned by `body_id`.
// Edge ids match the format expected by DocumentManager::select_edge.
// `selected_edge_ids` carries the multi-edge selection set; an edge is
// flagged selected iff its id appears anywhere in the set. We accept a
// vector rather than a hash set because edge selections are O(N <= ~20)
// in practice — the linear scan is cheaper than building a hash set
// per body.
void enumerate_body_edges(const TopoDS_Shape& body_shape,
                          const std::string& body_id,
                          const std::vector<std::string>& selected_edge_ids,
                          std::vector<ViewportEdgePrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  TopTools_IndexedMapOfShape edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge edge = TopoDS::Edge(edge_map(i));
    if (is_nonsemantic_seam_line_edge(body_shape, edge)) {
      continue;
    }
    ViewportEdgePrimitive primitive{};
    primitive.id = body_id + ":edge:" + std::to_string(i - 1);
    primitive.owner_body_id = body_id;
    sample_edge(edge, primitive.points, primitive.kind);
    if (primitive.points.size() < 6) {
      // Degenerate edges (fewer than 2 sample points) cannot be picked
      // or rendered; skip them rather than emit dead entries.
      continue;
    }
    // Exact length via OCCT mass-property integration. We swallow
    // failures here (defaulting to 0) because a rare degenerate edge
    // shouldn't take the whole snapshot down — the UI just shows
    // 0 mm for that edge.
    try {
      GProp_GProps props;
      BRepGProp::LinearProperties(edge, props);
      primitive.length = props.Mass();
    } catch (const std::exception&) {
      primitive.length = 0.0;
    }
    primitive.is_selected =
        std::find(selected_edge_ids.begin(),
                  selected_edge_ids.end(),
                  primitive.id) != selected_edge_ids.end();
    out.push_back(std::move(primitive));
  }
}

// Append every unique vertex of `body_shape` to `out`, owned by `body_id`.
// Vertex ids match the format expected by DocumentManager::select_vertex.
void enumerate_body_vertices(
    const TopoDS_Shape& body_shape,
    const std::string& body_id,
    const std::vector<std::string>& selected_vertex_ids,
    std::vector<ViewportVertexPrimitive>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  TopTools_IndexedMapOfShape vertex_map;
  TopExp::MapShapes(body_shape, TopAbs_VERTEX, vertex_map);

  for (int i = 1; i <= vertex_map.Extent(); ++i) {
    const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_map(i));
    if (vertex.IsNull()) {
      continue;
    }
    if (is_nonsemantic_circle_seam_vertex(body_shape, vertex)) {
      continue;
    }
    gp_Pnt position;
    try {
      position = BRep_Tool::Pnt(vertex);
    } catch (const std::exception&) {
      continue;
    }
    ViewportVertexPrimitive primitive{};
    primitive.id = body_id + ":vertex:" + std::to_string(i - 1);
    primitive.owner_body_id = body_id;
    primitive.x = position.X();
    primitive.y = position.Y();
    primitive.z = position.Z();
    primitive.is_selected =
        std::find(selected_vertex_ids.begin(),
                  selected_vertex_ids.end(),
                  primitive.id) != selected_vertex_ids.end();
    out.push_back(std::move(primitive));
  }
}

// Build a plane frame for `face` by reading the underlying surface's plane
// (when planar) and the face center. For non-planar faces, return a
// representative frame oriented around the face center + face normal at
// that point — picking still works, sketch-on-face is rejected via the
// returned `is_planar` flag.
struct FaceFrameInfo {
  ViewportSolidFace::PlaneFrame frame;
  double center_x;
  double center_y;
  double center_z;
  double normal_x;
  double normal_y;
  double normal_z;
  bool is_planar;
};

FaceFrameInfo derive_face_frame(const TopoDS_Face& face) {
  FaceFrameInfo info{};
  info.is_planar = false;
  // Sensible defaults; these are overwritten below.
  info.frame = ViewportSolidFace::PlaneFrame{
      .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
      .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
      .y_axis_x = 0.0, .y_axis_y = 1.0, .y_axis_z = 0.0,
      .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
  };

  try {
    BRepAdaptor_Surface surface(face);
    const GeomAbs_SurfaceType type = surface.GetType();

    // Compute the face's parametric mid-point so we can sample a
    // representative (point, normal) regardless of surface type.
    const double u_mid = 0.5 * (surface.FirstUParameter() + surface.LastUParameter());
    const double v_mid = 0.5 * (surface.FirstVParameter() + surface.LastVParameter());

    BRepGProp_Face prop(face);
    gp_Pnt center;
    gp_Vec normal;
    prop.Normal(u_mid, v_mid, center, normal);
    if (normal.Magnitude() > 0.0) {
      normal.Normalize();
    } else {
      normal = gp_Vec(0.0, 0.0, 1.0);
    }
    // Honor the face's orientation flag so the emitted normal is the
    // outward-pointing one consumers expect.
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }

    info.center_x = center.X();
    info.center_y = center.Y();
    info.center_z = center.Z();
    info.normal_x = normal.X();
    info.normal_y = normal.Y();
    info.normal_z = normal.Z();

    if (type == GeomAbs_Plane) {
      info.is_planar = true;
      const gp_Pln plane = surface.Plane();
      const gp_Ax3 ax = plane.Position();
      const gp_Pnt origin = ax.Location();
      gp_Dir x_axis = ax.XDirection();
      gp_Dir y_axis = ax.YDirection();
      gp_Dir z_axis = ax.Direction();
      // Mirror the orientation flag onto the plane axes too — a reversed
      // face has its "outward" normal flipped, and the y-axis is flipped
      // to keep the frame right-handed.
      if (face.Orientation() == TopAbs_REVERSED) {
        z_axis.Reverse();
        y_axis.Reverse();
      }
      info.frame = ViewportSolidFace::PlaneFrame{
          .origin_x = origin.X(),
          .origin_y = origin.Y(),
          .origin_z = origin.Z(),
          .x_axis_x = x_axis.X(),
          .x_axis_y = x_axis.Y(),
          .x_axis_z = x_axis.Z(),
          .y_axis_x = y_axis.X(),
          .y_axis_y = y_axis.Y(),
          .y_axis_z = y_axis.Z(),
          .normal_x = z_axis.X(),
          .normal_y = z_axis.Y(),
          .normal_z = z_axis.Z(),
      };
      // Override center/normal with the plane axes for consistency.
      info.normal_x = z_axis.X();
      info.normal_y = z_axis.Y();
      info.normal_z = z_axis.Z();
    } else {
      // Non-planar face: synthesize a frame at the sampled center with
      // the sampled normal so the UI can still place a label / preview
      // marker. Sketch-on-face is rejected via `is_planar = false`.
      gp_Vec arbitrary = std::abs(normal.X()) < 0.9
                             ? gp_Vec(1.0, 0.0, 0.0)
                             : gp_Vec(0.0, 1.0, 0.0);
      gp_Vec x_axis = arbitrary.Crossed(normal);
      if (x_axis.Magnitude() > 0.0) {
        x_axis.Normalize();
      }
      gp_Vec y_axis = normal.Crossed(x_axis);
      if (y_axis.Magnitude() > 0.0) {
        y_axis.Normalize();
      }
      info.frame = ViewportSolidFace::PlaneFrame{
          .origin_x = center.X(),
          .origin_y = center.Y(),
          .origin_z = center.Z(),
          .x_axis_x = x_axis.X(),
          .x_axis_y = x_axis.Y(),
          .x_axis_z = x_axis.Z(),
          .y_axis_x = y_axis.X(),
          .y_axis_y = y_axis.Y(),
          .y_axis_z = y_axis.Z(),
          .normal_x = normal.X(),
          .normal_y = normal.Y(),
          .normal_z = normal.Z(),
      };
    }
  } catch (const std::exception&) {
    // Bad surface: leave defaults, mark non-planar.
    info.is_planar = false;
  }

  return info;
}

// Append every unique face of `body_shape` to `out`, owned by `body_id`.
// Each face is fully tessellated in world space so the UI can build a
// real BufferGeometry and the picker hits the actual face geometry.
// Face ids match the format expected by DocumentManager::select_face
// ("<owner_id>:face:<index>"), with index coming from
// TopExp::MapShapes(TopAbs_FACE).
void enumerate_body_faces(const TopoDS_Shape& body_shape,
                           const std::string& body_id,
                           const std::string& body_kind,
                           const DocumentState& document,
                           const std::optional<std::string>& selected_face_id,
                           std::vector<ViewportSolidFace>& out) {
  if (body_shape.IsNull()) {
    return;
  }

  // BRepMesh_IncrementalMesh deflection mirrors body_compiler so the
  // face triangulation matches the body mesh that's already on screen.
  // Skipping this would leave faces without per-face triangulation when
  // body_compiler hasn't tessellated this body (legacy non-boolean path).
  try {
    BRepMesh_IncrementalMesh mesher(body_shape,
                                    /*deflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/true);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  TopTools_IndexedMapOfShape face_map;
  TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);

  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    if (face.IsNull()) {
      continue;
    }

    const std::string face_id =
        body_id + ":face:" + std::to_string(i - 1);

    TopLoc_Location location;
    const Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      // No tessellation for this face — skip rather than emit a face
      // that can't be picked or rendered.
      continue;
    }

    const FaceFrameInfo frame_info = derive_face_frame(face);

    ViewportSolidFace primitive{};
    primitive.face_id = face_id;
    primitive.owner_id = body_id;
    primitive.owner_kind = body_kind;
    primitive.label = "Face " + std::to_string(i);
    primitive.sketchability = frame_info.is_planar ? "planar" : "non-planar";
    primitive.center_x = frame_info.center_x;
    primitive.center_y = frame_info.center_y;
    primitive.center_z = frame_info.center_z;
    primitive.normal_x = frame_info.normal_x;
    primitive.normal_y = frame_info.normal_y;
    primitive.normal_z = frame_info.normal_z;
    primitive.plane_frame = frame_info.frame;
    primitive.is_selected = selected_face_id.has_value() &&
                            selected_face_id.value() == face_id;
    primitive.appearance_color =
        face_appearance_color(document, face_id, appearance_face_signature(face));

    // Append per-face triangulation (positions in world space, indices
    // local to this face's positions array).
    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int node_count = triangulation->NbNodes();
    primitive.triangle_positions.reserve(static_cast<size_t>(node_count) * 3);
    for (int node_index = 1; node_index <= node_count; ++node_index) {
      gp_Pnt node = triangulation->Node(node_index);
      node.Transform(transform);
      primitive.triangle_positions.push_back(node.X());
      primitive.triangle_positions.push_back(node.Y());
      primitive.triangle_positions.push_back(node.Z());
    }

    const int triangle_count = triangulation->NbTriangles();
    primitive.triangle_indices.reserve(static_cast<size_t>(triangle_count) * 3);
    for (int tri_index = 1; tri_index <= triangle_count; ++tri_index) {
      const Poly_Triangle& triangle = triangulation->Triangle(tri_index);
      int n1 = 0;
      int n2 = 0;
      int n3 = 0;
      triangle.Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }
      primitive.triangle_indices.push_back(n1 - 1);
      primitive.triangle_indices.push_back(n2 - 1);
      primitive.triangle_indices.push_back(n3 - 1);
    }

    if (primitive.triangle_positions.empty() ||
        primitive.triangle_indices.empty()) {
      continue;
    }

    out.push_back(std::move(primitive));
  }
}

// Tessellate `shape` into flat positions/normals/indices arrays. Used
// for the cut-preview emission below, which renders a translucent
// "this is what's about to be cut" volume so the user can see exactly
// which material the cut extrude will remove.
//
// The output is intentionally minimal compared to body_compiler's
// tessellate_shape: per-vertex normals are computed in a single pass
// (one normal per vertex, summed across triangles) without seam-
// splitting. The cut preview is a UI overlay so visual fidelity is
// less important than getting it on screen at all.
void tessellate_shape_to_arrays(const TopoDS_Shape& shape,
                                std::vector<double>& positions,
                                std::vector<double>& normals,
                                std::vector<int>& indices) {
  if (shape.IsNull()) {
    return;
  }
  try {
    BRepMesh_IncrementalMesh mesher(shape,
                                    /*deflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/true);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  for (TopExp_Explorer face_explorer(shape, TopAbs_FACE);
       face_explorer.More();
       face_explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(face_explorer.Current());
    TopLoc_Location location;
    const Handle(Poly_Triangulation) triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }
    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int base_index = static_cast<int>(positions.size() / 3);
    const int node_count = triangulation->NbNodes();
    for (int i = 1; i <= node_count; ++i) {
      gp_Pnt node = triangulation->Node(i);
      node.Transform(transform);
      positions.push_back(node.X());
      positions.push_back(node.Y());
      positions.push_back(node.Z());
      normals.push_back(0.0);
      normals.push_back(0.0);
      normals.push_back(0.0);
    }
    const int triangle_count = triangulation->NbTriangles();
    for (int t = 1; t <= triangle_count; ++t) {
      const Poly_Triangle& triangle = triangulation->Triangle(t);
      int n1 = 0;
      int n2 = 0;
      int n3 = 0;
      triangle.Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }
      const int i1 = base_index + (n1 - 1);
      const int i2 = base_index + (n2 - 1);
      const int i3 = base_index + (n3 - 1);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i3);
      // Accumulate per-triangle normal onto each vertex so the simple
      // viewer flat-shading pass has something to work with.
      const double ax = positions[3 * i1 + 0];
      const double ay = positions[3 * i1 + 1];
      const double az = positions[3 * i1 + 2];
      const double bx = positions[3 * i2 + 0];
      const double by = positions[3 * i2 + 1];
      const double bz = positions[3 * i2 + 2];
      const double cx = positions[3 * i3 + 0];
      const double cy = positions[3 * i3 + 1];
      const double cz = positions[3 * i3 + 2];
      const double ux = bx - ax;
      const double uy = by - ay;
      const double uz = bz - az;
      const double vx = cx - ax;
      const double vy = cy - ay;
      const double vz = cz - az;
      const double nx = uy * vz - uz * vy;
      const double ny = uz * vx - ux * vz;
      const double nz = ux * vy - uy * vx;
      const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
      const double inv = length > 0.0 ? 1.0 / length : 0.0;
      const double nxn = nx * inv;
      const double nyn = ny * inv;
      const double nzn = nz * inv;
      for (int idx : {i1, i2, i3}) {
        normals[3 * idx + 0] += nxn;
        normals[3 * idx + 1] += nyn;
        normals[3 * idx + 2] += nzn;
      }
    }
  }
}

bool has_feature_id(const DocumentState& document, const std::string& id) {
  return std::any_of(document.feature_history.begin(),
                     document.feature_history.end(),
                     [&](const FeatureEntry& feature) {
                       return feature.id == id;
                     });
}

DocumentState document_at_timeline_cursor(const DocumentState& source) {
  if (!source.timeline_cursor.has_value()) {
    return source;
  }

  DocumentState snapshot = source;
  snapshot.feature_history.clear();

  int remaining_actions = std::max(0, source.timeline_cursor.value());
  for (const auto& feature : source.feature_history) {
    if (feature.kind == "root_part") {
      snapshot.feature_history.push_back(feature);
      continue;
    }
    if (remaining_actions <= 0) {
      continue;
    }
    snapshot.feature_history.push_back(feature);
    --remaining_actions;
  }

  if (snapshot.selected_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.selected_feature_id.value())) {
    snapshot.selected_feature_id = std::nullopt;
  }
  if (snapshot.selected_reference_id.has_value() &&
      snapshot.selected_reference_id->rfind("ref-plane-", 0) != 0 &&
      !has_feature_id(snapshot, snapshot.selected_reference_id.value())) {
    snapshot.selected_reference_id = std::nullopt;
  }
  if (snapshot.active_sketch_feature_id.has_value() &&
      !has_feature_id(snapshot, snapshot.active_sketch_feature_id.value())) {
    snapshot.active_sketch_feature_id = std::nullopt;
    snapshot.active_sketch_plane_id = std::nullopt;
    snapshot.active_sketch_face_id = std::nullopt;
    snapshot.active_sketch_tool = std::nullopt;
    snapshot.selected_sketch_point_id = std::nullopt;
    snapshot.selected_sketch_entity_id = std::nullopt;
    snapshot.selected_sketch_dimension_id = std::nullopt;
    snapshot.selected_sketch_profile_id = std::nullopt;
    snapshot.selected_sketch_point_ids.clear();
    snapshot.selected_sketch_entity_ids.clear();
    snapshot.selected_sketch_profile_ids.clear();
  }
  snapshot.selected_face_id = std::nullopt;
  snapshot.selected_edge_ids.clear();
  snapshot.selected_vertex_ids.clear();

  return snapshot;
}

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
  if (!document.has_value()) {
    return ViewportState{
        .has_active_document = false,
        .boxes = {},
        .cylinders = {},
        .polygon_extrudes = {},
        .solid_faces = {},
        .reference_planes = {},
        .reference_axes = {},
        .reference_points = {},
        .helices = {},
        .sketch_lines = {},
        .sketch_circles = {},
        .sketch_arcs = {},
        .sketch_points = {},
        .sketch_dimensions = {},
        .sketch_constraints = {},
        .sketch_profiles = {},
        .dof_statuses = {},
        .meshes = {},
        .cut_previews = {},
        .bodies = {},
        .edges = {},
        .vertices = {},
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
        .selection_filter = SelectionFilter{},
    };
  }

  const DocumentState viewport_document =
      document_at_timeline_cursor(document.value());
  const DocumentState* view = &viewport_document;

  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportPolygonExtrudePrimitive> polygon_extrudes;
  std::vector<ViewportSolidFace> solid_faces;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportReferencePoint> reference_points;
  std::vector<ViewportHelixPrimitive> helices;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchPointPrimitive> sketch_points;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  std::vector<ViewportMeshPrimitive> meshes;
  std::vector<ViewportCutPreview> cut_previews;
  std::vector<ViewportBodySummary> bodies;
  std::vector<ViewportEdgePrimitive> edges;
  std::vector<ViewportVertexPrimitive> vertices;
  std::vector<EntityDofResult> dof_statuses;
  double current_x_offset = 0.0;
  double scene_width = 0.0;
  double max_height = 0.0;
  double max_depth = 0.0;

  // Walk the feature history once with boolean operators applied so we
  // know which features get consumed by Fuse/Cut and which bodies need
  // to be tessellated as mesh primitives. Features in the resulting
  // `consumed_feature_ids` set must be skipped by the legacy primitive
  // emission below to avoid double-rendering. Failures in OCCT booleans
  // produce empty meshes — see body_compiler.cpp — so legacy fallback
  // still renders something.
  CompiledBodies compiled_bodies = compile_bodies(*view);
  std::optional<std::string> selected_move_target_body_id;
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id == view->selected_feature_id.value() &&
          feature.kind == "move" &&
          feature.move_parameters.has_value()) {
        selected_move_target_body_id = feature.move_parameters->target_body_id;
        break;
      }
    }
  }
  for (const auto& body_mesh : compiled_bodies.meshes) {
    ViewportMeshPrimitive mesh{};
    mesh.id = body_mesh.body_id;
    mesh.positions = body_mesh.vertices;
    mesh.normals = body_mesh.normals;
    mesh.indices = body_mesh.indices;
    mesh.is_selected =
        view->selected_feature_id.has_value() &&
        (view->selected_feature_id.value() == body_mesh.body_id ||
         (selected_move_target_body_id.has_value() &&
          selected_move_target_body_id.value() == body_mesh.body_id));
    mesh.appearance_color = body_appearance_color(*view, body_mesh.body_id);
    meshes.push_back(std::move(mesh));
  }
  const std::set<std::string>& consumed = compiled_bodies.consumed_feature_ids;

  // Cut preview overlay: when the user has a cut extrude selected (i.e.
  // the floating Extrude panel is open and editing it), emit a
  // translucent red mesh of the cutter volume so they can see exactly
  // what's about to be removed. This is a UI overlay only — the
  // booleaned body itself already renders the post-cut shape via
  // `meshes`. We only emit the preview while the feature is the
  // currently-selected one to avoid clutter on saved documents.
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id != view->selected_feature_id.value()) {
        continue;
      }
      if (feature.kind != "extrude" ||
          !feature.extrude_parameters.has_value() ||
          feature.extrude_parameters->mode != "cut") {
        break;
      }
      try {
        const TopoDS_Shape cutter =
            build_extrude_shape(feature.extrude_parameters.value());
        if (cutter.IsNull()) {
          break;
        }
        ViewportCutPreview preview{};
        preview.id = feature.id;
        tessellate_shape_to_arrays(cutter,
                                   preview.positions,
                                   preview.normals,
                                   preview.indices);
        if (!preview.positions.empty() && !preview.indices.empty()) {
          cut_previews.push_back(std::move(preview));
        }
      } catch (const std::exception&) {
        // Cutter build failures shouldn't break the rest of the
        // viewport; just skip the preview for this snapshot.
      }
      break;
    }
  }

  // Build the body summary list for the UI's target picker. The body's
  // root id maps 1:1 to a feature id, so we look up the human-readable
  // name from feature_history; missing or empty names degrade to the id
  // itself so the picker is always populated.
  for (const auto& body : compiled_bodies.bodies) {
    std::string label = body.id;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id && !feature.name.empty()) {
        label = feature.name;
        break;
      }
    }
    const BodyBounds bounds = bounds_for_shape(body.shape);
    bodies.push_back(ViewportBodySummary{
        .id = body.id,
        .label = label,
        .center_x = bounds.center_x,
        .center_y = bounds.center_y,
        .center_z = bounds.center_z,
        .width = bounds.width,
        .height = bounds.height,
        .depth = bounds.depth,
        .local_frame = body.local_frame,
    });
    // Edge picking uses pick_shape when the body has one (set by
    // body_compiler for the duration of a pending fillet/chamfer
    // panel session). This keeps edge ids stable while the user
    // toggles edges, even though body.shape is mutating with each
    // update_*_edges. Vertices and faces still come from the live
    // post-op shape because vertex / face picks aren't part of the
    // pending feature's input set.
    const TopoDS_Shape& edge_pick_shape =
        body.pick_shape.IsNull() ? body.shape : body.pick_shape;
    enumerate_body_edges(edge_pick_shape,
                         body.id,
                         view->selected_edge_ids,
                         edges);
    enumerate_body_vertices(body.shape,
                            body.id,
                            view->selected_vertex_ids,
                            vertices);
    // Look up the body's owning feature kind so the face's owner_kind
    // stays useful to consumers (the UI uses it to label faces).
    std::string body_kind;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id) {
        body_kind = feature.kind;
        break;
      }
    }
    // Legacy box/cylinder features render at a `current_x_offset` that
    // body_compiler doesn't know about (their shapes are built at the
    // origin). Body-derived faces would therefore land away from the
    // visual primitive — keep their analytical faces (emitted below in
    // the per-feature loop) and skip body-derived faces for them.
    if (body_kind != "box" && body_kind != "cylinder") {
      enumerate_body_faces(body.shape,
                           body.id,
                           body_kind,
                           *view,
                           view->selected_face_id,
                           solid_faces);
    }
  }

  for (const auto& feature : view->feature_history) {
    // Suppressed features are excluded from every viewport-visible
    // emission path: legacy primitives, sketch overlays, body-derived
    // faces (already excluded via body_compiler skipping them). The UI
    // still shows the feature in the timeline / hierarchy, just dimmed.
    if (feature.suppressed) {
      continue;
    }
    const bool is_selected =
        view->selected_feature_id.has_value() &&
        view->selected_feature_id.value() == feature.id;
    const bool feature_consumed_by_boolean =
        consumed.find(feature.id) != consumed.end();

    if (feature.kind == "box" && feature.box_parameters.has_value()) {
      if (feature_consumed_by_boolean) {
        continue;
      }
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
          .appearance_color = body_appearance_color(*view, feature.id),
      });
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "top",
          "Top",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height,
          parameters.depth / 2.0,
          0.0,
          1.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height,
                           parameters.depth / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           1.0,
                           0.0),
          parameters.width,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:top"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "bottom",
          "Bottom",
          "planar",
          current_x_offset + parameters.width / 2.0,
          0.0,
          parameters.depth / 2.0,
          0.0,
          -1.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           0.0,
                           parameters.depth / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           -1.0,
                           0.0),
          parameters.width,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:bottom"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "front",
          "Front",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height / 2.0,
          parameters.depth,
          0.0,
          0.0,
          1.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height / 2.0,
                           parameters.depth,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0),
          parameters.width,
          parameters.height,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:front"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "back",
          "Back",
          "planar",
          current_x_offset + parameters.width / 2.0,
          parameters.height / 2.0,
          0.0,
          0.0,
          0.0,
          -1.0,
          make_plane_frame(current_x_offset + parameters.width / 2.0,
                           parameters.height / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           -1.0),
          parameters.width,
          parameters.height,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:back"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "right",
          "Right",
          "planar",
          current_x_offset + parameters.width,
          parameters.height / 2.0,
          parameters.depth / 2.0,
          1.0,
          0.0,
          0.0,
          make_plane_frame(current_x_offset + parameters.width,
                           parameters.height / 2.0,
                           parameters.depth / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           1.0,
                           0.0,
                           0.0),
          parameters.height,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:right"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "left",
          "Left",
          "planar",
          current_x_offset,
          parameters.height / 2.0,
          parameters.depth / 2.0,
          -1.0,
          0.0,
          0.0,
          make_plane_frame(current_x_offset,
                           parameters.height / 2.0,
                           parameters.depth / 2.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           -1.0,
                           0.0,
                           0.0),
          parameters.height,
          parameters.depth,
          0.0,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:left"));

      current_x_offset += parameters.width + kBoxSpacing;
      scene_width = std::max(scene_width, current_x_offset - kBoxSpacing);
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (parameters.depth > max_depth) {
        max_depth = parameters.depth;
      }
      continue;
    }

    if (feature.kind == "cylinder" && feature.cylinder_parameters.has_value()) {
      if (feature_consumed_by_boolean) {
        continue;
      }
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
          .appearance_color = body_appearance_color(*view, feature.id),
      });
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "top",
          "Top",
          "planar",
          current_x_offset + diameter / 2.0,
          parameters.height,
          diameter / 2.0,
          0.0,
          1.0,
          0.0,
          make_plane_frame(current_x_offset + diameter / 2.0,
                           parameters.height,
                           diameter / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           1.0,
                           0.0),
          diameter,
          diameter,
          parameters.radius,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:top"));
      solid_faces.push_back(make_solid_face(
          feature.id,
          feature.kind,
          "bottom",
          "Bottom",
          "planar",
          current_x_offset + diameter / 2.0,
          0.0,
          diameter / 2.0,
          0.0,
          -1.0,
          0.0,
          make_plane_frame(current_x_offset + diameter / 2.0,
                           0.0,
                           diameter / 2.0,
                           1.0,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           1.0,
                           0.0,
                           -1.0,
                           0.0),
          diameter,
          diameter,
          parameters.radius,
          view->selected_face_id.has_value() &&
              view->selected_face_id.value() == feature.id + ":face:bottom"));

      current_x_offset += diameter + kBoxSpacing;
      scene_width = std::max(scene_width, current_x_offset - kBoxSpacing);
      if (parameters.height > max_height) {
        max_height = parameters.height;
      }
      if (diameter > max_depth) {
        max_depth = diameter;
      }
      continue;
    }

    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      if (feature.dependency_broken || feature.status == "warning") {
        continue;
      }
      if (feature_consumed_by_boolean) {
        continue;
      }
      const auto& parameters = feature.extrude_parameters.value();

      if (parameters.profile_kind == "rectangle") {
        double face_center_x = 0.0;
        double face_center_y = 0.0;
        double face_center_z = 0.0;
        double face_normal_x = 0.0;
        double face_normal_y = 0.0;
        double face_normal_z = 0.0;
        double face_width = parameters.width;
        double face_height = parameters.height;
        double face_origin_x = 0.0;
        double face_origin_y = 0.0;
        double face_origin_z = 0.0;

        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          polygon_extrudes.push_back(ViewportPolygonExtrudePrimitive{
              .id = feature.id,
              .label = feature.name,
              .plane_id = parameters.plane_id,
              .plane_frame = std::optional<ViewportSketchPlaneFrame>(
                  make_sketch_plane_frame(frame)),
              .profile_points = {
                  SketchProfilePoint{.x = parameters.start_x, .y = parameters.start_y},
                  SketchProfilePoint{.x = parameters.start_x + parameters.width,
                                     .y = parameters.start_y},
                  SketchProfilePoint{.x = parameters.start_x + parameters.width,
                                     .y = parameters.start_y + parameters.height},
                  SketchProfilePoint{.x = parameters.start_x,
                                     .y = parameters.start_y + parameters.height},
              },
              .inner_loops = {},
              .depth = parameters.depth,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });

          const auto top_center = to_world_point(
              frame,
              parameters.start_x + parameters.width / 2.0,
              parameters.start_y + parameters.height / 2.0,
              parameters.depth);
          const auto base_center = to_world_point(
              frame,
              parameters.start_x + parameters.width / 2.0,
              parameters.start_y + parameters.height / 2.0,
              0.0);

          face_center_x = top_center.x;
          face_center_y = top_center.y;
          face_center_z = top_center.z;
          face_normal_x = frame.normal_x;
          face_normal_y = frame.normal_y;
          face_normal_z = frame.normal_z;
          face_origin_x = top_center.x;
          face_origin_y = top_center.y;
          face_origin_z = top_center.z;

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "base",
              "Base",
              "planar",
              base_center.x,
              base_center.y,
              base_center.z,
              -frame.normal_x,
              -frame.normal_y,
              -frame.normal_z,
              make_plane_frame(base_center.x,
                               base_center.y,
                               base_center.z,
                               frame.x_axis_x,
                               frame.x_axis_y,
                               frame.x_axis_z,
                               frame.y_axis_x,
                               frame.y_axis_y,
                               frame.y_axis_z,
                               -frame.normal_x,
                               -frame.normal_y,
                               -frame.normal_z),
              face_width,
              face_height,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:base"));
        } else if (parameters.plane_id == "ref-plane-xy") {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.width,
              .height = parameters.depth,
              .depth = parameters.height,
              .x_offset = parameters.start_x,
              .center_x = parameters.start_x + parameters.width / 2.0,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y + parameters.height / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.start_x + parameters.width / 2.0;
          face_center_y = parameters.depth;
          face_center_z = parameters.start_y + parameters.height / 2.0;
          face_normal_y = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.height);
          scene_width = std::max(scene_width, parameters.start_x + parameters.width);
        } else if (parameters.plane_id == "ref-plane-yz") {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.depth,
              .height = parameters.width,
              .depth = parameters.height,
              .x_offset = 0.0,
              .center_x = parameters.depth / 2.0,
              .center_y = parameters.start_x + parameters.width / 2.0,
              .center_z = parameters.start_y + parameters.height / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.depth;
          face_center_y = parameters.start_x + parameters.width / 2.0;
          face_center_z = parameters.start_y + parameters.height / 2.0;
          face_normal_x = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.start_x + parameters.width);
          max_depth = std::max(max_depth, parameters.start_y + parameters.height);
          scene_width = std::max(scene_width, parameters.depth);
        } else {
          boxes.push_back(ViewportBoxPrimitive{
              .id = feature.id,
              .label = feature.name,
              .width = parameters.width,
              .height = parameters.height,
              .depth = parameters.depth,
              .x_offset = parameters.start_x,
              .center_x = parameters.start_x + parameters.width / 2.0,
              .center_y = parameters.start_y + parameters.height / 2.0,
              .center_z = parameters.depth / 2.0,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          face_center_x = parameters.start_x + parameters.width / 2.0;
          face_center_y = parameters.start_y + parameters.height / 2.0;
          face_center_z = parameters.depth;
          face_normal_z = 1.0;
          face_origin_x = face_center_x;
          face_origin_y = face_center_y;
          face_origin_z = face_center_z;
          max_height = std::max(max_height, parameters.start_y + parameters.height);
          max_depth = std::max(max_depth, parameters.depth);
          scene_width = std::max(scene_width, parameters.start_x + parameters.width);
        }
        solid_faces.push_back(make_solid_face(
            feature.id,
            feature.kind,
            "top",
            "Top",
            "planar",
            face_center_x,
            face_center_y,
            face_center_z,
            face_normal_x,
            face_normal_y,
            face_normal_z,
            parameters.plane_frame.has_value()
                ? make_plane_frame(face_origin_x,
                                   face_origin_y,
                                   face_origin_z,
                                   parameters.plane_frame->x_axis_x,
                                   parameters.plane_frame->x_axis_y,
                                   parameters.plane_frame->x_axis_z,
                                   parameters.plane_frame->y_axis_x,
                                   parameters.plane_frame->y_axis_y,
                                   parameters.plane_frame->y_axis_z,
                                   parameters.plane_frame->normal_x,
                                   parameters.plane_frame->normal_y,
                                   parameters.plane_frame->normal_z)
                : make_face_frame_for_plane(
                      parameters.plane_id, face_origin_x, face_origin_y, face_origin_z),
            face_width,
            face_height,
            0.0,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));

        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          const auto make_side_frame = [&](double origin_x,
                                           double origin_y,
                                           double origin_z,
                                           double normal_x,
                                           double normal_y,
                                           double normal_z,
                                           bool along_x_axis) {
            return make_plane_frame(origin_x,
                                    origin_y,
                                    origin_z,
                                    along_x_axis ? frame.y_axis_x : frame.x_axis_x,
                                    along_x_axis ? frame.y_axis_y : frame.x_axis_y,
                                    along_x_axis ? frame.y_axis_z : frame.x_axis_z,
                                    frame.normal_x,
                                    frame.normal_y,
                                    frame.normal_z,
                                    normal_x,
                                    normal_y,
                                    normal_z);
          };

          const auto left_center = to_world_point(frame, 0.0, parameters.height / 2.0, parameters.depth / 2.0);
          const auto right_center = to_world_point(frame, parameters.width, parameters.height / 2.0, parameters.depth / 2.0);
          const auto front_center = to_world_point(frame, parameters.width / 2.0, 0.0, parameters.depth / 2.0);
          const auto back_center = to_world_point(frame, parameters.width / 2.0, parameters.height, parameters.depth / 2.0);

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "left",
              "Left",
              "planar",
              left_center.x,
              left_center.y,
              left_center.z,
              -frame.x_axis_x,
              -frame.x_axis_y,
              -frame.x_axis_z,
              make_side_frame(left_center.x,
                              left_center.y,
                              left_center.z,
                              -frame.x_axis_x,
                              -frame.x_axis_y,
                              -frame.x_axis_z,
                              true),
              parameters.height,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:left"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "right",
              "Right",
              "planar",
              right_center.x,
              right_center.y,
              right_center.z,
              frame.x_axis_x,
              frame.x_axis_y,
              frame.x_axis_z,
              make_side_frame(right_center.x,
                              right_center.y,
                              right_center.z,
                              frame.x_axis_x,
                              frame.x_axis_y,
                              frame.x_axis_z,
                              true),
              parameters.height,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:right"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "front",
              "Front",
              "planar",
              front_center.x,
              front_center.y,
              front_center.z,
              -frame.y_axis_x,
              -frame.y_axis_y,
              -frame.y_axis_z,
              make_side_frame(front_center.x,
                              front_center.y,
                              front_center.z,
                              -frame.y_axis_x,
                              -frame.y_axis_y,
                              -frame.y_axis_z,
                              false),
              parameters.width,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:front"));

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "back",
              "Back",
              "planar",
              back_center.x,
              back_center.y,
              back_center.z,
              frame.y_axis_x,
              frame.y_axis_y,
              frame.y_axis_z,
              make_side_frame(back_center.x,
                              back_center.y,
                              back_center.z,
                              frame.y_axis_x,
                              frame.y_axis_y,
                              frame.y_axis_z,
                              false),
              parameters.width,
              parameters.depth,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:back"));
        }
      } else if (parameters.profile_kind == "polygon") {
        polygon_extrudes.push_back(ViewportPolygonExtrudePrimitive{
            .id = feature.id,
            .label = feature.name,
            .plane_id = parameters.plane_id,
            .plane_frame = parameters.plane_frame.has_value()
                               ? std::optional<ViewportSketchPlaneFrame>(
                                     make_sketch_plane_frame(parameters.plane_frame.value()))
                               : std::nullopt,
            .profile_points = parameters.profile_points,
            .inner_loops = parameters.inner_loops,
            .depth = parameters.depth,
            .is_selected = is_selected,
            .appearance_color = body_appearance_color(*view, feature.id),
        });

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();

        for (const auto& point : parameters.profile_points) {
          if (parameters.plane_frame.has_value()) {
            include_world_point_for_frame(parameters.plane_frame.value(),
                                          point.x,
                                          point.y,
                                          parameters.depth,
                                          min_x,
                                          max_x,
                                          min_y,
                                          max_y,
                                          min_z,
                                          max_z);
            continue;
          }

          include_world_point_for_plane(parameters.plane_id,
                                       point.x,
                                       point.y,
                                       parameters.depth,
                                       min_x,
                                       max_x,
                                       min_y,
                                       max_y,
                                       min_z,
                                       max_z);
        }

        if (!parameters.profile_points.empty()) {
          scene_width = std::max(scene_width, max_x);
          max_height = std::max(max_height, max_y);
          max_depth = std::max(max_depth, max_z);
        }

        double face_center_x = (min_x + max_x) / 2.0;
        double face_center_y = parameters.depth;
        double face_center_z = (min_z + max_z) / 2.0;
        double face_width = max_x - min_x;
        double face_height = max_z - min_z;
        double face_normal_x = 0.0;
        double face_normal_y = 1.0;
        double face_normal_z = 0.0;
        if (parameters.plane_frame.has_value()) {
          const auto& frame = parameters.plane_frame.value();
          const double local_center_x = (min_x + max_x) / 2.0;
          const double local_center_y = (min_z + max_z) / 2.0;
          const auto base_center =
              to_world_point(frame, local_center_x, local_center_y, 0.0);
          const auto top_center =
              to_world_point(frame, local_center_x, local_center_y, parameters.depth);
          face_center_x = top_center.x;
          face_center_y = top_center.y;
          face_center_z = top_center.z;
          face_normal_x = frame.normal_x;
          face_normal_y = frame.normal_y;
          face_normal_z = frame.normal_z;

          solid_faces.push_back(make_solid_face(
              feature.id,
              feature.kind,
              "base",
              "Base",
              "planar",
              base_center.x,
              base_center.y,
              base_center.z,
              -frame.normal_x,
              -frame.normal_y,
              -frame.normal_z,
              make_plane_frame(base_center.x,
                               base_center.y,
                               base_center.z,
                               frame.x_axis_x,
                               frame.x_axis_y,
                               frame.x_axis_z,
                               frame.y_axis_x,
                               frame.y_axis_y,
                               frame.y_axis_z,
                               -frame.normal_x,
                               -frame.normal_y,
                               -frame.normal_z),
              face_width,
              face_height,
              0.0,
              view->selected_face_id.has_value() &&
                  view->selected_face_id.value() == feature.id + ":face:base"));
        } else if (parameters.plane_id == "ref-plane-yz") {
          face_center_x = parameters.depth;
          face_center_y = (min_y + max_y) / 2.0;
          face_center_z = (min_z + max_z) / 2.0;
          face_width = max_y - min_y;
          face_height = max_z - min_z;
          face_normal_x = 1.0;
          face_normal_y = 0.0;
        } else if (parameters.plane_id == "ref-plane-xz") {
          face_center_x = (min_x + max_x) / 2.0;
          face_center_y = (min_y + max_y) / 2.0;
          face_center_z = parameters.depth;
          face_width = max_x - min_x;
          face_height = max_y - min_y;
          face_normal_y = 0.0;
          face_normal_z = 1.0;
        }

        solid_faces.push_back(make_solid_face(
            feature.id,
            feature.kind,
            "top",
            "Top",
            "planar",
            face_center_x,
            face_center_y,
            face_center_z,
            face_normal_x,
            face_normal_y,
            face_normal_z,
            parameters.plane_frame.has_value()
                ? make_plane_frame(face_center_x,
                                   face_center_y,
                                   face_center_z,
                                   parameters.plane_frame->x_axis_x,
                                   parameters.plane_frame->x_axis_y,
                                   parameters.plane_frame->x_axis_z,
                                   parameters.plane_frame->y_axis_x,
                                   parameters.plane_frame->y_axis_y,
                                   parameters.plane_frame->y_axis_z,
                                   parameters.plane_frame->normal_x,
                                   parameters.plane_frame->normal_y,
                                   parameters.plane_frame->normal_z)
                : make_face_frame_for_plane(
                      parameters.plane_id, face_center_x, face_center_y, face_center_z),
            face_width,
            face_height,
            0.0,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));

        if (parameters.plane_frame.has_value() &&
            parameters.profile_points.size() >= 3) {
          const auto& frame = parameters.plane_frame.value();
          for (size_t index = 0; index < parameters.profile_points.size(); ++index) {
            const auto& start = parameters.profile_points[index];
            const auto& end = parameters.profile_points[(index + 1) %
                                                       parameters.profile_points.size()];

            const auto base_start = to_world_point(frame, start.x, start.y, 0.0);
            const auto base_end = to_world_point(frame, end.x, end.y, 0.0);
            const auto top_start = to_world_point(frame, start.x, start.y, parameters.depth);
            const auto top_end = to_world_point(frame, end.x, end.y, parameters.depth);

            const WorldVector edge_vector = subtract_points(base_end, base_start);
            const WorldVector depth_vector = subtract_points(top_start, base_start);
            const double edge_length = vector_length(edge_vector);
            const WorldVector face_normal = normalize_vector(cross_product(edge_vector, depth_vector));
            if (edge_length <= 0.0 ||
                vector_length(face_normal) <= 0.0) {
              continue;
            }

            const WorldPoint center{
                .x = (base_start.x + base_end.x + top_start.x + top_end.x) / 4.0,
                .y = (base_start.y + base_end.y + top_start.y + top_end.y) / 4.0,
                .z = (base_start.z + base_end.z + top_start.z + top_end.z) / 4.0,
            };

            const WorldVector edge_axis = normalize_vector(edge_vector);
            const WorldVector depth_axis = normalize_vector(depth_vector);

            solid_faces.push_back(make_solid_face(
                feature.id,
                feature.kind,
                "side-" + std::to_string(index),
                "Side",
                "planar",
                center.x,
                center.y,
                center.z,
                face_normal.x,
                face_normal.y,
                face_normal.z,
                make_plane_frame(center.x,
                                 center.y,
                                 center.z,
                                 edge_axis.x,
                                 edge_axis.y,
                                 edge_axis.z,
                                 depth_axis.x,
                                 depth_axis.y,
                                 depth_axis.z,
                                 face_normal.x,
                                 face_normal.y,
                                 face_normal.z),
                edge_length,
                parameters.depth,
                0.0,
                view->selected_face_id.has_value() &&
                    view->selected_face_id.value() ==
                        feature.id + ":face:side-" + std::to_string(index)));
          }
        }
      } else if (parameters.profile_kind == "circle") {
        if (parameters.plane_id != "ref-plane-xy") {
          continue;
        }

        if (parameters.plane_id == "ref-plane-xy") {
          cylinders.push_back(ViewportCylinderPrimitive{
              .id = feature.id,
              .label = feature.name,
              .radius = parameters.radius,
              .height = parameters.depth,
              .x_offset = parameters.start_x - parameters.radius,
              .center_x = parameters.start_x,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.radius);
          scene_width = std::max(scene_width, parameters.start_x + parameters.radius);
        } else {
          cylinders.push_back(ViewportCylinderPrimitive{
              .id = feature.id,
              .label = feature.name,
              .radius = parameters.radius,
              .height = parameters.depth,
              .x_offset = parameters.start_x - parameters.radius,
              .center_x = parameters.start_x,
              .center_y = parameters.depth / 2.0,
              .center_z = parameters.start_y,
              .is_selected = is_selected,
              .appearance_color = body_appearance_color(*view, feature.id),
          });
          max_height = std::max(max_height, parameters.depth);
          max_depth = std::max(max_depth, parameters.start_y + parameters.radius);
          scene_width = std::max(scene_width, parameters.start_x + parameters.radius);
        }
        solid_faces.push_back(make_solid_face(
            feature.id,
            feature.kind,
            "top",
            "Top",
            "planar",
            parameters.start_x,
            parameters.depth,
            parameters.start_y,
            0.0,
            1.0,
            0.0,
            make_face_frame_for_plane(
                parameters.plane_id,
                parameters.start_x,
                parameters.depth,
                parameters.start_y),
            parameters.radius * 2.0,
            parameters.radius * 2.0,
            parameters.radius,
            view->selected_face_id.has_value() &&
                view->selected_face_id.value() == feature.id + ":face:top"));
      }
      continue;
    }

    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      const auto profiles = detect_sketch_profiles(feature);

      // Emit Mirror tool preview geometry first so the rest of
      // the per-line bookkeeping (constraint badges, dimensions,
      // anchors) doesn't run for transient entities — they're
      // pure visual previews. The flag flips them to a dashed
      // translucent style on the UI side.
      if (feature.sketch_parameters->pending_mirror.has_value()) {
        for (const auto& preview_line :
             feature.sketch_parameters->pending_mirror->generated_lines) {
          auto primitive = make_sketch_line_primitive(
              preview_line, *feature.sketch_parameters, /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_lines.push_back(primitive);
        }
        for (const auto& preview_circle :
             feature.sketch_parameters->pending_mirror->generated_circles) {
          auto primitive = make_sketch_circle_primitive(
              preview_circle,
              *feature.sketch_parameters,
              /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_circles.push_back(primitive);
        }
      }

      const auto is_sketch_entity_selected = [&](const std::string& id) {
        if (!view->selected_sketch_entity_ids.empty()) {
          return std::find(view->selected_sketch_entity_ids.begin(),
                           view->selected_sketch_entity_ids.end(),
                           id) != view->selected_sketch_entity_ids.end();
        }
        return view->selected_sketch_entity_id.has_value() &&
               view->selected_sketch_entity_id.value() == id;
      };
      const auto is_sketch_point_selected = [&](const std::string& id) {
        if (!view->selected_sketch_point_ids.empty()) {
          return std::find(view->selected_sketch_point_ids.begin(),
                           view->selected_sketch_point_ids.end(),
                           id) != view->selected_sketch_point_ids.end();
        }
        return view->selected_sketch_point_id.has_value() &&
               view->selected_sketch_point_id.value() == id;
      };
      // Track which lines carry at least one relation badge so we
      // can offset the H/V badge to avoid overlap.
      std::unordered_set<std::string> lines_with_relation_badge;
      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          lines_with_relation_badge.insert(relation.first_line_id);
          if (!relation.second_line_id.empty()) {
            lines_with_relation_badge.insert(relation.second_line_id);
          }
        }
      }

      for (const auto& line : feature.sketch_parameters->lines) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(line.id);
        sketch_lines.push_back(
            make_sketch_line_primitive(line,
                                       *feature.sketch_parameters,
                                       is_selected_sketch_entity));
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "line_length" &&
                       dimension.entity_id == line.id;
              });
          if (dimension_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_dimension =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dimension_it->id;
            sketch_dimensions.push_back(make_line_dimension_primitive(
                line,
                *dimension_it,
                *feature.sketch_parameters,
                is_selected_dimension));
          }

          const auto angle_dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "line_angle" &&
                       dimension.entity_id == line.id;
              });
          if (angle_dimension_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_angle_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() ==
                    angle_dimension_it->id;
            sketch_dimensions.push_back(make_line_angle_dimension_primitive(
                line,
                *angle_dimension_it,
                *feature.sketch_parameters,
                is_selected_angle_dim));
          }

          if (line.constraint.has_value()) {
            const bool has_relation =
                lines_with_relation_badge.find(line.id) !=
                lines_with_relation_badge.end();
            sketch_constraints.push_back(make_line_constraint_primitive(
                line,
                feature.sketch_parameters->plane_id,
                line.constraint.value(),
                line.constraint.value() == "horizontal" ? "H" : "V",
                is_selected_sketch_entity,
                /*related_entity_id=*/std::nullopt,
                /*badge_offset_multiplier=*/has_relation ? 3.0 : 1.0));
          }
        }
      }

      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          if (relation.kind != "equal_length" &&
              relation.kind != "perpendicular" &&
              relation.kind != "parallel") {
            continue;
          }

          const auto first_line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == relation.first_line_id; });
          const auto second_line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == relation.second_line_id;
              });

          if (first_line_it == feature.sketch_parameters->lines.end() ||
              second_line_it == feature.sketch_parameters->lines.end()) {
            continue;
          }

          const bool first_is_selected =
              is_sketch_entity_selected(first_line_it->id);
          const bool second_is_selected =
              is_sketch_entity_selected(second_line_it->id);

          sketch_constraints.push_back(make_line_constraint_primitive(
              *first_line_it,
              feature.sketch_parameters->plane_id,
              relation.kind,
              relation.kind == "equal_length"
                  ? "="
                  : relation.kind == "perpendicular" ? "P" : "//",
              first_is_selected,
              relation.second_line_id));
          sketch_constraints.push_back(make_line_constraint_primitive(
              *second_line_it,
              feature.sketch_parameters->plane_id,
              relation.kind,
              relation.kind == "equal_length"
                  ? "="
                  : relation.kind == "perpendicular" ? "P" : "//",
              second_is_selected,
              relation.first_line_id));
        }

        // Tangent (line ↔ circle) badge. Same line-mounted "T" glyph
        // rendered at the line's midpoint as the other line-line
        // relations. We don't put a matching badge on the circle —
        // the relation only drives the line's end, so attaching the
        // marker to the line keeps the affordance visually tied to
        // the entity that actually moves.
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          if (relation.kind != "tangent_line_circle") {
            continue;
          }

          const auto line_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == relation.first_line_id;
              });
          if (line_it == feature.sketch_parameters->lines.end()) {
            continue;
          }

          const bool is_selected =
              is_sketch_entity_selected(line_it->id);
          sketch_constraints.push_back(make_line_constraint_primitive(
              *line_it,
              feature.sketch_parameters->plane_id,
              relation.kind,
              "T",
              is_selected,
              relation.second_line_id));
        }

        // Midpoint anchors render a small "M" badge at the host
        // line's midpoint so the user sees that the bound point is
        // tracking the line's midpoint (CAD convention).
        for (const auto& anchor : feature.sketch_parameters->midpoint_anchors) {
          const auto host_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == anchor.line_id; });
          if (host_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const bool is_selected =
              is_sketch_entity_selected(anchor.point_id);
          sketch_constraints.push_back(make_line_constraint_primitive(
              *host_it,
              feature.sketch_parameters->plane_id,
              "midpoint",
              "M",
              is_selected,
              anchor.point_id));
        }

        // Point-line anchors render a "/" badge at the anchor's
        // parametric position along the host line. We can't reuse
        // `make_line_constraint_primitive` (it always places the
        // badge at the line's midpoint), so we build the primitive
        // inline here, mirroring the same offset / normal math.
        for (const auto& anchor :
             feature.sketch_parameters->point_line_anchors) {
          const auto host_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) { return line.id == anchor.line_id; });
          if (host_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const double dx = host_it->end_x - host_it->start_x;
          const double dy = host_it->end_y - host_it->start_y;
          const double length = std::sqrt(dx * dx + dy * dy);
          const double normal_x = length > 0.0 ? -dy / length : 0.0;
          const double normal_y = length > 0.0 ? dx / length : 1.0;
          const double anchor_x = host_it->start_x + anchor.t * dx;
          const double anchor_y = host_it->start_y + anchor.t * dy;
          const WorldPoint position = to_world_point(
              feature.sketch_parameters->plane_id,
              anchor_x + normal_x * kConstraintBadgeOffset,
              anchor_y + normal_y * kConstraintBadgeOffset);
          const bool is_selected =
              is_sketch_entity_selected(anchor.point_id);
          sketch_constraints.push_back(ViewportSketchConstraintPrimitive{
              .constraint_id = "constraint-on_line-" + anchor.point_id,
              .plane_id = feature.sketch_parameters->plane_id,
              .kind = "on_line",
              .entity_id = host_it->id,
              .related_entity_id = anchor.point_id,
              .label = "/",
              .is_selected = is_selected,
              .position_x = position.x,
              .position_y = position.y,
              .position_z = position.z,
          });
        }

        // Angle dimensions span two lines. Emit them once per dim
        // (rather than per line) so we don't double-render. Skip
        // silently if either referenced line is missing — that's
        // possible mid-edit when a line is being deleted.
        for (const auto& dimension :
             feature.sketch_parameters->dimensions) {
          if (dimension.kind != "angle") {
            continue;
          }
          const auto line_a_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == dimension.entity_id;
              });
          const auto line_b_it = std::find_if(
              feature.sketch_parameters->lines.begin(),
              feature.sketch_parameters->lines.end(),
              [&](const SketchLine& line) {
                return line.id == dimension.secondary_entity_id;
              });
          if (line_a_it == feature.sketch_parameters->lines.end() ||
              line_b_it == feature.sketch_parameters->lines.end()) {
            continue;
          }
          const bool is_selected_dimension =
              view->selected_sketch_dimension_id.has_value() &&
              view->selected_sketch_dimension_id.value() == dimension.id;
          sketch_dimensions.push_back(make_angle_dimension_primitive(
              *line_a_it,
              *line_b_it,
              dimension,
              *feature.sketch_parameters,
              is_selected_dimension));
        }

        for (const auto& dimension :
             feature.sketch_parameters->dimensions) {
          const bool is_selected_dimension =
              view->selected_sketch_dimension_id.has_value() &&
              view->selected_sketch_dimension_id.value() == dimension.id;
          if (dimension.kind == "circle_center_distance") {
            const auto driven_circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.entity_id;
                });
            const auto reference_circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.secondary_entity_id;
                });
            if (driven_circle_it == feature.sketch_parameters->circles.end() ||
                reference_circle_it == feature.sketch_parameters->circles.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_circle_center_distance_dimension_primitive(
                    *driven_circle_it,
                    *reference_circle_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "circle_line_distance") {
            const auto circle_it = std::find_if(
                feature.sketch_parameters->circles.begin(),
                feature.sketch_parameters->circles.end(),
                [&](const SketchCircle& circle) {
                  return circle.id == dimension.entity_id;
                });
            const auto line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.secondary_entity_id;
                });
            if (circle_it == feature.sketch_parameters->circles.end() ||
                line_it == feature.sketch_parameters->lines.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_circle_line_distance_dimension_primitive(
                    *circle_it,
                    *line_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "point_distance") {
            const auto point_a_it = std::find_if(
                feature.sketch_parameters->points.begin(),
                feature.sketch_parameters->points.end(),
                [&](const SketchPoint& p) {
                  return p.id == dimension.entity_id;
                });
            const auto point_b_it = std::find_if(
                feature.sketch_parameters->points.begin(),
                feature.sketch_parameters->points.end(),
                [&](const SketchPoint& p) {
                  return p.id == dimension.secondary_entity_id;
                });
            if (point_a_it == feature.sketch_parameters->points.end() ||
                point_b_it == feature.sketch_parameters->points.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_point_distance_dimension_primitive(
                    *point_a_it,
                    *point_b_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          } else if (dimension.kind == "line_line_distance") {
            const auto driven_line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.entity_id;
                });
            const auto reference_line_it = std::find_if(
                feature.sketch_parameters->lines.begin(),
                feature.sketch_parameters->lines.end(),
                [&](const SketchLine& line) {
                  return line.id == dimension.secondary_entity_id;
                });
            if (driven_line_it == feature.sketch_parameters->lines.end() ||
                reference_line_it == feature.sketch_parameters->lines.end()) {
              continue;
            }
            sketch_dimensions.push_back(
                make_line_line_distance_dimension_primitive(
                    *driven_line_it,
                    *reference_line_it,
                    dimension,
                    *feature.sketch_parameters,
                    is_selected_dimension));
          }
        }
      }

      for (const auto& arc : feature.sketch_parameters->arcs) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(arc.id);
        sketch_arcs.push_back(make_sketch_arc_primitive(
            arc,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
      }

      for (const auto& circle : feature.sketch_parameters->circles) {
        const bool is_selected_sketch_entity =
            is_sketch_entity_selected(circle.id);
        sketch_circles.push_back(make_sketch_circle_primitive(
            circle,
            *feature.sketch_parameters,
            is_selected_sketch_entity));
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dimension_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& dimension) {
                return dimension.kind == "circle_radius" &&
                       dimension.entity_id == circle.id;
              });
          if (dimension_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_dimension =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dimension_it->id;
            sketch_dimensions.push_back(make_circle_dimension_primitive(
                circle,
                *dimension_it,
                *feature.sketch_parameters,
                is_selected_dimension));
          }
        }
      }

      for (const auto& polygon : feature.sketch_parameters->polygons) {
        const bool is_selected_polygon =
            is_sketch_entity_selected(polygon.id);
        sketch_polygons.push_back(make_sketch_polygon_primitive(
            polygon, *feature.sketch_parameters, is_selected_polygon));
        // Emit polygon radius dimension
        if (view->active_sketch_feature_id.has_value() &&
            view->active_sketch_feature_id.value() == feature.id) {
          const auto dim_it = std::find_if(
              feature.sketch_parameters->dimensions.begin(),
              feature.sketch_parameters->dimensions.end(),
              [&](const SketchDimension& d) {
                return d.kind == "polygon_radius" && d.entity_id == polygon.id;
              });
          if (dim_it != feature.sketch_parameters->dimensions.end()) {
            const bool is_selected_dim =
                view->selected_sketch_dimension_id.has_value() &&
                view->selected_sketch_dimension_id.value() == dim_it->id;
            const WorldPoint pc = to_world_point(*feature.sketch_parameters, polygon.center_x, polygon.center_y);
            const WorldPoint pd = to_world_point(*feature.sketch_parameters, polygon.center_x + polygon.radius, polygon.center_y);
            sketch_dimensions.push_back(ViewportSketchDimensionPrimitive{
                .dimension_id = dim_it->id,
                .plane_id = feature.sketch_parameters->plane_id,
                .kind = "polygon_radius",
                .entity_id = polygon.id,
                .label = "R " + format_dimension_value(dim_it->value) + " mm",
                .is_selected = is_selected_dim,
                .anchor_start_x = pc.x, .anchor_start_y = pc.y, .anchor_start_z = pc.z,
                .anchor_end_x = pd.x, .anchor_end_y = pd.y, .anchor_end_z = pd.z,
                .dimension_start_x = pc.x, .dimension_start_y = pc.y, .dimension_start_z = pc.z,
                .dimension_end_x = pd.x, .dimension_end_y = pd.y, .dimension_end_z = pd.z,
                .label_x = pd.x, .label_y = pd.y, .label_z = pd.z,
            });
          }
        }
      }

      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& point : feature.sketch_parameters->points) {
          const bool is_selected_point =
              is_sketch_point_selected(point.id);
          sketch_points.push_back(make_sketch_point_primitive(
              point, *feature.sketch_parameters, is_selected_point));
          if (point.is_fixed) {
            sketch_constraints.push_back(make_point_constraint_primitive(
                point, *feature.sketch_parameters, is_selected_point));
          }
        }

        // Emit badge primitives for explicit coincident and concentric
        // constraints stored in the sketch parameters.
        for (const auto& c : feature.sketch_parameters->constraints) {
          if (c.kind == "coincident") {
            // Find the shared point position from the lines listed in
            // target_ids. After the point merge, all lines share one
            // endpoint; find a point ID that appears in at least two.
            std::string shared_point_id;
            for (const auto& tid : c.target_ids) {
              for (const auto& line : feature.sketch_parameters->lines) {
                if (line.id != tid) continue;
                const std::string& sp = line.start_point_id;
                const std::string& ep = line.end_point_id;
                for (const auto& tid2 : c.target_ids) {
                  if (tid2 == tid) continue;
                  for (const auto& line2 : feature.sketch_parameters->lines) {
                    if (line2.id != tid2) continue;
                    if (line2.start_point_id == sp || line2.end_point_id == sp) {
                      shared_point_id = sp; break;
                    }
                    if (line2.start_point_id == ep || line2.end_point_id == ep) {
                      shared_point_id = ep; break;
                    }
                  }
                  if (!shared_point_id.empty()) break;
                }
                if (!shared_point_id.empty()) break;
              }
              if (!shared_point_id.empty()) break;
            }
            if (!shared_point_id.empty()) {
              const auto pt_pos = find_point_position(
                  *feature.sketch_parameters, shared_point_id);
              if (pt_pos.has_value()) {
                const WorldPoint pos = to_world_point(
                    *feature.sketch_parameters,
                    std::get<0>(pt_pos.value()),
                    std::get<1>(pt_pos.value()),
                    kSketchPlaneOffset + kConstraintBadgeOffset);
                const bool is_sel =
                    is_sketch_point_selected(shared_point_id);
                sketch_constraints.push_back(
                    ViewportSketchConstraintPrimitive{
                        .constraint_id = c.constraint_id,
                        .plane_id = feature.sketch_parameters->plane_id,
                        .kind = "coincident",
                        .entity_id = c.constraint_id,
                        .related_entity_id = std::nullopt,
                        .label = "∞",
                        .is_selected = is_sel,
                        .position_x = pos.x,
                        .position_y = pos.y,
                        .position_z = pos.z,
                    });
              }
            }
          }
          if (c.kind == "concentric") {
            // Target IDs are circle IDs; place badge at the first
            // circle's center.
            if (!c.target_ids.empty()) {
              const auto& circ_id = c.target_ids[0];
              for (const auto& circ : feature.sketch_parameters->circles) {
                if (circ.id == circ_id) {
                  const WorldPoint pos = to_world_point(
                      *feature.sketch_parameters,
                      circ.center_x, circ.center_y,
                      kSketchPlaneOffset + kConstraintBadgeOffset);
                  const bool is_sel =
                      is_sketch_point_selected(
                          "point-circle-" + circ_id + "-center");
                  sketch_constraints.push_back(
                      ViewportSketchConstraintPrimitive{
                          .constraint_id = c.constraint_id,
                          .plane_id = feature.sketch_parameters->plane_id,
                          .kind = "concentric",
                          .entity_id = circ_id,
                          .related_entity_id = std::nullopt,
                          .label = "◎",
                          .is_selected = is_sel,
                          .position_x = pos.x,
                          .position_y = pos.y,
                          .position_z = pos.z,
                      });
                  break;
                }
              }
            }
          }
        }
      }

      for (const auto& rectangle : profiles.polygons) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      rectangle.id) != view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_rectangle_profile_primitive(rectangle, is_selected_profile));
      }

      for (const auto& circle_profile : profiles.circles) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      circle_profile.id) !=
            view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_circle_profile_primitive(circle_profile, is_selected_profile));
      }
    }
  }

  scene_width = std::max(
      scene_width,
      (boxes.empty() && cylinders.empty()) ? 0.0 : current_x_offset - kBoxSpacing);
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
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-xy",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
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
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-yz",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
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
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-xz",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
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

  // Emit construction-plane features as additional reference planes.
  // We reuse `ViewportReferencePlane` (rather than introducing a new
  // viewport struct) so existing viewport machinery — selection,
  // active-sketch-plane highlighting, hierarchy filtering — keeps
  // working unchanged. Construction planes ship the cached world-space
  // `plane_frame`; the renderer uses it instead of the legacy
  // orientation rotation.
  for (const auto& feature : view->feature_history) {
    if (feature.suppressed) {
      continue;
    }
    if (feature.kind != "construction_plane" ||
        !feature.construction_plane_parameters.has_value()) {
      continue;
    }
    const auto& params = feature.construction_plane_parameters.value();
    reference_planes.push_back(ViewportReferencePlane{
        .id = feature.id,
        .label = feature.name,
        .orientation = "custom",
        .center_x = params.plane_frame.origin_x,
        .center_y = params.plane_frame.origin_y,
        .center_z = params.plane_frame.origin_z,
        .width = kReferencePlaneSize,
        .height = kReferencePlaneSize,
        .is_selected =
            view->selected_reference_id.has_value() &&
            view->selected_reference_id.value() == feature.id,
        .is_active_sketch_plane =
            view->active_sketch_plane_id.has_value() &&
            view->active_sketch_plane_id.value() == feature.id,
        .plane_frame = params.plane_frame,
    });
  }

  for (const auto& feature : view->feature_history) {
    if (feature.suppressed) {
      continue;
    }
    if (feature.kind == "construction_axis" &&
        feature.construction_axis_parameters.has_value()) {
      const auto& params = feature.construction_axis_parameters.value();
      reference_axes.push_back(ViewportReferenceAxis{
          .id = feature.id,
          .label = feature.name,
          .axis = "custom",
          .start_x = params.start_x,
          .start_y = params.start_y,
          .start_z = params.start_z,
          .end_x = params.end_x,
          .end_y = params.end_y,
          .end_z = params.end_z,
      });
      continue;
    }
    if (feature.kind == "construction_point" &&
        feature.construction_point_parameters.has_value()) {
      const auto& params = feature.construction_point_parameters.value();
      reference_points.push_back(ViewportReferencePoint{
          .id = feature.id,
          .label = feature.name,
          .x = params.x,
          .y = params.y,
          .z = params.z,
          .is_selected = view->selected_feature_id.has_value() &&
                         view->selected_feature_id.value() == feature.id,
      });
      continue;
    }
    if (feature.kind == "helix" && feature.helix_parameters.has_value() &&
        !feature.dependency_broken) {
      const auto& params = feature.helix_parameters.value();
      helices.push_back(ViewportHelixPrimitive{
          .id = feature.id,
          .label = feature.name,
          .points = params.points,
          .is_selected = view->selected_feature_id.has_value() &&
                         view->selected_feature_id.value() == feature.id,
      });
      continue;
    }
    if (feature.kind == "thread" && feature.thread_parameters.has_value() &&
        !feature.dependency_broken &&
        feature.thread_parameters->representation == "cosmetic") {
      const auto& params = feature.thread_parameters.value();
      const auto axis = resolve_construction_axis_source(*view, params.axis_source_id);
      if (axis.has_value()) {
        const double radius =
            std::max(params.major_diameter, params.minor_diameter) * 0.5 * 1.015;
        const auto points = make_cosmetic_thread_points(
            axis->start_x,
            axis->start_y,
            axis->start_z,
            axis->end_x,
            axis->end_y,
            axis->end_z,
            radius,
            params.pitch,
            params.length,
            params.start_offset,
            params.handedness != "left");
        if (!points.empty()) {
          helices.push_back(ViewportHelixPrimitive{
              .id = feature.id,
              .label = feature.name,
              .points = points,
              .is_selected = view->selected_feature_id.has_value() &&
                             view->selected_feature_id.value() == feature.id,
          });
        }
      }
      continue;
    }
    if (feature.kind == "hole" && feature.hole_parameters.has_value() &&
        !feature.dependency_broken &&
        feature.hole_parameters->thread_enabled &&
        feature.hole_parameters->thread_representation == "cosmetic" &&
        feature.hole_parameters->thread_pitch > 0.0) {
      const auto& params = feature.hole_parameters.value();
      const double start_x = params.plane_frame.origin_x +
                             params.plane_frame.x_axis_x * params.center_x +
                             params.plane_frame.y_axis_x * params.center_y;
      const double start_y = params.plane_frame.origin_y +
                             params.plane_frame.x_axis_y * params.center_x +
                             params.plane_frame.y_axis_y * params.center_y;
      const double start_z = params.plane_frame.origin_z +
                             params.plane_frame.x_axis_z * params.center_x +
                             params.plane_frame.y_axis_z * params.center_y;
      const double length = std::min(
          std::abs(params.thread_depth),
          params.extent_type == "through_all" ? std::max(10000.0, std::abs(params.depth))
                                              : std::abs(params.depth));
      const double end_x = start_x - params.plane_frame.normal_x * length;
      const double end_y = start_y - params.plane_frame.normal_y * length;
      const double end_z = start_z - params.plane_frame.normal_z * length;
      const double radius =
          std::max(params.major_diameter, params.minor_diameter) * 0.5;
      const auto points = make_cosmetic_thread_points(start_x,
                                                      start_y,
                                                      start_z,
                                                      end_x,
                                                      end_y,
                                                      end_z,
                                                      radius,
                                                      params.thread_pitch,
                                                      length,
                                                      0.0,
                                                      true);
      if (!points.empty()) {
        helices.push_back(ViewportHelixPrimitive{
            .id = feature.id + ":thread",
            .label = feature.name + " thread",
            .points = points,
            .is_selected = view->selected_feature_id.has_value() &&
                           view->selected_feature_id.value() == feature.id,
        });
      }
      continue;
    }
    if (feature.kind == "fastener" && feature.fastener_parameters.has_value() &&
        feature.fastener_parameters->thread_representation == "cosmetic") {
      const auto& params = feature.fastener_parameters.value();
      const double shaft_radius = params.diameter * 0.5;
      const double thread_length =
          std::min(std::max(params.thread_length, 0.0), params.length);
      const double pitch =
          params.pitch > 0.0 ? params.pitch : std::max(params.diameter * 0.16, 0.2);
      const auto points = make_cosmetic_thread_points(
          shaft_radius,
          0.0,
          shaft_radius,
          shaft_radius,
          params.length,
          shaft_radius,
          shaft_radius * 1.015,
          pitch,
          thread_length,
          0.0,
          true);
      if (!points.empty()) {
        helices.push_back(ViewportHelixPrimitive{
            .id = feature.id + ":thread",
            .label = feature.name + " thread",
            .points = points,
            .is_selected = view->selected_feature_id.has_value() &&
                           view->selected_feature_id.value() == feature.id,
        });
      }
    }
  }

  // Drop legacy named-suffix analytical faces for extrude features
  // (e.g. "<id>:face:top", "<id>:face:base", "<id>:face:side-N"). The
  // per-feature loop above still emits those for backwards
  // compatibility with the old tests / serialization paths, but every
  // extrude body now also gets accurate body-derived faces from
  // `enumerate_body_faces` (with numeric suffixes "<id>:face:0",
  // "<id>:face:1", ...). When both are present they overlap as
  // transparent meshes at nearly-identical world positions, producing
  // a "ghost plane" the user can't easily click through. Body-derived
  // faces always win because they handle filleting, plane-frame
  // rotations, and booleaned topology correctly — analytical ones
  // only ever matched on the simple new-body case.
  {
    auto is_named_suffix_for_extrude =
        [&](const ViewportSolidFace& face) -> bool {
      if (face.owner_kind != "extrude") {
        return false;
      }
      // Find the suffix after the last ":face:" delimiter and check
      // whether it parses as a non-negative integer. Numeric -> body-
      // derived (keep). Non-numeric -> legacy analytical (drop).
      const std::string separator = ":face:";
      const auto pos = face.face_id.rfind(separator);
      if (pos == std::string::npos) {
        return false;
      }
      const std::string suffix =
          face.face_id.substr(pos + separator.size());
      if (suffix.empty()) {
        return false;
      }
      for (const char ch : suffix) {
        if (ch < '0' || ch > '9') {
          return true;
        }
      }
      return false;
    };
    solid_faces.erase(std::remove_if(solid_faces.begin(),
                                     solid_faces.end(),
                                     is_named_suffix_for_extrude),
                      solid_faces.end());
  }

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

  // Build pre-computed snap candidates for the active sketch.
  std::vector<SnapCandidate> snap_candidates;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        const auto& sketch = feat.sketch_parameters.value();
        const auto& filter = document->selection_filter;
        // Endpoints
        if (filter.snap_endpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.point_id=line.start_point_id,.local_x=line.start_x,.local_y=line.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.point_id=line.end_point_id,.local_x=line.end_x,.local_y=line.end_y,.distance=0,.label="Endpoint"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.point_id=arc.start_point_id,.local_x=arc.start_x,.local_y=arc.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.point_id=arc.end_point_id,.local_x=arc.end_x,.local_y=arc.end_y,.distance=0,.label="Endpoint"});
          }
        }
        // Midpoints
        if (filter.snap_midpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double mx = (line.start_x + line.end_x) / 2.0;
            const double my = (line.start_y + line.end_y) / 2.0;
            snap_candidates.push_back({.kind="midpoint",.entity_id=line.id,.point_id="",.local_x=mx,.local_y=my,.distance=0,.label="Midpoint"});
          }
        }
        // Centers
        if (filter.snap_center) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=circle.id,.point_id="",.local_x=circle.center_x,.local_y=circle.center_y,.distance=0,.label="Center"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=arc.id,.point_id="",.local_x=arc.center_x,.local_y=arc.center_y,.distance=0,.label="Center"});
          }
          for (const auto& poly : sketch.polygons) {
            if (poly.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=poly.id,.point_id="",.local_x=poly.center_x,.local_y=poly.center_y,.distance=0,.label="Center"});
          }
        }
        // Quadrant points (4 per circle)
        if (filter.snap_quadrant) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            const double cx = circle.center_x;
            const double cy = circle.center_y;
            const double r = circle.radius;
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx+r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx,.local_y=cy+r,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx-r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx,.local_y=cy-r,.distance=0,.label="Quadrant"});
          }
        }
        // Intersection points (line-line and line-arc)
        if (filter.snap_intersection) {
          // Line-line intersections
          for (size_t i = 0; i < sketch.lines.size(); ++i) {
            const auto& a = sketch.lines[i];
            if (a.is_construction && !filter.select_construction) continue;
            for (size_t j = i + 1; j < sketch.lines.size(); ++j) {
              const auto& b = sketch.lines[j];
              if (b.is_construction && !filter.select_construction) continue;
              const double a_dx = a.end_x - a.start_x;
              const double a_dy = a.end_y - a.start_y;
              const double b_dx = b.end_x - b.start_x;
              const double b_dy = b.end_y - b.start_y;
              const double denom = a_dx * b_dy - a_dy * b_dx;
              if (std::abs(denom) < 1e-12) continue;
              const double t = ((b.start_x - a.start_x) * b_dy - (b.start_y - a.start_y) * b_dx) / denom;
              const double u = ((b.start_x - a.start_x) * a_dy - (b.start_y - a.start_y) * a_dx) / denom;
              if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) continue;
              snap_candidates.push_back({.kind="intersection",.entity_id=a.id,.point_id="",.local_x=a.start_x+t*a_dx,.local_y=a.start_y+t*a_dy,.distance=0,.label="Intersection"});
            }
          }
          // Line-arc intersections
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double dx = line.end_x - line.start_x;
            const double dy = line.end_y - line.start_y;
            const double len_sq = dx * dx + dy * dy;
            if (len_sq < 1e-12) continue;
            for (const auto& arc : sketch.arcs) {
              if (arc.is_construction && !filter.select_construction) continue;
              const double r = std::hypot(arc.start_x - arc.center_x, arc.start_y - arc.center_y);
              const double fx = line.start_x - arc.center_x;
              const double fy = line.start_y - arc.center_y;
              const double a_val = len_sq;
              const double b_val = 2.0 * (fx * dx + fy * dy);
              const double c_val = fx * fx + fy * fy - r * r;
              double disc = b_val * b_val - 4.0 * a_val * c_val;
              if (disc < 0) continue;
              disc = std::sqrt(disc);
              for (double sign : {-1.0, 1.0}) {
                const double t = (-b_val + sign * disc) / (2.0 * a_val);
                if (t < 0.0 || t > 1.0) continue;
                snap_candidates.push_back({.kind="intersection",.entity_id=line.id,.point_id="",.local_x=line.start_x+t*dx,.local_y=line.start_y+t*dy,.distance=0,.label="Intersection"});
              }
            }
          }
        }
        // grid_line, polar, tangent, nearest, and perpendicular are
        // cursor-position-dependent and handled by the TS dynamic snap
        // path — they are not pre-computed here.
        break;
      }
    }
  }

  // Populate DOF statuses for the active sketch.
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        dof_statuses = count_sketch_dof(feat.sketch_parameters.value());
        break;
      }
    }
  }

  for (auto& face : solid_faces) {
    if (!face.appearance_color.has_value()) {
      face.appearance_color =
          face_appearance_color(*view, face.face_id, "semantic:" + face.face_id);
    }
  }

  return ViewportState{
      .has_active_document = true,
      .boxes = boxes,
      .cylinders = cylinders,
      .polygon_extrudes = polygon_extrudes,
      .solid_faces = solid_faces,
      .reference_planes = reference_planes,
      .reference_axes = reference_axes,
      .reference_points = reference_points,
      .helices = helices,
      .sketch_lines = sketch_lines,
      .sketch_circles = sketch_circles,
      .sketch_polygons = sketch_polygons,
      .sketch_arcs = sketch_arcs,
      .sketch_points = sketch_points,
      .sketch_dimensions = sketch_dimensions,
      .sketch_constraints = sketch_constraints,
      .sketch_profiles = sketch_profiles,
      .dof_statuses = dof_statuses,
      .meshes = meshes,
      .cut_previews = cut_previews,
      .bodies = bodies,
      .edges = edges,
      .vertices = vertices,
      .scene_width = scene_width_with_references,
      .scene_height = scene_height_with_references,
      .scene_depth = scene_depth_with_references,
      .scene_bounds = scene_bounds,
      .snap_candidates = snap_candidates,
      .selection_filter = document.has_value()
          ? document->selection_filter
          : SelectionFilter{},
  };
}

}  // namespace polysmith::core
