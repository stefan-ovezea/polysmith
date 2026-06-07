#include "core/face_geometry.h"

#include <algorithm>
#include <cmath>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
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

FaceOutlinePoint to_outline_point(const gp_Pnt& point) {
  return FaceOutlinePoint{.x = point.X(), .y = point.Y(), .z = point.Z()};
}

std::vector<TopoDS_Edge> ordered_wire_edges(const TopoDS_Wire& wire) {
  std::vector<TopoDS_Edge> ordered_edges;
  for (BRepTools_WireExplorer explorer(wire); explorer.More();
       explorer.Next()) {
    ordered_edges.push_back(TopoDS::Edge(explorer.Current()));
  }
  return ordered_edges;
}

std::optional<FaceOutlineCircle> circle_from_edge(const TopoDS_Edge& edge) {
  try {
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Circle) {
      return std::nullopt;
    }
    const gp_Circ circle = curve.Circle();
    const gp_Pnt center = circle.Location();
    const gp_Dir axis = circle.Axis().Direction();
    return FaceOutlineCircle{
        .center = FaceOutlinePoint{
            .x = center.X(), .y = center.Y(), .z = center.Z()},
        .axis = FaceOutlinePoint{.x = axis.X(), .y = axis.Y(), .z = axis.Z()},
        .radius = circle.Radius(),
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<FaceOutlineCircle> circle_from_wire(const TopoDS_Wire& wire) {
  const auto ordered_edges = ordered_wire_edges(wire);
  if (ordered_edges.size() != 1) {
    return std::nullopt;
  }
  return circle_from_edge(ordered_edges.front());
}

std::vector<FaceOutlinePoint> sample_circle_edge_loop(const TopoDS_Edge& edge) {
  std::vector<FaceOutlinePoint> points;
  try {
    const auto circle_outline = circle_from_edge(edge);
    if (!circle_outline.has_value()) {
      return points;
    }
    BRepAdaptor_Curve curve(edge);
    constexpr int kSegments = 64;
    points.reserve(kSegments);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    for (int index = 0; index < kSegments; ++index) {
      const double t =
          first + (last - first) * static_cast<double>(index) /
                      static_cast<double>(kSegments);
      points.push_back(to_outline_point(curve.Value(t)));
    }
  } catch (const std::exception&) {
    points.clear();
  }
  return points;
}

std::vector<FaceOutlinePoint> sample_wire_loop(const TopoDS_Wire& wire) {
  const auto ordered_edges = ordered_wire_edges(wire);
  if (ordered_edges.empty()) {
    return {};
  }

  if (ordered_edges.size() == 1) {
    auto circle_points = sample_circle_edge_loop(ordered_edges.front());
    if (!circle_points.empty()) {
      return circle_points;
    }
  }

  std::vector<FaceOutlinePoint> points;
  points.reserve(ordered_edges.size());
  for (const auto& edge : ordered_edges) {
    if (edge.IsNull()) {
      continue;
    }
    TopoDS_Vertex first_vertex;
    TopoDS_Vertex last_vertex;
    TopExp::Vertices(edge, first_vertex, last_vertex,
                     /*CumOri=*/true);
    if (first_vertex.IsNull()) {
      continue;
    }
    points.push_back(to_outline_point(BRep_Tool::Pnt(first_vertex)));
  }
  return points;
}

std::vector<std::vector<FaceOutlinePoint>> inner_wire_loops(
    const TopoDS_Face& face,
    const TopoDS_Wire& outer_wire) {
  std::vector<std::vector<FaceOutlinePoint>> loops;
  for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More();
       explorer.Next()) {
    const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
    if (wire.IsNull() || wire.IsSame(outer_wire)) {
      continue;
    }
    auto points = sample_wire_loop(wire);
    if (points.size() >= 3) {
      loops.push_back(std::move(points));
    }
  }
  return loops;
}

std::optional<std::vector<FaceOutlineCircle>> inner_wire_circles(
    const TopoDS_Face& face,
    const TopoDS_Wire& outer_wire) {
  std::vector<FaceOutlineCircle> circles;
  for (TopExp_Explorer explorer(face, TopAbs_WIRE); explorer.More();
       explorer.Next()) {
    const TopoDS_Wire wire = TopoDS::Wire(explorer.Current());
    if (wire.IsNull() || wire.IsSame(outer_wire)) {
      continue;
    }
    const auto circle = circle_from_wire(wire);
    if (!circle.has_value()) {
      return std::nullopt;
    }
    circles.push_back(circle.value());
  }
  return circles;
}

std::optional<PlaneFrame> derive_planar_frame(const TopoDS_Face& face) {
  if (face.IsNull()) {
    return std::nullopt;
  }
  try {
    BRepAdaptor_Surface surface(face, false);
    if (surface.GetType() != GeomAbs_Plane) {
      return std::nullopt;
    }
    gp_Pln plane = surface.Plane();
    gp_Ax3 axes = plane.Position();
    gp_Dir x_axis = axes.XDirection();
    gp_Dir y_axis = axes.YDirection();
    gp_Dir normal = axes.Direction();
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
      y_axis.Reverse();
    }
    const gp_Pnt origin = axes.Location();
    return PlaneFrame{
        .origin_x = origin.X(),
        .origin_y = origin.Y(),
        .origin_z = origin.Z(),
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
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

SketchProfilePoint to_local_profile_point(const PlaneFrame& frame,
                                          const FaceOutlinePoint& world) {
  const double dx = world.x - frame.origin_x;
  const double dy = world.y - frame.origin_y;
  const double dz = world.z - frame.origin_z;
  return SketchProfilePoint{
      .x = dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z,
      .y = dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z,
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

// Returns true when `suffix` parses as a non-negative integer — that's
// the body-derived face id format ("<body_id>:face:<index>"), distinct
// from the legacy named suffixes ("top", "base", "side-N", etc.).
bool suffix_is_numeric_index(const std::string& suffix, int& index_out) {
  if (suffix.empty()) {
    return false;
  }
  try {
    size_t consumed = 0;
    const int value = std::stoi(suffix, &consumed);
    if (consumed != suffix.size() || value < 0) {
      return false;
    }
    index_out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// Walk `face`'s outer wire and produce a world-space outline. Detects:
//   - a wire with a single circle edge -> "circle" outline
//   - everything else -> "polygon" outline (line segments only;
//     curved edges are sampled by their two endpoints, which is a
//     reasonable approximation when the user projects a face whose
//     outer wire happens to include arcs).
// Returns nullopt only when the wire couldn't be walked at all.
std::optional<FaceOutline> outline_from_face(const TopoDS_Face& face) {
  TopoDS_Wire outer;
  try {
    outer = BRepTools::OuterWire(face);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (outer.IsNull()) {
    return std::nullopt;
  }

  // Collect edges in order around the wire so the projected polygon's
  // corners follow the face's actual outline.
  const std::vector<TopoDS_Edge> ordered_edges = ordered_wire_edges(outer);
  if (ordered_edges.empty()) {
    return std::nullopt;
  }

  // Circular and annular planar faces should project as sketch circles,
  // not as sampled polygon loops. If every loop is circular, keep that
  // exact semantic shape and let the projector emit one circle per loop.
  if (const auto outer_circle = circle_from_wire(outer);
      outer_circle.has_value()) {
    if (const auto inner_circles = inner_wire_circles(face, outer);
        inner_circles.has_value()) {
      FaceOutline outline{};
      outline.kind = "circle";
      outline.circle_center = outer_circle->center;
      outline.circle_axis = outer_circle->axis;
      outline.circle_radius = outer_circle->radius;
      outline.inner_circles = inner_circles.value();
      return outline;
    }
  }

  FaceOutline outline{};
  outline.kind = "polygon";
  outline.polygon_corners = sample_wire_loop(outer);
  outline.inner_loops = inner_wire_loops(face, outer);

  if (outline.polygon_corners.size() < 3) {
    return std::nullopt;
  }

  return outline;
}

// Resolve a numeric face id ("<body_id>:face:<index>") by recompiling
// bodies, finding the body whose root id matches `body_id`, and walking
// `TopExp::MapShapes(TopAbs_FACE)` to retrieve the requested face. The
// recompile mirrors what the viewport already does — it's the source of
// truth for body topology, so it's also the source of truth for face
// resolution.
std::optional<FaceOutline> outline_for_body_face(
    const DocumentState& document,
    const std::string& body_id,
    int face_index) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    TopTools_IndexedMapOfShape face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return outline_from_face(face);
  }
  return std::nullopt;
}

std::optional<TopoDS_Face> resolve_body_face(const DocumentState& document,
                                             const std::string& body_id,
                                             int face_index) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    TopTools_IndexedMapOfShape face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return face;
  }
  return std::nullopt;
}

std::vector<SketchProfilePoint> to_local_profile_loop(
    const PlaneFrame& frame,
    const std::vector<FaceOutlinePoint>& loop) {
  std::vector<SketchProfilePoint> local;
  local.reserve(loop.size());
  for (const auto& point : loop) {
    local.push_back(to_local_profile_point(frame, point));
  }
  return local;
}

}  // namespace

std::optional<FaceOutline> compute_face_outline(const DocumentState& document,
                                                const std::string& face_id) {
  const auto parsed = parse_face_id(face_id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  // Body-derived face ids ship a numeric index suffix. Resolve those
  // through the OCCT body shape so projections work for booleaned,
  // filleted, chamfered, and plane-frame-rotated faces.
  int face_index = -1;
  if (suffix_is_numeric_index(parsed->suffix, face_index)) {
    return outline_for_body_face(document, parsed->owner_id, face_index);
  }

  // Legacy named-suffix face ids: handled per source feature.
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

std::optional<PlanarFaceProfile> compute_planar_face_profile(
    const DocumentState& document,
    const std::string& face_id) {
  const auto parsed = parse_face_id(face_id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  int face_index = -1;
  if (!suffix_is_numeric_index(parsed->suffix, face_index)) {
    return std::nullopt;
  }

  const auto face = resolve_body_face(document, parsed->owner_id, face_index);
  if (!face.has_value()) {
    return std::nullopt;
  }

  const auto plane_frame = derive_planar_frame(face.value());
  if (!plane_frame.has_value()) {
    return std::nullopt;
  }

  TopoDS_Wire outer;
  try {
    outer = BRepTools::OuterWire(face.value());
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (outer.IsNull()) {
    return std::nullopt;
  }

  const auto outer_points = sample_wire_loop(outer);
  if (outer_points.size() < 3) {
    return std::nullopt;
  }

  PlanarFaceProfile profile{};
  profile.plane_frame = plane_frame.value();
  profile.outer_points = to_local_profile_loop(profile.plane_frame, outer_points);
  for (const auto& loop : inner_wire_loops(face.value(), outer)) {
    if (loop.size() >= 3) {
      profile.inner_loops.push_back(
          to_local_profile_loop(profile.plane_frame, loop));
    }
  }
  return profile;
}

}  // namespace polysmith::core
