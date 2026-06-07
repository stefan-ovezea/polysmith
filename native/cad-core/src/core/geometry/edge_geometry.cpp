#include "core/edge_geometry.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>

#include "core/body_compiler.h"
#include "core/document.h"

namespace polysmith::core {
namespace {

// Shared helper: split "<owner>:<separator>:<index>" — used for both
// edge ids and vertex ids. Returns {owner, index>=0} on success.
struct ParsedTopologyId {
  std::string owner_id;
  int index = -1;
};

std::optional<ParsedTopologyId> parse_topology_id(
    const std::string& id, const std::string& separator) {
  const auto pos = id.find(separator);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  ParsedTopologyId result;
  result.owner_id = id.substr(0, pos);
  try {
    size_t consumed = 0;
    const std::string suffix = id.substr(pos + separator.size());
    const int parsed = std::stoi(suffix, &consumed);
    if (consumed != suffix.size() || parsed < 0) {
      return std::nullopt;
    }
    result.index = parsed;
  } catch (const std::exception&) {
    return std::nullopt;
  }
  return result;
}

// Find the OCCT shape for `body_id` in the freshly recompiled bodies.
// Mirrors the recompile-on-resolve pattern used in face_geometry.cpp.
const polysmith::core::CompiledBody* find_compiled_body(
    const CompiledBodies& compiled, const std::string& body_id) {
  for (const auto& body : compiled.bodies) {
    if (body.id == body_id) {
      return &body;
    }
  }
  return nullptr;
}

EdgePoint to_edge_point(const gp_Pnt& point) {
  return EdgePoint{
      .x = point.X(),
      .y = point.Y(),
      .z = point.Z(),
  };
}

}  // namespace

std::optional<EdgeGeometry> compute_edge_geometry(
    const DocumentState& document, const std::string& edge_id) {
  const auto parsed = parse_topology_id(edge_id, ":edge:");
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
  const auto* body = find_compiled_body(compiled, parsed->owner_id);
  if (body == nullptr || body->shape.IsNull()) {
    return std::nullopt;
  }

  TopTools_IndexedMapOfShape edge_map;
  TopExp::MapShapes(body->shape, TopAbs_EDGE, edge_map);

  const int one_based = parsed->index + 1;
  if (one_based < 1 || one_based > edge_map.Extent()) {
    return std::nullopt;
  }

  const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
  if (edge.IsNull()) {
    return std::nullopt;
  }

  // First / last vertex give us the line endpoints (and the arc
  // boundary for a partial-circle edge).
  TopoDS_Vertex first_vertex;
  TopoDS_Vertex last_vertex;
  TopExp::Vertices(edge, first_vertex, last_vertex, /*CumOri=*/true);
  if (first_vertex.IsNull() || last_vertex.IsNull()) {
    return std::nullopt;
  }
  const gp_Pnt start = BRep_Tool::Pnt(first_vertex);
  const gp_Pnt end_point = BRep_Tool::Pnt(last_vertex);

  EdgeGeometry result{};
  result.start = to_edge_point(start);
  result.end = to_edge_point(end_point);

  try {
    BRepAdaptor_Curve curve(edge);
    const GeomAbs_CurveType curve_type = curve.GetType();
    if (curve_type == GeomAbs_Line) {
      result.kind = "line";
      return result;
    }

    if (curve_type == GeomAbs_Circle) {
      const gp_Circ circle = curve.Circle();
      const gp_Pnt center = circle.Location();
      const gp_Dir axis = circle.Axis().Direction();
      result.center = to_edge_point(center);
      result.axis = EdgePoint{
          .x = axis.X(),
          .y = axis.Y(),
          .z = axis.Z(),
      };
      result.radius = circle.Radius();
      // A closed circular edge collapses both endpoints to the same
      // point — that's how OCCT marks "this curve is a full circle".
      // Distinguish it from a partial arc by comparing endpoint
      // distance against a small tolerance scaled to the radius.
      const double dx = end_point.X() - start.X();
      const double dy = end_point.Y() - start.Y();
      const double dz = end_point.Z() - start.Z();
      const double endpoint_distance =
          std::sqrt(dx * dx + dy * dy + dz * dz);
      const double tolerance = std::max(1e-7, result.radius * 1e-7);
      result.kind = endpoint_distance <= tolerance ? "circle" : "arc";
      return result;
    }
  } catch (const std::exception&) {
    // Fall through to "unsupported".
  }

  result.kind = "unsupported";
  return result;
}

std::optional<EdgePoint> compute_vertex_position(
    const DocumentState& document, const std::string& vertex_id) {
  const auto parsed = parse_topology_id(vertex_id, ":vertex:");
  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document);
  const auto* body = find_compiled_body(compiled, parsed->owner_id);
  if (body == nullptr || body->shape.IsNull()) {
    return std::nullopt;
  }

  TopTools_IndexedMapOfShape vertex_map;
  TopExp::MapShapes(body->shape, TopAbs_VERTEX, vertex_map);

  const int one_based = parsed->index + 1;
  if (one_based < 1 || one_based > vertex_map.Extent()) {
    return std::nullopt;
  }

  const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_map(one_based));
  if (vertex.IsNull()) {
    return std::nullopt;
  }

  return to_edge_point(BRep_Tool::Pnt(vertex));
}

}  // namespace polysmith::core
