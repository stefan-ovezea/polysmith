#include "core/cam_operation.h"

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

#include "core/body_compiler.h"
#include "core/document.h"

namespace polysmith::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

// ── 2D point helpers ────────────────────────────────────────────

struct Point2D {
  double x;
  double y;
};

double cross2D(const Point2D& a, const Point2D& b) {
  return a.x * b.y - a.y * b.x;
}

Point2D sub2D(const Point2D& a, const Point2D& b) {
  return {a.x - b.x, a.y - b.y};
}

// Clip a line segment (p1→p2) to a convex polygon using
// Sutherland–Hodgman. Returns the clipped segment endpoints,
// or empty vector if completely outside.
std::vector<Point2D> clipLineToPolygon(
    Point2D p1, Point2D p2,
    const std::vector<Point2D>& poly) {
  std::vector<Point2D> output = {p1, p2};

  for (size_t i = 0; i < poly.size(); ++i) {
    if (output.empty()) break;
    std::vector<Point2D> input = std::move(output);
    output.clear();

    const Point2D& edgeStart = poly[i];
    const Point2D& edgeEnd = poly[(i + 1) % poly.size()];
    const Point2D edgeVec = sub2D(edgeEnd, edgeStart);

    for (size_t j = 0; j < input.size(); ++j) {
      const Point2D& current = input[j];
      const Point2D& previous = input[(j + input.size() - 1) % input.size()];

      const double dCurrent = cross2D(edgeVec, sub2D(current, edgeStart));
      const double dPrevious = cross2D(edgeVec, sub2D(previous, edgeStart));

      // Current point is inside (left of edge for CCW polygon).
      if (dCurrent >= 0) {
        // Previous was outside → add intersection.
        if (dPrevious < 0) {
          const double t = dPrevious / (dPrevious - dCurrent);
          output.push_back({
              previous.x + t * (current.x - previous.x),
              previous.y + t * (current.y - previous.y),
          });
        }
        output.push_back(current);
      } else if (dPrevious >= 0) {
        // Current is outside, previous was inside → add intersection.
        const double t = dPrevious / (dPrevious - dCurrent);
        output.push_back({
            previous.x + t * (current.x - previous.x),
            previous.y + t * (current.y - previous.y),
        });
      }
    }
  }

  return output;
}

// Sample the boundary of a face as a 2D polygon in world XY space.
// Assumes the face is roughly horizontal (within ~5° of XY plane).
// Returns polygon vertices projected to the face's Z height.
std::vector<Point2D> faceBoundaryPolygonXY(const TopoDS_Face& face, double faceZ) {
  std::vector<Point2D> poly;

  TopExp_Explorer edgeExp(face, TopAbs_EDGE);
  for (; edgeExp.More(); edgeExp.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(edgeExp.Current());
    if (edge.IsNull()) continue;

    BRepAdaptor_Curve curve(edge);
    const double t0 = curve.FirstParameter();
    const double t1 = curve.LastParameter();

    const int samples = 30;
    for (int k = 0; k < samples; ++k) {
      const double t = t0 + (t1 - t0) * static_cast<double>(k) / samples;
      gp_Pnt pt;
      curve.D0(t, pt);
      poly.push_back({pt.X(), pt.Y()});
    }
  }

  // Remove near-duplicate consecutive points.
  if (poly.size() > 2) {
    std::vector<Point2D> deduped;
    deduped.push_back(poly[0]);
    for (size_t i = 1; i < poly.size(); ++i) {
      const double dx = poly[i].x - deduped.back().x;
      const double dy = poly[i].y - deduped.back().y;
      if (dx * dx + dy * dy > 0.01) {
        deduped.push_back(poly[i]);
      }
    }
    poly = std::move(deduped);
  }

  return poly;
}

}  // namespace

// ── Public API ──────────────────────────────────────────────────

CamToolpath generate_face_milling_toolpath(
    const CamOperationEntry& op,
    const DocumentState& document) {
  CamToolpath result;

  // Resolve the target body.
  const CompiledBodies compiled = compile_bodies(document);
  const TopoDS_Shape* bodyShape = nullptr;
  for (const auto& body : compiled.bodies) {
    if (body.id == op.bodyId) {
      bodyShape = &body.shape;
      break;
    }
  }
  if (!bodyShape || bodyShape->IsNull()) {
    return result;
  }

  // Find the target face.
  TopTools_IndexedMapOfShape faceMap;
  TopExp::MapShapes(*bodyShape, TopAbs_FACE, faceMap);
  const int oneBased = op.faceIndex + 1;
  if (oneBased < 1 || oneBased > faceMap.Extent()) {
    return result;
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
  const double uMid = 0.5 * (surface.FirstUParameter() + surface.LastUParameter());
  const double vMid = 0.5 * (surface.FirstVParameter() + surface.LastVParameter());
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
  const double cutZ = center.Z() - op.faceMilling.value_or(FaceMillingParams{}).depth;

  // Get parameters.
  const auto& params = op.faceMilling.value_or(FaceMillingParams{});
  const double stepover = params.stepover;
  const double angleRad = params.angleDeg * kPi / 180.0;
  const double safetyZ = document.cam_setup.has_value()
      ? document.cam_setup->safetyPlaneZ
      : 10.0;

  // Get face boundary polygon for clipping (world XY, at face Z).
  const auto boundaryPoly = faceBoundaryPolygonXY(face, center.Z());

  // Build the zigzag pattern in world XY space.
  // The scan lines are oriented at `angleDeg` from the X axis.
  const double cosA = std::cos(angleRad);
  const double sinA = std::sin(angleRad);

  // Expand slightly to ensure coverage.
  const double margin = 1.0;
  xMin -= margin; xMax += margin;
  yMin -= margin; yMax += margin;

  // Direction perpendicular to scan lines (for stepping).
  const double perpX = -sinA;
  const double perpY = cosA;

  // Compute scan line range in the perpendicular direction.
  auto projectPerp = [&](double wx, double wy) {
    return perpX * wx + perpY * wy;
  };

  double perpMin = std::min({
      projectPerp(xMin, yMin), projectPerp(xMax, yMin),
      projectPerp(xMin, yMax), projectPerp(xMax, yMax)});
  double perpMax = std::max({
      projectPerp(xMin, yMin), projectPerp(xMax, yMin),
      projectPerp(xMin, yMax), projectPerp(xMax, yMax)});

  // Generate scan lines.
  bool forward = true;
  for (double perp = perpMin; perp <= perpMax + stepover * 0.5; perp += stepover) {
    // A point on the scan line.
    const double px = perp * perpX;
    const double py = perp * perpY;

    // Extend the scan line far enough to cover the face.
    const double halfLen = std::max(xMax - xMin, yMax - yMin) * 1.5;
    
    Point2D start = {px - halfLen * cosA, py - halfLen * sinA};
    Point2D end   = {px + halfLen * cosA, py + halfLen * sinA};

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
  for (const auto& move : result.moves) {
    result.totalPoints += static_cast<int>(move.points.size());
  }
  if (result.totalPoints > 0) {
    result.minX = xMin; result.maxX = xMax;
    result.minY = yMin; result.maxY = yMax;
    result.minZ = cutZ; result.maxZ = safetyZ;
  }

  return result;
}

}  // namespace polysmith::core
