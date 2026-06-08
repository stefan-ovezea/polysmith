#include "core/cam/cam_operation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {
#include "core/cam/impl/face_milling_geometry_helpers.inc"

// ── Public API ──────────────────────────────────────────────────

CamToolpath generate_face_milling_toolpath(const CamOperationEntry &op,
                                           const DocumentState &document) {
  CamToolpath result;

  // Resolve the target body.
  const CompiledBodies compiled = compile_bodies(document);
  const TopoDS_Shape *bodyShape = nullptr;
  for (const auto &body : compiled.bodies) {
    if (body.id == op.bodyId) {
      bodyShape = &body.shape;
      break;
    }
  }
  if (!bodyShape || bodyShape->IsNull()) {
    return result;
  }

  // Find the target face — prefer the stored index, but fall back to
  // the face with the best Z-axis alignment (tool axis for 3-axis milling).
  TopTools_IndexedMapOfShape faceMap;
  TopExp::MapShapes(*bodyShape, TopAbs_FACE, faceMap);

  auto findBestZFace = [&]() -> int {
    int bestIdx = 0;
    double bestDot = -1.0;
    for (int i = 1; i <= faceMap.Extent(); ++i) {
      const TopoDS_Face &f = TopoDS::Face(faceMap(i));
      BRepAdaptor_Surface surf(f);
      const double uMid =
          0.5 * (surf.FirstUParameter() + surf.LastUParameter());
      const double vMid =
          0.5 * (surf.FirstVParameter() + surf.LastVParameter());
      gp_Pnt pt;
      gp_Vec d1u, d1v;
      surf.D1(uMid, vMid, pt, d1u, d1v);
      gp_Vec n = d1u.Crossed(d1v);
      if (n.Magnitude() > 1e-12) {
        n.Normalize();
        if (f.Orientation() == TopAbs_REVERSED)
          n.Reverse();
        const double dotZ = std::abs(n.Z());
        if (dotZ > bestDot) {
          bestDot = dotZ;
          bestIdx = i - 1;
        }
      }
    }
    return (bestDot > 0.5) ? bestIdx : 0;
  };

  int oneBased = op.faceIndex + 1;
  if (oneBased < 1 || oneBased > faceMap.Extent()) {
    oneBased = findBestZFace() + 1;
  }
  const TopoDS_Face face = TopoDS::Face(faceMap(oneBased));
  if (face.IsNull()) {
    return result;
  }

  // Get face bounds in world space.
  Bnd_Box faceBox;
  BRepBndLib::Add(face, faceBox);
  double xMin = 0, yMin = 0, zMin = 0, xMax = 0, yMax = 0, zMax = 0;
  faceBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);

  // Get the face's plane for Z height.
  BRepAdaptor_Surface surface(face);
  const double uMid =
      0.5 * (surface.FirstUParameter() + surface.LastUParameter());
  const double vMid =
      0.5 * (surface.FirstVParameter() + surface.LastVParameter());
  gp_Pnt center;
  gp_Vec d1u, d1v;
  surface.D1(uMid, vMid, center, d1u, d1v);
  gp_Vec normal = d1u.Crossed(d1v);
  if (normal.Magnitude() <= 1e-12) {
    return result;
  }
  normal.Normalize();
  if (face.Orientation() == TopAbs_REVERSED) {
    normal.Reverse();
  }

  // Face milling cuts at a fixed Z below the face.
  const double cutZ =
      center.Z() - op.faceMilling.value_or(FaceMillingParams{}).depth;

  // Get parameters.
  const auto &params = op.faceMilling.value_or(FaceMillingParams{});
  const double stepover = params.stepover;
  const double angleRad = params.angleDeg * kPi / 180.0;
  const double safetyZ =
      document.cam_setup.has_value() ? document.cam_setup->safetyPlaneZ : 10.0;

  // Get face boundary polygon for clipping (world XY, at face Z).
  const auto boundaryPoly = faceBoundaryPolygonXY(face, center.Z());

  // Build the zigzag pattern in world XY space.
  // The scan lines are oriented at `angleDeg` from the X axis.
  const double cosA = std::cos(angleRad);
  const double sinA = std::sin(angleRad);

  // Expand slightly to ensure coverage.
  const double margin = 1.0;
  xMin -= margin;
  xMax += margin;
  yMin -= margin;
  yMax += margin;

  // Direction perpendicular to scan lines (for stepping).
  const double perpX = -sinA;
  const double perpY = cosA;

  // Compute scan line range in the perpendicular direction.
  auto projectPerp = [&](double wx, double wy) {
    return perpX * wx + perpY * wy;
  };

  double perpMin = std::min({projectPerp(xMin, yMin), projectPerp(xMax, yMin),
                             projectPerp(xMin, yMax), projectPerp(xMax, yMax)});
  double perpMax = std::max({projectPerp(xMin, yMin), projectPerp(xMax, yMin),
                             projectPerp(xMin, yMax), projectPerp(xMax, yMax)});

  // Generate scan lines.
  bool forward = true;
  for (double perp = perpMin; perp <= perpMax + stepover * 0.5;
       perp += stepover) {
    // A point on the scan line.
    const double px = perp * perpX;
    const double py = perp * perpY;

    // Extend the scan line far enough to cover the face.
    const double halfLen = std::max(xMax - xMin, yMax - yMin) * 1.5;

    Point2D start = {px - halfLen * cosA, py - halfLen * sinA};
    Point2D end = {px + halfLen * cosA, py + halfLen * sinA};

    // For zigzag: alternate direction.
    if (!forward) {
      std::swap(start, end);
    }
    forward = !forward;

    // Clip to face boundary.
    auto clipped = clipLineToPolygon(start, end, boundaryPoly);
    if (clipped.size() >= 2) {
      // Rapid to start of segment.
      {
        CamToolpathMove move;
        move.isRapid = true;
        move.points.push_back({clipped[0].x, clipped[0].y, safetyZ});
        move.points.push_back({clipped[0].x, clipped[0].y, cutZ});
        result.moves.push_back(std::move(move));
      }
      // Feed along the clipped segment.
      {
        CamToolpathMove move;
        move.isRapid = false;
        for (size_t i = 0; i < clipped.size(); ++i) {
          move.points.push_back({clipped[i].x, clipped[i].y, cutZ});
        }
        result.moves.push_back(std::move(move));
      }
      // Rapid retract.
      {
        CamToolpathMove move;
        move.isRapid = true;
        move.points.push_back({clipped.back().x, clipped.back().y, safetyZ});
        result.moves.push_back(std::move(move));
      }
    }
  }

  // Compute bounds and point count.
  result.totalPoints = 0;
  for (const auto &move : result.moves) {
    result.totalPoints += static_cast<int>(move.points.size());
  }
  if (result.totalPoints > 0) {
    result.minX = xMin;
    result.maxX = xMax;
    result.minY = yMin;
    result.maxY = yMax;
    result.minZ = cutZ;
    result.maxZ = safetyZ;
  }

  return result;
}

} // namespace polysmith::core
