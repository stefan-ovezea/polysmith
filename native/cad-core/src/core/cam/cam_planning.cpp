#include "core/cam/cam_planning.h"

#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopTools_ShapeMapHasher.hxx>

namespace polysmith::core::cam_planning {

namespace {

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
