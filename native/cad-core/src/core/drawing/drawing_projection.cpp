#include "core/drawing/drawing_projection.h"

#include "core/diagnostics/logger.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <HLRAlgo_EdgeStatus.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_Curve.hxx>
#include <HLRBRep_Data.hxx>
#include <HLRBRep_EdgeData.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

namespace polysmith::core {

namespace {

std::array<double, 2> point2d(const gp_Pnt& p) {
  return {p.X(), p.Y()};
}

std::array<double, 3> point3d(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}

std::array<double, 3> dir3d(const gp_Dir& d) {
  return {d.X(), d.Y(), d.Z()};
}

// View-plane direction (the Z component is ~0 for projected curves).
std::array<double, 2> dir2d(const gp_Dir& d) {
  return {d.X(), d.Y()};
}

const char* curve_kind_name(const GeomAbs_CurveType type) {
  switch (type) {
    case GeomAbs_Line: return "line";
    case GeomAbs_Circle: return "circle";
    case GeomAbs_Ellipse: return "ellipse";
    case GeomAbs_BSplineCurve: return "bspline";
    default: return "other";
  }
}

// 1e-9: the status-interval gap threshold from the P0 spike.
constexpr double kIntervalGap = 1e-9;

// ── Deterministic ordering (the golden-file foundation) ──────────

int line_class_rank(const std::string& cls) {
  if (cls == "visible") return 0;
  if (cls == "hidden") return 1;
  return 2;
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

std::string record_key(const ProjectedEdgeRecord& rec) {
  std::ostringstream os;
  os << std::fixed;
  os.precision(6);
  os << line_class_rank(rec.line_class) << "|" << rec.curve_class << "|"
     << rec.curve_kind << "|";
  if (rec.curve_kind == "line") {
    // Lines: sorted quantised endpoints are order-independent.
    double x0 = quant(rec.p_start[0]);
    double y0 = quant(rec.p_start[1]);
    double x1 = quant(rec.p_end[0]);
    double y1 = quant(rec.p_end[1]);
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    os << x0 << "," << y0 << "|" << x1 << "," << y1;
  } else {
    // Curved records: start | end + the defining geometry (a full
    // circle's start == end, so the endpoints alone under-identify
    // it).
    os << quant(rec.p_start[0]) << "," << quant(rec.p_start[1]) << "|"
       << quant(rec.p_end[0]) << "," << quant(rec.p_end[1]);
    if (rec.circle_center.has_value()) {
      os << "|" << quant(rec.circle_center.value()[0]) << ","
         << quant(rec.circle_center.value()[1]) << ","
         << quant(rec.circle_radius.value());
    } else if (rec.ellipse_center.has_value()) {
      os << "|" << quant(rec.ellipse_center.value()[0]) << ","
         << quant(rec.ellipse_center.value()[1]) << ","
         << quant(rec.ellipse_major_radius.value()) << ","
         << quant(rec.ellipse_minor_radius.value());
    }
  }
  return os.str();
}

/// The source identity for stable sorting: body id + edge index.
/// An empty body id means the witness is unresolved (HLR-derived
/// edges that match no body geometry — chord segments etc.).
std::string source_identity(const ProjectedEdgeRecord& rec) {
  const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
  if (witness == nullptr) {
    return "outline";
  }
  if (witness->body_id.empty()) {
    return "unresolved";
  }
  return witness->body_id + "#" + std::to_string(witness->src_edge_index);
}

// Curve-geometry equality within tolerance — matches HLR's split
// copies of an edge against the original body edges (a split circle
// arc shares the original circle's center/axis/radius, a split line
// lies on the original line).
constexpr double kMatchTolerance = 1e-7;

bool same_curve_geometry(const BRepAdaptor_Curve& a,
                         const BRepAdaptor_Curve& b) {
  if (a.GetType() != b.GetType()) {
    return false;
  }
  switch (a.GetType()) {
    case GeomAbs_Line: {
      const gp_Lin la = a.Line();
      const gp_Lin lb = b.Line();
      if (std::abs(la.Direction().Dot(lb.Direction())) <
          1.0 - kMatchTolerance) {
        return false;
      }
      // Perpendicular distance from lb's location to la (a split
      // line's location is a point ON the original line).
      const gp_Vec offset(lb.Location(), la.Location());
      const gp_Vec cross =
          offset.Crossed(gp_Vec(la.Direction()));
      return cross.Magnitude() < kMatchTolerance;
    }
    case GeomAbs_Circle: {
      const gp_Circ ca = a.Circle();
      const gp_Circ cb = b.Circle();
      return ca.Location().Distance(cb.Location()) < kMatchTolerance &&
             std::abs(ca.Radius() - cb.Radius()) < kMatchTolerance &&
             std::abs(ca.Axis().Direction().Dot(cb.Axis().Direction())) >
                 1.0 - kMatchTolerance;
    }
    case GeomAbs_Ellipse: {
      const gp_Elips ea = a.Ellipse();
      const gp_Elips eb = b.Ellipse();
      return ea.Location().Distance(eb.Location()) < kMatchTolerance &&
             std::abs(ea.MajorRadius() - eb.MajorRadius()) <
                 kMatchTolerance &&
             std::abs(ea.MinorRadius() - eb.MinorRadius()) <
                 kMatchTolerance &&
             std::abs(ea.Axis().Direction().Dot(eb.Axis().Direction())) >
                 1.0 - kMatchTolerance;
    }
    default:
      return false;  // splines are not matched — honest "unresolved"
  }
}

// ── Section cut + hatch boundary helpers (P4) ─────────────────────

// Tessellates a curve edge into sample points (lines take their two
// endpoints; curved edges are sampled — the hatch boundary is drawn,
// never dimensioned, so a polygon is exact enough).
void append_edge_points(const TopoDS_Edge& edge, std::vector<gp_Pnt>& points) {
  BRepAdaptor_Curve curve(edge);
  const double first = curve.FirstParameter();
  const double last = curve.LastParameter();
  const int samples = curve.GetType() == GeomAbs_Line ? 1 : 48;
  for (int i = 0; i <= samples; ++i) {
    points.push_back(curve.Value(first + (last - first) * i / samples));
  }
}

double signed_area(const std::vector<std::array<double, 2>>& loop) {
  double area = 0.0;
  for (size_t i = 0; i < loop.size(); ++i) {
    const auto& p = loop[i];
    const auto& q = loop[(i + 1) % loop.size()];
    area += p[0] * q[1] - q[0] * p[1];
  }
  return area / 2.0;
}

// Chains projected polyline edges into closed loops by endpoint
// proximity (1e-6).  The section edges of one body form disjoint
// loops (outer + holes) — chaining is purely geometric; no topology
// assumed beyond connectivity.
std::vector<std::vector<std::array<double, 2>>> chain_into_loops(
    std::vector<std::vector<std::array<double, 2>>> chains) {
  const auto same_point = [](const std::array<double, 2>& a,
                             const std::array<double, 2>& b) {
    return std::abs(a[0] - b[0]) < 1e-6 && std::abs(a[1] - b[1]) < 1e-6;
  };
  bool merged = true;
  while (merged && chains.size() > 1) {
    merged = false;
    for (size_t i = 0; i < chains.size() && !merged; ++i) {
      for (size_t j = i + 1; j < chains.size(); ++j) {
        std::vector<std::array<double, 2>> joined;
        if (same_point(chains[i].back(), chains[j].front())) {
          joined = chains[i];
          joined.insert(joined.end(), chains[j].begin() + 1, chains[j].end());
        } else if (same_point(chains[i].back(), chains[j].back())) {
          joined = chains[i];
          joined.insert(joined.end(), chains[j].rbegin() + 1, chains[j].rend());
        } else if (same_point(chains[i].front(), chains[j].back())) {
          joined = chains[j];
          joined.insert(joined.end(), chains[i].begin() + 1, chains[i].end());
        } else if (same_point(chains[i].front(), chains[j].front())) {
          joined.assign(chains[j].rbegin(), chains[j].rend() - 1);
          joined.insert(joined.end(), chains[i].begin(), chains[i].end());
        } else {
          continue;
        }
        chains[i] = std::move(joined);
        chains.erase(chains.begin() + static_cast<std::ptrdiff_t>(j));
        merged = true;
        break;
      }
    }
  }
  for (auto& loop : chains) {
    if (loop.size() >= 3 && same_point(loop.front(), loop.back())) {
      loop.pop_back();
    }
  }
  return chains;
}

// The hatch boundary of a section view: for every source body, the
// planar faces of the cut shape that lie ON the cutting plane, each
// face contributing one HatchRegion (its outer wire + hole wires).
// Wires are projected to view space — for section views the cutting
// plane IS the view plane, so the polygon is the exact cross-section.
std::vector<HatchRegion> build_hatch_regions(
    const std::vector<SourceBody>& sources, const gp_Pln& plane,
    const HLRAlgo_Projector& projector) {
  constexpr double kPlaneTolerance = 1e-6;
  std::vector<HatchRegion> regions;
  for (const auto& source : sources) {
    for (TopExp_Explorer fex(source.shape, TopAbs_FACE); fex.More();
         fex.Next()) {
      const TopoDS_Face face = TopoDS::Face(fex.Current());
      BRepAdaptor_Surface surface(face);
      if (surface.GetType() != GeomAbs_Plane) {
        continue;
      }
      const gp_Pln face_plane = surface.Plane();
      if (face_plane.Distance(plane.Location()) > kPlaneTolerance) {
        continue;
      }
      if (std::abs(face_plane.Axis().Direction().Dot(
              plane.Axis().Direction())) < 1.0 - 1e-9) {
        continue;
      }

      // Project the face's wire edges to polylines and chain them
      // into loops GEOMETRICALLY: boolean output repeats edges inside
      // a wire (both directions) and BRepTools_WireExplorer order is
      // not reliable on it.  Unique edges by TShape, then endpoint
      // chaining.  The largest |area| loop is the outer boundary.
      std::vector<std::vector<std::array<double, 2>>> chains;
      for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
        std::vector<TopoDS_Edge> unique_edges;
        for (TopExp_Explorer eex(wex.Current(), TopAbs_EDGE); eex.More();
             eex.Next()) {
          const TopoDS_Edge edge = TopoDS::Edge(eex.Current());
          const bool seen = std::any_of(
              unique_edges.begin(), unique_edges.end(),
              [&](const TopoDS_Edge& other) { return other.IsSame(edge); });
          if (!seen) {
            unique_edges.push_back(edge);
          }
        }
        for (const auto& edge : unique_edges) {
          std::vector<gp_Pnt> points_3d;
          append_edge_points(edge, points_3d);
          std::vector<std::array<double, 2>> chain;
          for (const gp_Pnt& p : points_3d) {
            gp_Pnt2d projected;
            projector.Project(p, projected);
            if (!std::isfinite(projected.X()) ||
                !std::isfinite(projected.Y())) {
              continue;
            }
            const std::array<double, 2> q = {projected.X(), projected.Y()};
            if (chain.empty() ||
                std::hypot(chain.back()[0] - q[0],
                           chain.back()[1] - q[1]) > 1e-7) {
              chain.push_back(q);
            }
          }
          if (chain.size() >= 2) {
            chains.push_back(std::move(chain));
          }
        }
      }
      const auto loops = chain_into_loops(std::move(chains));
      if (loops.empty()) {
        continue;
      }
      size_t outer_index = 0;
      double best_area = 0.0;
      for (size_t i = 0; i < loops.size(); ++i) {
        const double area = std::abs(signed_area(loops[i]));
        if (i == 0 || area > best_area) {
          best_area = area;
          outer_index = i;
        }
      }
      HatchRegion region;
      region.outer_loop = loops[outer_index];
      for (size_t i = 0; i < loops.size(); ++i) {
        if (i != outer_index) {
          region.holes.push_back(loops[i]);
        }
      }
      regions.push_back(std::move(region));
    }
  }
  // Deterministic region order (golden foundation): outer-loop
  // bounding-box minimum x, then y.
  std::sort(regions.begin(), regions.end(),
            [](const HatchRegion& a, const HatchRegion& b) {
              auto bbox_min = [](const HatchRegion& r) {
                std::array<double, 2> m = {0.0, 0.0};
                if (r.outer_loop.empty()) {
                  return m;
                }
                m = r.outer_loop[0];
                for (const auto& p : r.outer_loop) {
                  m[0] = std::min(m[0], p[0]);
                  m[1] = std::min(m[1], p[1]);
                }
                return m;
              };
              const auto ma = bbox_min(a);
              const auto mb = bbox_min(b);
              if (ma[0] != mb[0]) {
                return ma[0] < mb[0];
              }
              return ma[1] < mb[1];
            });
  return regions;
}

// The hatch boundary for a section-only (cut_away = false) view: the
// uncut body's cross-section wires at the cutting plane, tessellated
// and projected.  The cut path above uses cut-face wires instead —
// both produce the same HatchRegion shape.
HatchRegion hatch_region_from_section_shape(
    const TopoDS_Shape& section_shape, const HLRAlgo_Projector& projector) {
  std::vector<std::vector<std::array<double, 2>>> chains;
  for (TopExp_Explorer eex(section_shape, TopAbs_EDGE); eex.More();
       eex.Next()) {
    std::vector<gp_Pnt> points_3d;
    append_edge_points(TopoDS::Edge(eex.Current()), points_3d);
    std::vector<std::array<double, 2>> points;
    for (const gp_Pnt& p : points_3d) {
      gp_Pnt2d projected;
      projector.Project(p, projected);
      if (!std::isfinite(projected.X()) || !std::isfinite(projected.Y())) {
        continue;
      }
      const std::array<double, 2> q = {projected.X(), projected.Y()};
      if (points.empty() ||
          std::hypot(points.back()[0] - q[0], points.back()[1] - q[1]) >
              1e-7) {
        points.push_back(q);
      }
    }
    if (points.size() >= 2) {
      chains.push_back(std::move(points));
    }
  }
  const auto loops = chain_into_loops(std::move(chains));
  HatchRegion region;
  if (loops.empty()) {
    return region;
  }
  size_t outer_index = 0;
  double best_area = 0.0;
  for (size_t i = 0; i < loops.size(); ++i) {
    const double area = std::abs(signed_area(loops[i]));
    if (i == 0 || area > best_area) {
      best_area = area;
      outer_index = i;
    }
  }
  region.outer_loop = loops[outer_index];
  for (size_t i = 0; i < loops.size(); ++i) {
    if (i != outer_index) {
      region.holes.push_back(loops[i]);
    }
  }
  return region;
}

}  // namespace

// ── Source edge witness ───────────────────────────────────────────

int match_body_edge(const TopoDS_Shape& body_shape,
                    const TopoDS_Edge& mapped_edge) {
  if (body_shape.IsNull() || mapped_edge.IsNull()) {
    return -1;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  BRepAdaptor_Curve mapped_curve(mapped_edge);

  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge candidate = TopoDS::Edge(edge_map(i));
    if (candidate.IsSame(mapped_edge)) {
      return i - 1;  // 0-based, the capture_edge_reference convention
    }
    BRepAdaptor_Curve candidate_curve(candidate);
    if (same_curve_geometry(mapped_curve, candidate_curve)) {
      return i - 1;
    }
  }
  return -1;
}

std::optional<SourceEdgeWitness> build_source_edge_witness(
    const std::string& body_id, const TopoDS_Shape& body_shape,
    int edge_index) {
  if (body_shape.IsNull() || edge_index < 0) {
    return std::nullopt;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
  const int one_based = edge_index + 1;
  if (one_based < 1 || one_based > edge_map.Extent()) {
    return std::nullopt;
  }
  const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
  if (edge.IsNull()) {
    return std::nullopt;
  }

  TopoDS_Vertex first_vertex;
  TopoDS_Vertex last_vertex;
  TopExp::Vertices(edge, first_vertex, last_vertex, /*CumOri=*/true);
  if (first_vertex.IsNull() || last_vertex.IsNull()) {
    return std::nullopt;
  }
  const gp_Pnt start = BRep_Tool::Pnt(first_vertex);
  const gp_Pnt end = BRep_Tool::Pnt(last_vertex);

  SourceEdgeWitness witness;
  witness.body_id = body_id;
  witness.src_edge_index = edge_index;
  witness.start_point = point3d(start);
  witness.end_point = point3d(end);
  const double dx = end.X() - start.X();
  const double dy = end.Y() - start.Y();
  const double dz = end.Z() - start.Z();
  witness.length = std::sqrt(dx * dx + dy * dy + dz * dz);

  BRepAdaptor_Curve curve(edge);
  witness.curve_kind = curve_kind_name(curve.GetType());
  if (curve.GetType() == GeomAbs_Line && witness.length > 1e-9) {
    witness.tangent = {dx / witness.length, dy / witness.length,
                       dz / witness.length};
  }
  // Circle witness for circles AND arcs (arcs keep their endpoints,
  // so the endpoint witness plus center/axis/radius identifies them;
  // full circles have start == end and need the circle witness).
  if (curve.GetType() == GeomAbs_Circle) {
    const gp_Pnt center = curve.Circle().Location();
    const gp_Dir axis = curve.Circle().Axis().Direction();
    witness.center = point3d(center);
    witness.axis = dir3d(axis);
    witness.radius = curve.Circle().Radius();
  }
  // The status intervals of the HLR entry live in the SOURCE edge's
  // parameter space (the P0 spike finding), so the witness's param
  // range is the source curve's own range.
  witness.param_range = {curve.FirstParameter(), curve.LastParameter()};
  return witness;
}

// ── Standard view frames ──────────────────────────────────────────

std::optional<DrawingViewFrame> standard_view_frame(
    const std::string& standard_view) {
  DrawingViewFrame frame;
  frame.origin = {0.0, 0.0, 0.0};
  // See the header table: every frame keeps view-Y = normal ×
  // x_direction = +Z and matches first-angle placement semantics.
  if (standard_view == "front") {
    frame.normal = {1.0, 0.0, 0.0};
    frame.x_direction = {0.0, 1.0, 0.0};
  } else if (standard_view == "right") {
    frame.normal = {0.0, 1.0, 0.0};
    frame.x_direction = {-1.0, 0.0, 0.0};
  } else if (standard_view == "left") {
    frame.normal = {0.0, -1.0, 0.0};
    frame.x_direction = {1.0, 0.0, 0.0};
  } else if (standard_view == "top") {
    frame.normal = {0.0, 0.0, 1.0};
    frame.x_direction = {0.0, 1.0, 0.0};
  } else if (standard_view == "bottom") {
    frame.normal = {0.0, 0.0, -1.0};
    frame.x_direction = {0.0, -1.0, 0.0};
  } else if (standard_view == "back") {
    frame.normal = {-1.0, 0.0, 0.0};
    frame.x_direction = {0.0, -1.0, 0.0};
  } else {
    return std::nullopt;
  }
  return frame;
}

std::optional<DrawingViewFrame> resolve_view_frame(const DrawingView& view) {
  if (view.kind == "section") {
    if (!view.section.has_value()) {
      return std::nullopt;  // no definition at all
    }
    // The view plane IS the cutting plane; view-X is the orthogonal
    // projection of world +X onto the plane (fallbacks +Y, +Z — with
    // a unit normal the last fallback always resolves).
    DrawingViewFrame frame;
    frame.origin = view.section->cutting_plane_point;
    frame.normal = view.section->cutting_plane_normal;
    const auto project_onto_plane = [&](const std::array<double, 3>& axis) {
      const double dot = axis[0] * frame.normal[0] + axis[1] * frame.normal[1] +
                         axis[2] * frame.normal[2];
      return std::array<double, 3>{axis[0] - dot * frame.normal[0],
                                   axis[1] - dot * frame.normal[1],
                                   axis[2] - dot * frame.normal[2]};
    };
    std::array<double, 3> x_axis = project_onto_plane({1.0, 0.0, 0.0});
    double x_length = std::sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1] +
                                x_axis[2] * x_axis[2]);
    if (x_length < 1e-9) {
      x_axis = project_onto_plane({0.0, 1.0, 0.0});
      x_length = std::sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1] +
                           x_axis[2] * x_axis[2]);
    }
    if (x_length < 1e-9) {
      x_axis = project_onto_plane({0.0, 0.0, 1.0});
      x_length = std::sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1] +
                           x_axis[2] * x_axis[2]);
    }
    if (x_length < 1e-9) {
      return std::nullopt;  // degenerate normal
    }
    frame.x_direction = {x_axis[0] / x_length, x_axis[1] / x_length,
                         x_axis[2] / x_length};
    return frame;
  }
  if (!view.standard_view.empty()) {
    return standard_view_frame(view.standard_view);
  }
  return view.custom_frame;
}

// ── The projection itself ─────────────────────────────────────────

ProjectionResult project(const ProjectionInput& input) {
  ProjectionResult result;
  result.source_revision = input.source_revision;

  // For section views the cutting plane IS the view plane, which puts
  // the cut face exactly AT HLR's projection plane (depth 0) — the
  // degenerate case where HLR's occlusion drops parts of the cut face
  // (proven by the hole-box test: a rim circle and one outer edge
  // vanished).  Shift the frame origin a hair back along the view
  // direction: the projected x/y coordinates are unchanged (the shift
  // is parallel to the view direction), but the cut face now sits a
  // hair in front of the eye plane and occludes normally.
  DrawingViewFrame effective_frame = input.frame;
  if (input.section.has_value()) {
    constexpr double kEyeOffsetMm = 1e-3;
    effective_frame.origin = {
        input.frame.origin[0] - input.frame.normal[0] * kEyeOffsetMm,
        input.frame.origin[1] - input.frame.normal[1] * kEyeOffsetMm,
        input.frame.origin[2] - input.frame.normal[2] * kEyeOffsetMm};
  }

  const gp_Ax2 frame(
      gp_Pnt(effective_frame.origin[0], effective_frame.origin[1],
             effective_frame.origin[2]),
      gp_Dir(input.frame.normal[0], input.frame.normal[1],
             input.frame.normal[2]),
      gp_Dir(input.frame.x_direction[0], input.frame.x_direction[1],
             input.frame.x_direction[2]));

  const HLRAlgo_Projector projector(frame);

  // ── Section pre-pass (P4) ─────────────────────────────────────
  // A section view cuts every source with a half-space (cut_away:
  // material on the cutting plane's normal side is removed), and
  // hidden edges are suppressed entirely (ISO 128-3 §7: hidden edges
  // are not drawn on sectioned parts).
  const bool is_section_view = input.section.has_value();
  std::vector<SourceBody> projected_sources = input.sources;
  gp_Pln cutting_plane(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
  if (is_section_view) {
    const auto& section = input.section.value();
    cutting_plane = gp_Pln(
        gp_Pnt(section.cutting_plane_point[0], section.cutting_plane_point[1],
               section.cutting_plane_point[2]),
        gp_Dir(section.cutting_plane_normal[0], section.cutting_plane_normal[1],
               section.cutting_plane_normal[2]));
    if (section.cut_away) {
      projected_sources.clear();
      const TopoDS_Face plane_face =
          BRepBuilderAPI_MakeFace(cutting_plane).Face();
      // This OCCT build's MakeHalfSpace(face, ref) treats the
      // reference point as OUTSIDE the material: the halfspace keeps
      // the side OPPOSITE the ref (verified empirically — the probe
      // in cad_core_drawing_section_test pins the direction).  The
      // ref sits on the +normal side, so the cut removes the normal
      // side (the cut_away semantics).
      const gp_Pnt ref_point(
          section.cutting_plane_point[0] + section.cutting_plane_normal[0],
          section.cutting_plane_point[1] + section.cutting_plane_normal[1],
          section.cutting_plane_point[2] + section.cutting_plane_normal[2]);
      const TopoDS_Shape half_space =
          BRepPrimAPI_MakeHalfSpace(plane_face, ref_point).Solid();
      for (const auto& source : input.sources) {
        BRepAlgoAPI_Cut cut(source.shape, half_space);
        if (!cut.IsDone()) {
          polysmith::core::log_warn(
              "drawing", "the section cut failed for body '" +
                             source.body_id +
                             "' — projecting the uncut body");
          projected_sources.push_back(source);
        } else {
          projected_sources.push_back({source.body_id, cut.Shape()});
        }
      }
      result.hatch_regions =
          build_hatch_regions(projected_sources, cutting_plane, projector);
    } else {
      // Section-only: the body is NOT cut; the hatch boundary is the
      // body's cross-section at the cutting plane.
      const TopoDS_Face plane_face =
          BRepBuilderAPI_MakeFace(cutting_plane).Face();
      for (const auto& source : input.sources) {
        BRepAlgoAPI_Section section_op(source.shape, plane_face);
        if (!section_op.IsDone()) {
          continue;
        }
        HatchRegion region = hatch_region_from_section_shape(
            section_op.Shape(), projector);
        if (!region.outer_loop.empty()) {
          result.hatch_regions.push_back(std::move(region));
        }
      }
    }
  }
  const bool emit_hidden = input.show_hidden && !is_section_view;

  occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
  for (const auto& source : projected_sources) {
    algo->Add(source.shape, /*nbIso=*/0);
  }
  algo->Projector(projector);
  algo->Update();
  algo->Hide();

  const occ::handle<HLRBRep_Data> ds = algo->DataStructure();
  auto& edata = ds->EDataArray();  // non-const: Status() is non-const

  for (int i = 1; i <= ds->NbEdges(); ++i) {
    const HLRBRep_Curve& gc = edata(i).Geometry();
    HLRAlgo_EdgeStatus& st = edata(i).Status();
    const TopoDS_Shape& mapped = ds->EdgeMap()(i);
    const HLRBRep_EdgeData& ed = edata(i);

    const bool has_source =
        !mapped.IsNull() && mapped.ShapeType() == TopAbs_EDGE;
    const std::string curve_class = !has_source      ? "outline"
                                    : ed.RgNLine()   ? "seam"
                                    : ed.Rg1Line()   ? "smooth"
                                                     : "sharp";

    // Parts to emit: (line class, [start, end]) in source-parameter
    // space.
    std::vector<std::pair<std::string, std::pair<double, double>>> parts;
    double bounds_start, bounds_end;
    float ts, te;
    st.Bounds(bounds_start, ts, bounds_end, te);
    if (st.AllHidden()) {
      if (emit_hidden) {
        parts.push_back({"hidden", {bounds_start, bounds_end}});
      }
    } else {
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        parts.push_back({"visible", {s, e}});
      }
      if (emit_hidden) {
        // Hidden complement (partially hidden edges): gaps between
        // the visible parts, bounded by the edge bounds.  Fully
        // visible edges contribute no hidden part.
        std::vector<std::pair<double, double>> gaps;
        double cursor = bounds_start;
        for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
          double s, e;
          st.VisiblePart(vp, s, ts, e, te);
          if (s - cursor > kIntervalGap) gaps.push_back({cursor, s});
          cursor = e;
        }
        if (bounds_end - cursor > kIntervalGap) {
          gaps.push_back({cursor, bounds_end});
        }
        for (const auto& gap : gaps) {
          parts.push_back({"hidden", gap});
        }
      }
    }

    for (const auto& part : parts) {
      const TopoDS_Edge piece =
          HLRBRep::MakeEdge(gc, part.second.first, part.second.second);
      if (piece.IsNull()) continue;

      ProjectedEdgeRecord rec;
      rec.line_class = part.first;
      rec.curve_class = curve_class;
      if (has_source) {
        const TopoDS_Edge mapped_edge = TopoDS::Edge(mapped);
        // The mapped edge may be an HLR split copy — match it back
        // to the owning body by IsSame, then curve geometry.
        for (const auto& source : input.sources) {
          const int edge_index =
              match_body_edge(source.shape, mapped_edge);
          if (edge_index >= 0) {
            const auto witness = build_source_edge_witness(
                source.body_id, source.shape, edge_index);
            if (witness.has_value()) {
              rec.source = witness.value();
            }
            break;
          }
        }
      }
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        rec.visible_intervals.push_back({s, e});
      }

      BRepAdaptor_Curve out(piece);
      rec.curve_kind = curve_kind_name(out.GetType());
      rec.first_param = out.FirstParameter();
      rec.last_param = out.LastParameter();
      const gp_Pnt q0 = out.Value(rec.first_param);
      const gp_Pnt q1 = out.Value(rec.last_param);
      // Depth edges (parallel to the view direction) project to
      // degenerate, un-drawable pieces with NaN coordinates — filter
      // them (the P0 spike found them landing hidden; the production
      // engine drops what cannot be drawn).  Full circles/ellipses
      // legitimately have start == end (the closed loop) — they are
      // NOT degenerate.
      const bool closed_loop =
          (out.GetType() == GeomAbs_Circle &&
           out.Circle().Radius() > 1e-9) ||
          (out.GetType() == GeomAbs_Ellipse &&
           out.Ellipse().MajorRadius() > 1e-9);
      if (!std::isfinite(q0.X()) || !std::isfinite(q0.Y()) ||
          !std::isfinite(q1.X()) || !std::isfinite(q1.Y()) ||
          (!closed_loop && q0.Distance(q1) < 1e-9)) {
        continue;
      }
      rec.p_start = point2d(q0);
      rec.p_end = point2d(q1);
      // Renderable curve geometry: circles/ellipses carry their
      // center/radii so the flattened sheet stream can draw exact
      // arcs.  For circle/ellipse parameterizations the parameters
      // ARE angles.
      if (out.GetType() == GeomAbs_Circle) {
        const gp_Circ circle = out.Circle();
        rec.circle_center = point2d(circle.Location());
        rec.circle_radius = circle.Radius();
        rec.start_angle = rec.first_param;
        rec.end_angle = rec.last_param;
      } else if (out.GetType() == GeomAbs_Ellipse) {
        const gp_Elips ellipse = out.Ellipse();
        rec.ellipse_center = point2d(ellipse.Location());
        rec.ellipse_major_dir = dir2d(ellipse.XAxis().Direction());
        rec.ellipse_major_radius = ellipse.MajorRadius();
        rec.ellipse_minor_radius = ellipse.MinorRadius();
        rec.start_angle = rec.first_param;
        rec.end_angle = rec.last_param;
      }
      result.edges.push_back(std::move(rec));
    }
  }

  // Silhouettes: extracted from OutLineVCompound (the face-wire path —
  // outline curves are not edge entries).  They carry no source edge;
  // the record carries a body-level FaceAttestation (bounding box of
  // the owning body + the view normal; area/sample_points stay
  // unresolved until dimensions on silhouettes land — P6 upgrades
  // this to a real face attestation).
  HLRBRep_HLRToShape to_shape(algo);
  TopoDS_Shape outline_compound = to_shape.OutLineVCompound();
  BRepLib::SameParameter(outline_compound, Precision::PConfusion(), false);
  for (TopExp_Explorer ex(outline_compound, TopAbs_EDGE); ex.More();
       ex.Next()) {
    const TopoDS_Edge& out_edge = TopoDS::Edge(ex.Current());

    ProjectedEdgeRecord rec;
    rec.line_class = "visible";
    rec.curve_class = "outline";
    // Body-level attestation: silhouettes belong to whichever source
    // body was added — with a single source the owner is unambiguous.
    if (input.sources.size() == 1) {
      FaceAttestation attestation;
      Bnd_Box box;
      BRepBndLib::Add(input.sources[0].shape, box);
      if (!box.IsVoid()) {
        box.Get(attestation.bounds.min_x, attestation.bounds.min_y,
                attestation.bounds.min_z, attestation.bounds.max_x,
                attestation.bounds.max_y, attestation.bounds.max_z);
      }
      attestation.normal = input.frame.normal;
      rec.source = attestation;
    } else {
      // Multi-body: the OutLineVCompound gives no face ownership —
      // leave the attestation unresolved rather than guess.
      FaceAttestation attestation;
      attestation.normal = input.frame.normal;
      rec.source = attestation;
    }

    BRepAdaptor_Curve out(out_edge);
    rec.curve_kind = curve_kind_name(out.GetType());
    rec.first_param = out.FirstParameter();
    rec.last_param = out.LastParameter();
    const gp_Pnt q0 = out.Value(rec.first_param);
    const gp_Pnt q1 = out.Value(rec.last_param);
    rec.p_start = point2d(q0);
    rec.p_end = point2d(q1);
    if (out.GetType() == GeomAbs_Circle) {
      const gp_Circ circle = out.Circle();
      rec.circle_center = point2d(circle.Location());
      rec.circle_radius = circle.Radius();
      rec.start_angle = rec.first_param;
      rec.end_angle = rec.last_param;
    } else if (out.GetType() == GeomAbs_Ellipse) {
      const gp_Elips ellipse = out.Ellipse();
      rec.ellipse_center = point2d(ellipse.Location());
      rec.ellipse_major_dir = dir2d(ellipse.XAxis().Direction());
      rec.ellipse_major_radius = ellipse.MajorRadius();
      rec.ellipse_minor_radius = ellipse.MinorRadius();
      rec.start_angle = rec.first_param;
      rec.end_angle = rec.last_param;
    }
    result.edges.push_back(std::move(rec));
  }

  // ── Cutting-plane traces (type-H chain lines) ─────────────────
  // A sibling section's cutting plane is drawn on every view that
  // sees it EDGE-ON (view normal ⊥ plane normal): the plane's trace
  // through each source body.  The section wires all collapse onto
  // the trace line — the record spans the projected extremes and the
  // de-dupe pass collapses overlaps between the wires.
  for (const auto& trace : input.section_traces) {
    const auto& section = trace.section;
    const double alignment = std::abs(
        input.frame.normal[0] * section.cutting_plane_normal[0] +
        input.frame.normal[1] * section.cutting_plane_normal[1] +
        input.frame.normal[2] * section.cutting_plane_normal[2]);
    if (alignment > 1e-6) {
      continue;  // not edge-on — no trace on this view
    }
    const gp_Pln trace_plane(
        gp_Pnt(section.cutting_plane_point[0], section.cutting_plane_point[1],
               section.cutting_plane_point[2]),
        gp_Dir(section.cutting_plane_normal[0], section.cutting_plane_normal[1],
               section.cutting_plane_normal[2]));
    const TopoDS_Face plane_face =
        BRepBuilderAPI_MakeFace(trace_plane).Face();
    for (const auto& source : input.sources) {
      BRepAlgoAPI_Section section_op(source.shape, plane_face);
      if (!section_op.IsDone()) {
        continue;
      }
      std::vector<std::array<double, 2>> points;
      for (TopExp_Explorer eex(section_op.Shape(), TopAbs_EDGE); eex.More();
           eex.Next()) {
        std::vector<gp_Pnt> points_3d;
        append_edge_points(TopoDS::Edge(eex.Current()), points_3d);
        for (const gp_Pnt& p : points_3d) {
          gp_Pnt2d projected;
          projector.Project(p, projected);
          if (!std::isfinite(projected.X()) ||
              !std::isfinite(projected.Y())) {
            continue;
          }
          points.push_back({projected.X(), projected.Y()});
        }
      }
      size_t best_i = 0;
      size_t best_j = 0;
      double best_distance = 0.0;
      for (size_t i = 0; i < points.size(); ++i) {
        for (size_t j = i + 1; j < points.size(); ++j) {
          const double distance = std::hypot(
              points[i][0] - points[j][0], points[i][1] - points[j][1]);
          if (distance > best_distance) {
            best_distance = distance;
            best_i = i;
            best_j = j;
          }
        }
      }
      if (best_distance < 1e-9) {
        continue;  // the trace degenerates to a point on this view
      }
      ProjectedEdgeRecord rec;
      rec.line_class = "visible";
      rec.curve_class = "cutting_plane";
      rec.curve_kind = "line";
      rec.p_start = points[best_i];
      rec.p_end = points[best_j];
      // Derived geometry: no model-edge provenance (the default
      // witness is the "unresolved" identity); the chain line ties to
      // its section through the label.  The sight direction drives
      // the A–A arrows: the section viewer sits on the plane's
      // NORMAL side (that side is cut away) looking along −normal.
      rec.section_label = section.label;
      {
        const gp_Pnt base(section.cutting_plane_point[0],
                          section.cutting_plane_point[1],
                          section.cutting_plane_point[2]);
        const gp_Pnt tip(
            base.X() - section.cutting_plane_normal[0],
            base.Y() - section.cutting_plane_normal[1],
            base.Z() - section.cutting_plane_normal[2]);
        gp_Pnt2d base_2d, tip_2d;
        projector.Project(base, base_2d);
        projector.Project(tip, tip_2d);
        const double dx = tip_2d.X() - base_2d.X();
        const double dy = tip_2d.Y() - base_2d.Y();
        const double length = std::hypot(dx, dy);
        if (length > 1e-9) {
          rec.trace_sight_dir = std::array<double, 2>{dx / length,
                                                      dy / length};
        }
      }
      result.edges.push_back(std::move(rec));
    }
  }

  // Deterministic order: line class rank, curve class, curve kind,
  // geometry signature, source identity.  Records that are exact
  // duplicates (same geometry AND same source — the explorer-revisit
  // case) collapse to one.
  std::sort(result.edges.begin(), result.edges.end(),
            [](const ProjectedEdgeRecord& a, const ProjectedEdgeRecord& b) {
              if (line_class_rank(a.line_class) !=
                  line_class_rank(b.line_class)) {
                return line_class_rank(a.line_class) <
                       line_class_rank(b.line_class);
              }
              if (a.curve_class != b.curve_class) {
                return a.curve_class < b.curve_class;
              }
              const std::string ka = record_key(a);
              const std::string kb = record_key(b);
              if (ka != kb) {
                return ka < kb;
              }
              return source_identity(a) < source_identity(b);
            });
  result.edges.erase(
      std::unique(result.edges.begin(), result.edges.end(),
                  [](const ProjectedEdgeRecord& a,
                     const ProjectedEdgeRecord& b) {
                    return record_key(a) == record_key(b) &&
                           source_identity(a) == source_identity(b);
                  }),
      result.edges.end());

  return result;
}

// ── Scanline hatching (ISO 128-3 §7) ──────────────────────────────

constexpr double kPi = 3.14159265358979323846;

// Clips hatch scanlines at `angle_deg` / `spacing_mm` (model mm at
// 1:1 — callers scale the result with the view) to a hatch region:
// rotate the loops by -angle so the scanlines are horizontal, clip
// against the polygon with the even-odd rule across outer loop +
// holes, rotate the segments back.  Pure geometry, no OCCT — the
// flatten (P5) and every export backend reuse this one function so
// hatching is identical everywhere.
std::vector<std::array<std::array<double, 2>, 2>> compute_hatch_segments(
    const HatchRegion& region, double angle_deg, double spacing_mm) {
  std::vector<std::array<std::array<double, 2>, 2>> segments;
  if (region.outer_loop.size() < 3 || spacing_mm <= 0.0) {
    return segments;
  }
  const double angle = angle_deg * kPi / 180.0;
  const double ca = std::cos(angle);
  const double sa = std::sin(angle);
  const auto rotate = [&](const std::array<double, 2>& p) {
    return std::array<double, 2>{p[0] * ca - p[1] * sa,
                                 p[0] * sa + p[1] * ca};
  };
  const auto unrotate = [&](const std::array<double, 2>& p) {
    return std::array<double, 2>{p[0] * ca + p[1] * sa,
                                 -p[0] * sa + p[1] * ca};
  };

  std::vector<std::vector<std::array<double, 2>>> loops;
  loops.reserve(1 + region.holes.size());
  loops.push_back({});
  for (const auto& p : region.outer_loop) {
    loops.back().push_back(rotate(p));
  }
  for (const auto& hole : region.holes) {
    loops.push_back({});
    for (const auto& p : hole) {
      loops.back().push_back(rotate(p));
    }
  }

  double y_min = std::numeric_limits<double>::infinity();
  double y_max = -std::numeric_limits<double>::infinity();
  for (const auto& loop : loops) {
    for (const auto& p : loop) {
      y_min = std::min(y_min, p[1]);
      y_max = std::max(y_max, p[1]);
    }
  }
  if (!std::isfinite(y_min) || y_max - y_min < 1e-9) {
    return segments;
  }

  // Scanlines offset by half a spacing so a hatch line never runs
  // exactly along the region boundary.
  for (double y = y_min + spacing_mm / 2.0; y < y_max; y += spacing_mm) {
    std::vector<double> crossings;
    for (const auto& loop : loops) {
      for (size_t i = 0; i < loop.size(); ++i) {
        const auto& p = loop[i];
        const auto& q = loop[(i + 1) % loop.size()];
        // Half-open crossing test: count when exactly one endpoint is
        // strictly below the scanline — vertices landing on the line
        // are counted once, never twice.
        const bool p_below = p[1] < y;
        const bool q_below = q[1] < y;
        if (p_below == q_below) {
          continue;
        }
        const double t = (y - p[1]) / (q[1] - p[1]);
        crossings.push_back(p[0] + t * (q[0] - p[0]));
      }
    }
    std::sort(crossings.begin(), crossings.end());
    // Even-odd pairing across ALL loops: holes flip the inside state
    // back to outside, so no polygon boolean ops are needed.
    for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
      if (crossings[i + 1] - crossings[i] > 1e-9) {
        segments.push_back(
            {unrotate({crossings[i], y}), unrotate({crossings[i + 1], y})});
      }
    }
  }
  return segments;
}

}  // namespace polysmith::core
