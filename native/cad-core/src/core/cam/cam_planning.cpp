#include "core/cam/cam_planning.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Circ.hxx>

namespace polysmith::core::cam_planning {

namespace {

using cam2d::BaseSegment;
using cam2d::XY;
using cam2d::xy_length;

// Recursively refines the curve span (t0,t1) between points p0,p1
// until the curve's midpoint deviation from the chord stays within
// tolerance (depth is the safety-net bound).
void refine_curve(const BRepAdaptor_Curve& curve, double t0, double t1,
                  const gp_Pnt& p0, const gp_Pnt& p1, double tolerance,
                  int depth, std::vector<XY>& out) {
  if (depth >= 12) {
    out.push_back({p1.X(), p1.Y()});
    return;
  }
  const double tMid = 0.5 * (t0 + t1);
  gp_Pnt pMid;
  curve.D0(tMid, pMid);
  const double midX = 0.5 * (p0.X() + p1.X());
  const double midY = 0.5 * (p0.Y() + p1.Y());
  if (xy_length(pMid.X() - midX, pMid.Y() - midY) <= tolerance) {
    out.push_back({p1.X(), p1.Y()});
    return;
  }
  refine_curve(curve, t0, tMid, p0, pMid, tolerance, depth + 1, out);
  refine_curve(curve, tMid, t1, pMid, p1, tolerance, depth + 1, out);
}

}  // namespace

bool plan_stepdown_levels(double stockTopZ, double faceZ, double stepdown,
                          const std::vector<double>& extra_levels,
                          std::vector<double>& levels,
                          std::vector<std::string>& warnings) {
  levels.clear();
  constexpr int kMaxLevels = 100;
  auto warn_capped = [&]() {
    std::ostringstream cap;
    cap << "Pass count capped at " << kMaxLevels
        << " — reduce the stepdown or the stock height.";
    warnings.push_back(cap.str());
  };
  if (stepdown <= 1e-9 || stockTopZ <= faceZ + 1e-9) {
    levels.push_back(faceZ);
    return false;
  }

  // Descending stepdown levels: stockTop − stepdown down to faceZ.
  bool capped = false;
  for (double z = stockTopZ - stepdown; z > faceZ + 1e-9; z -= stepdown) {
    levels.push_back(z);
    if (static_cast<int>(levels.size()) >= kMaxLevels) {
      warn_capped();
      capped = true;
      break;
    }
  }

  // Island tops and the like: extra levels strictly between faceZ and
  // stockTop join the plan (deduped within eps) so the stock above a
  // short island is cleared down to the island top.
  for (const double extra : extra_levels) {
    if (extra <= faceZ + 1e-9 || extra >= stockTopZ - 1e-9) {
      continue;
    }
    bool duplicate = false;
    for (const double existing : levels) {
      if (std::fabs(existing - extra) <= 1e-6) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    if (static_cast<int>(levels.size()) >= kMaxLevels) {
      if (!capped) {
        warn_capped();
      }
      break;
    }
    levels.push_back(extra);
  }

  std::sort(levels.begin(), levels.end(), std::greater<double>());
  levels.push_back(faceZ);  // the last cut is always pinned to the face
  return true;
}

bool sample_planar_wire(const TopoDS_Wire& wire, double chord_tolerance,
                        std::vector<cam2d::XY>& out_loop) {
  std::vector<std::vector<XY>> pieces;
  BRepTools_WireExplorer walker(wire);
  for (; walker.More(); walker.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(walker.Current());
    if (edge.IsNull()) {
      continue;
    }
    BRepAdaptor_Curve curve(edge);
    const double t0 = curve.FirstParameter();
    const double t1 = curve.LastParameter();
    gp_Pnt p0;
    gp_Pnt p1;
    curve.D0(t0, p0);
    curve.D0(t1, p1);
    std::vector<XY> piece;
    piece.push_back({p0.X(), p0.Y()});
    refine_curve(curve, t0, t1, p0, p1, chord_tolerance, /*depth=*/0, piece);
    pieces.push_back(std::move(piece));
  }

  out_loop.clear();
  if (pieces.empty()) {
    return false;
  }
  out_loop = pieces.front();
  std::vector<bool> used(pieces.size(), false);
  used[0] = true;
  for (size_t remaining = 1; remaining < pieces.size(); ++remaining) {
    bool chained = false;
    for (size_t j = 0; j < pieces.size(); ++j) {
      if (used[j]) {
        continue;
      }
      const XY& last = out_loop.back();
      const auto& piece = pieces[j];
      if (xy_length(piece.front().x - last.x,
                    piece.front().y - last.y) < 0.001) {
        out_loop.insert(out_loop.end(), piece.begin() + 1, piece.end());
        used[j] = true;
        chained = true;
        break;
      }
      if (xy_length(piece.back().x - last.x,
                    piece.back().y - last.y) < 0.001) {
        // Piece arrives reversed — append it flipped.
        for (size_t k = piece.size() - 1; k > 0; --k) {
          out_loop.push_back(piece[k - 1]);
        }
        used[j] = true;
        chained = true;
        break;
      }
    }
    if (!chained) {
      return false;
    }
  }
  // Drop the repeated closing point.
  if (out_loop.size() > 1 &&
      xy_length(out_loop.front().x - out_loop.back().x,
                out_loop.front().y - out_loop.back().y) < 0.001) {
    out_loop.pop_back();
  }
  return out_loop.size() >= 3;
}

bool build_base_segments_from_edges(const SketchProfileRegion& region,
                                    std::vector<BaseSegment>& out) {
  out.clear();
  for (const auto& edge : region.boundary_edges) {
    if (edge.entity_kind == "ellipse" || edge.entity_kind == "spline") {
      return false;
    }
    BaseSegment segment;
    segment.start = {edge.start_x, edge.start_y};
    segment.end = {edge.end_x, edge.end_y};
    if (edge.entity_kind == "circle" || edge.entity_kind == "arc") {
      segment.is_arc = true;
      segment.center = {edge.center_x, edge.center_y};
      segment.radius = edge.radius;
      segment.ccw = edge.ccw;
    }
    out.push_back(segment);
  }
  return !out.empty();
}

void build_base_segments_from_points(
    const std::vector<SketchProfilePoint>& points,
    std::vector<BaseSegment>& out) {
  out.clear();
  if (points.size() < 2) {
    return;
  }
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    BaseSegment segment;
    segment.start = {a.x, a.y};
    segment.end = {b.x, b.y};
    out.push_back(segment);
  }
}

bool build_base_segments_from_wire(const TopoDS_Wire& wire,
                                   std::vector<BaseSegment>& out) {
  // Exact edges only — a non-line/non-circle edge (ellipse, spline)
  // has no exact offset, so the caller falls back to sampled
  // polylines.
  std::vector<BaseSegment> pieces;
  BRepTools_WireExplorer walker(wire);
  for (; walker.More(); walker.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(walker.Current());
    if (edge.IsNull()) {
      continue;
    }
    BRepAdaptor_Curve curve(edge);
    const double t0 = curve.FirstParameter();
    const double t1 = curve.LastParameter();
    gp_Pnt p0;
    gp_Pnt p1;
    curve.D0(t0, p0);
    curve.D0(t1, p1);
    BaseSegment segment;
    segment.start = {p0.X(), p0.Y()};
    segment.end = {p1.X(), p1.Y()};
    if (curve.GetType() == GeomAbs_Circle) {
      gp_Circ circle = curve.Circle();
      segment.is_arc = true;
      segment.center = {circle.Location().X(), circle.Location().Y()};
      segment.radius = circle.Radius();
      if (xy_length(segment.end.x - segment.start.x,
                    segment.end.y - segment.start.y) < 1e-9) {
        // Full circle: start == end, walked CCW (the laser
        // synthesized-circle convention).
        segment.ccw = true;
      } else {
        // Sweep direction from the chord triangle: the adaptor
        // follows the edge, so start → mid → end traces the arc, and
        // the triangle's signed area is positive iff the walk is CCW
        // (holds for any sweep below 360°).
        gp_Pnt pMid;
        curve.D0(0.5 * (t0 + t1), pMid);
        const double cross =
            (pMid.X() - p0.X()) * (p1.Y() - p0.Y()) -
            (pMid.Y() - p0.Y()) * (p1.X() - p0.X());
        segment.ccw = cross > 0.0;
      }
    } else if (curve.GetType() != GeomAbs_Line) {
      return false;
    }
    pieces.push_back(segment);
  }
  if (pieces.empty()) {
    return false;
  }

  // Chain by endpoint matching (wire edge orientations vary — pieces
  // may arrive reversed; match-and-flip like sample_planar_wire).
  // Flipped arcs invert their sweep direction.
  out.clear();
  out.push_back(pieces.front());
  std::vector<bool> used(pieces.size(), false);
  used[0] = true;
  for (size_t remaining = 1; remaining < pieces.size(); ++remaining) {
    bool chained = false;
    for (size_t j = 0; j < pieces.size(); ++j) {
      if (used[j]) {
        continue;
      }
      BaseSegment& piece = pieces[j];
      const XY& last = out.back().end;
      if (xy_length(piece.start.x - last.x,
                    piece.start.y - last.y) < 0.001) {
        out.push_back(piece);
        used[j] = true;
        chained = true;
        break;
      }
      if (xy_length(piece.end.x - last.x,
                    piece.end.y - last.y) < 0.001) {
        std::swap(piece.start, piece.end);
        if (piece.is_arc) {
          piece.ccw = !piece.ccw;
        }
        out.push_back(piece);
        used[j] = true;
        chained = true;
        break;
      }
    }
    if (!chained) {
      return false;
    }
  }
  return !out.empty();
}

XY world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p) {
  return XY{frame.origin_x + frame.x_axis_x * p.x + frame.y_axis_x * p.y,
            frame.origin_y + frame.x_axis_y * p.x + frame.y_axis_y * p.y};
}

double world_z(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p, double cut_plane_offset) {
  return frame.origin_z + frame.x_axis_z * p.x + frame.y_axis_z * p.y +
         frame.normal_z * cut_plane_offset;
}

std::optional<SketchFeatureParameters::SketchPlaneFrame> resolve_sketch_frame(
    const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    return sketch.plane_frame;
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 1.0, .y_axis_z = 0.0,
        .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
    };
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 0.0, .x_axis_y = 1.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = 1.0,
        .normal_x = 1.0, .normal_y = 0.0, .normal_z = 0.0,
    };
  }
  // xz: x=(1,0,0), y=(0,0,-1) gives x×y=(0,1,0)=normal — the frame
  // must stay right-handed or arc sweeps mirror in world space.
  if (sketch.plane_id == "ref-plane-xz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = -1.0,
        .normal_x = 0.0, .normal_y = 1.0, .normal_z = 0.0,
    };
  }
  return std::nullopt;
}

bool face_cut_plane(const TopoDS_Face& face, gp_Pnt& out_point,
                    gp_Vec& out_normal) {
  BRepAdaptor_Surface surface(face);
  const double uMid =
      0.5 * (surface.FirstUParameter() + surface.LastUParameter());
  const double vMid =
      0.5 * (surface.FirstVParameter() + surface.LastVParameter());
  gp_Vec d1u;
  gp_Vec d1v;
  surface.D1(uMid, vMid, out_point, d1u, d1v);
  out_normal = d1u.Crossed(d1v);
  return out_normal.Magnitude() > 1e-12;
}

bool map_face_index(const TopoDS_Shape& body, int face_index,
                    TopoDS_Face& out_face, std::string& error_message) {
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
  TopExp::MapShapes(body, TopAbs_FACE, faceMap);
  if (face_index < 0 || face_index >= faceMap.Extent()) {
    error_message =
        "The referenced face no longer exists (geometry changed — "
        "re-select it).";
    return false;
  }
  out_face = TopoDS::Face(faceMap(face_index + 1));
  return true;
}

}  // namespace polysmith::core::cam_planning
