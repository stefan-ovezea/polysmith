#include "core/cam_operation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include "core/body_compiler.h"
#include "core/document.h"

namespace polysmith::core {
namespace {

// ── Helpers ───────────────────────────────────────────────────────

double point_distance(const gp_Pnt& a, const gp_Pnt& b) {
  return std::sqrt((a.X() - b.X()) * (a.X() - b.X()) +
                   (a.Y() - b.Y()) * (a.Y() - b.Y()) +
                   (a.Z() - b.Z()) * (a.Z() - b.Z()));
}

double normal_angle(const gp_Dir& a, const gp_Dir& b) {
  const double dot = a.X() * b.X() + a.Y() * b.Y() + a.Z() * b.Z();
  // Clamp to [-1, 1] to avoid acos domain errors from float imprecision.
  const double clamped = std::max(-1.0, std::min(1.0, dot));
  return std::acos(clamped);
}

double compute_face_area(const TopoDS_Face& face) {
  try {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.Mass();
  } catch (const std::exception&) {
    return 0.0;
  }
}

bool is_point_on_face(const gp_Pnt& pnt, const TopoDS_Face& face,
                      double tolerance = 1e-3) {
  try {
    BRepClass_FaceClassifier classifier;
    // tolerance = maximum distance from face surface to consider "on".
    classifier.Perform(face, pnt, tolerance);
    return classifier.State() == TopAbs_ON ||
           classifier.State() == TopAbs_IN;
  } catch (const std::exception&) {
    return false;
  }
}

// Sample `count` points on the face surface, distributed across the
// UV parameter domain. Returns the sample points in world coordinates.
std::vector<gp_Pnt> sample_face_points(const TopoDS_Face& face, int count = 10) {
  std::vector<gp_Pnt> points;
  try {
    BRepAdaptor_Surface surface(face);
    const double uMin = surface.FirstUParameter();
    const double uMax = surface.LastUParameter();
    const double vMin = surface.FirstVParameter();
    const double vMax = surface.LastVParameter();

    // Distribute points in a grid pattern across the UV domain.
    // Use roughly sqrt(count) divisions in each direction.
    const int divisions = static_cast<int>(std::ceil(std::sqrt(
        static_cast<double>(count))));
    for (int i = 0; i < divisions; ++i) {
      for (int j = 0; j < divisions; ++j) {
        if (static_cast<int>(points.size()) >= count) break;
        const double u = uMin + (uMax - uMin) * (i + 0.5) /
                                     static_cast<double>(divisions);
        const double v = vMin + (vMax - vMin) * (j + 0.5) /
                                     static_cast<double>(divisions);
        gp_Pnt p;
        gp_Vec normal;
        surface.D0(u, v, p);
        points.push_back(p);
      }
      if (static_cast<int>(points.size()) >= count) break;
    }
  } catch (const std::exception&) {
    // Return whatever we sampled before the error.
  }
  return points;
}

// Compute the normal at the face's UV center.
std::optional<gp_Dir> face_center_normal(const TopoDS_Face& face) {
  try {
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
      return std::nullopt;
    }
    normal.Normalize();
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }
    return gp_Dir(normal);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Score a candidate face against witness data. Returns 0.0 (no match)
// to 1.0 (perfect match).
double score_candidate(const CamFaceReference& reference,
                       const TopoDS_Face& face) {
  double score = 0.0;

  // ── Sample point containment (weight: 0.5) ──────────────────
  if (!reference.samplePoints.empty()) {
    int hits = 0;
    for (const auto& sp : reference.samplePoints) {
      gp_Pnt p(sp[0], sp[1], sp[2]);
      if (is_point_on_face(p, face)) {
        ++hits;
      }
    }
    const double hitRatio =
        static_cast<double>(hits) / reference.samplePoints.size();
    score += 0.5 * hitRatio;
  }

  // ── Area match (weight: 0.25) ───────────────────────────────
  const double area = compute_face_area(face);
  if (reference.capturedArea > 0.0 && area > 0.0) {
    const double ratio = std::min(area, reference.capturedArea) /
                         std::max(area, reference.capturedArea);
    score += 0.25 * ratio;
  }

  // ── Normal match (weight: 0.25) ─────────────────────────────
  const auto normal = face_center_normal(face);
  if (normal.has_value()) {
    const gp_Dir refNormal(reference.capturedNormal[0],
                           reference.capturedNormal[1],
                           reference.capturedNormal[2]);
    const double angle = normal_angle(*normal, refNormal);
    // Score drops linearly from 1.0 at 0° to 0.0 at kMaxNormalAngle.
    const double normalScore =
        1.0 - std::min(1.0, angle / kMaxNormalAngle);
    score += 0.25 * normalScore;
  }

  return score;
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────

std::optional<CamFaceReference> capture_face_reference(
    const std::string& bodyId,
    const TopoDS_Shape& bodyShape,
    int faceIndex,
    const std::string& label) {
  if (bodyShape.IsNull() || faceIndex < 0) {
    return std::nullopt;
  }

  TopTools_IndexedMapOfShape faceMap;
  TopExp::MapShapes(bodyShape, TopAbs_FACE, faceMap);
  const int oneBased = faceIndex + 1;
  if (oneBased < 1 || oneBased > faceMap.Extent()) {
    return std::nullopt;
  }

  const TopoDS_Face face = TopoDS::Face(faceMap(oneBased));
  if (face.IsNull()) {
    return std::nullopt;
  }

  // Sample points
  const auto worldPoints = sample_face_points(face);
  if (worldPoints.empty()) {
    return std::nullopt;
  }

  // Area
  const double area = compute_face_area(face);

  // Normal
  const auto normal = face_center_normal(face);
  if (!normal.has_value()) {
    return std::nullopt;
  }

  CamFaceReference ref;
  ref.bodyId = bodyId;
  ref.label = label;
  ref.capturedArea = area;

  ref.samplePoints.reserve(worldPoints.size());
  for (const auto& p : worldPoints) {
    ref.samplePoints.push_back({p.X(), p.Y(), p.Z()});
  }

  ref.capturedNormal = {normal->X(), normal->Y(), normal->Z()};

  return ref;
}

FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const TopoDS_Shape& bodyShape) {
  FaceResolutionResult result;

  if (bodyShape.IsNull()) {
    return result;
  }

  TopTools_IndexedMapOfShape faceMap;
  TopExp::MapShapes(bodyShape, TopAbs_FACE, faceMap);

  // Score every face.
  for (int i = 1; i <= faceMap.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(faceMap(i));
    if (face.IsNull()) continue;

    const double score = score_candidate(reference, face);
    if (score >= kScoreThreshold) {
      result.candidates.push_back(
          ResolvedFace{.faceIndex = i - 1, .score = score});
    }
  }

  // Sort by score descending.
  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const ResolvedFace& a, const ResolvedFace& b) {
              return a.score > b.score;
            });

  // Determine outcome.
  if (result.candidates.empty()) {
    result.outcome = FaceResolutionOutcome::NotFound;
  } else if (result.candidates.size() == 1) {
    result.outcome = FaceResolutionOutcome::Found;
  } else {
    // Check if the top two candidates are close enough to be ambiguous.
    // If the second-best is within 5% of the best, treat as ambiguous.
    const double best = result.candidates[0].score;
    const double second = result.candidates[1].score;
    if (second >= best * 0.95) {
      result.outcome = FaceResolutionOutcome::Ambiguous;
    } else {
      // Clear margin — keep only the winner.
      result.candidates.resize(1);
      result.outcome = FaceResolutionOutcome::Found;
    }
  }

  return result;
}

FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const DocumentState& document) {
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id == reference.bodyId && !body.shape.IsNull()) {
      return resolve_face_reference(reference, body.shape);
    }
  }
  return FaceResolutionResult{};
}

}  // namespace polysmith::core
