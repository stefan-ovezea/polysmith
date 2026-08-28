#include "core/cam/cam_profile_reference.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/document/document.h"
#include "core/sketch/sketch_profile_types.h"

namespace polysmith::core {

namespace {

// Shoelace area of a sampled polygon (absolute value).  Returns 0 for
// degenerate point sets.
double polygon_area(const std::vector<SketchProfilePoint>& points) {
  if (points.size() < 3) {
    return 0.0;
  }
  double twiceArea = 0.0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    twiceArea += a.x * b.y - b.x * a.y;
  }
  return std::abs(twiceArea) / 2.0;
}

// Shoelace centroid of a sampled polygon.  Falls back to the vertex
// average for degenerate point sets.
std::pair<double, double> polygon_centroid(
    const std::vector<SketchProfilePoint>& points) {
  if (points.empty()) {
    return {0.0, 0.0};
  }
  double twiceArea = 0.0;
  double cx = 0.0;
  double cy = 0.0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    const double cross = a.x * b.y - b.x * a.y;
    twiceArea += cross;
    cx += (a.x + b.x) * cross;
    cy += (a.y + b.y) * cross;
  }
  if (std::abs(twiceArea) < 1e-12) {
    double sx = 0.0;
    double sy = 0.0;
    for (const auto& p : points) {
      sx += p.x;
      sy += p.y;
    }
    return {sx / points.size(), sy / points.size()};
  }
  return {cx / (3.0 * twiceArea), cy / (3.0 * twiceArea)};
}

}  // namespace

std::optional<CamProfileReference> capture_profile_reference(
    const std::string& sketchFeatureId, const SketchProfileRegion& region) {
  if (region.points.empty() && region.kind != "circle" &&
      !region.source_circle_id.has_value()) {
    return std::nullopt;
  }

  CamProfileReference reference;
  reference.sketchFeatureId = sketchFeatureId;
  reference.profileId = region.id;
  reference.innerLoopCount = static_cast<int>(region.inner_loops.size());
  reference.sourceCircleId = region.source_circle_id;

  // Circle-sourced regions carry exact center/radius — no sampling
  // error.  The exact detector emits them as polygon-kind regions
  // whose single boundary edge is the circle, so read the exact
  // center/radius from that edge (falling back to the cached region
  // fields for legacy profiles).  Polygons use the sampled point set.
  if (region.kind == "circle" || region.source_circle_id.has_value()) {
    constexpr double kPi = 3.14159265358979323846;
    double cx = region.center_x;
    double cy = region.center_y;
    double radius = region.radius;
    for (const auto& edge : region.boundary_edges) {
      if (edge.entity_kind == "circle") {
        cx = edge.center_x;
        cy = edge.center_y;
        radius = edge.radius;
        break;
      }
    }
    reference.centerX = cx;
    reference.centerY = cy;
    reference.area = kPi * radius * radius;
    reference.minX = cx - radius;
    reference.maxX = cx + radius;
    reference.minY = cy - radius;
    reference.maxY = cy + radius;
  } else {
    // Material area and centroid: the outer polygon minus its hole
    // loops.  The hole points are sampled, so the subtraction is an
    // approximation, but capture and resolve compute the same numbers
    // — consistency is what the witness matching needs.
    double area = polygon_area(region.points);
    double centroidNumeratorX = 0.0;
    double centroidNumeratorY = 0.0;
    const auto [outerCx, outerCy] = polygon_centroid(region.points);
    centroidNumeratorX = outerCx * area;
    centroidNumeratorY = outerCy * area;
    for (const auto& hole : region.inner_loops) {
      const double holeArea = polygon_area(hole);
      const auto [holeCx, holeCy] = polygon_centroid(hole);
      area -= holeArea;
      centroidNumeratorX -= holeCx * holeArea;
      centroidNumeratorY -= holeCy * holeArea;
    }
    reference.area = area;
    if (area > 1e-12) {
      reference.centerX = centroidNumeratorX / area;
      reference.centerY = centroidNumeratorY / area;
    } else {
      reference.centerX = outerCx;
      reference.centerY = outerCy;
    }
    reference.minX = std::numeric_limits<double>::max();
    reference.minY = std::numeric_limits<double>::max();
    reference.maxX = std::numeric_limits<double>::lowest();
    reference.maxY = std::numeric_limits<double>::lowest();
    for (const auto& p : region.points) {
      reference.minX = std::min(reference.minX, p.x);
      reference.minY = std::min(reference.minY, p.y);
      reference.maxX = std::max(reference.maxX, p.x);
      reference.maxY = std::max(reference.maxY, p.y);
    }
  }

  // Boundary signature — the ordered kinds of the exact boundary edges.
  for (const auto& edge : region.boundary_edges) {
    reference.boundaryEdgeKinds.push_back(edge.entity_kind);
  }
  // Legacy profiles without exact boundary edges fall back to a
  // degenerate signature (empty) — resolution then leans on centroid,
  // area, and hole count alone.

  return reference;
}

std::optional<CamProfileReference> cam_profile_reference_from_attestation(
    const SketchProfileAttestation& attestation) {
  if (attestation.sketch_feature_id.empty()) {
    return std::nullopt;
  }
  CamProfileReference reference;
  reference.sketchFeatureId = attestation.sketch_feature_id;
  reference.profileId = attestation.profile_id;
  reference.centerX = attestation.center_x;
  reference.centerY = attestation.center_y;
  reference.area = attestation.area;
  reference.minX = attestation.min_x;
  reference.minY = attestation.min_y;
  reference.maxX = attestation.max_x;
  reference.maxY = attestation.max_y;
  reference.boundaryEdgeKinds = attestation.boundary_edge_kinds;
  reference.innerLoopCount = attestation.inner_loop_count;
  reference.sourceCircleId = attestation.source_circle_id;
  return reference;
}

namespace {

// Scores a candidate region against witness data, 0.0 (no match) to
// 1.0 (perfect match).  Weights: boundary signature 0.35 (the entity
// set is the strongest identity signal), area 0.3, centroid 0.25,
// hole count 0.1.  A moved-but-unchanged region (same entities, same
// area, shifted centroid) still clears the 0.7 threshold, while a
// same-shaped region from a different entity set scores ~0.4.
double score_profile_candidate(const CamProfileReference& reference,
                               const SketchProfileRegion& region) {
  const auto candidate = capture_profile_reference(
      reference.sketchFeatureId, region);
  if (!candidate.has_value()) {
    return 0.0;
  }

  double score = 0.0;

  // ── Boundary signature (weight 0.35) ────────────────────────
  if (!reference.boundaryEdgeKinds.empty() &&
      reference.boundaryEdgeKinds == candidate->boundaryEdgeKinds) {
    score += 0.35;
  }

  // ── Area match (weight 0.3) ──────────────────────────────────
  if (reference.area > 0.0 && candidate->area > 0.0) {
    const double ratio = std::min(reference.area, candidate->area) /
                         std::max(reference.area, candidate->area);
    score += 0.3 * ratio;
  }

  // ── Centroid match (weight 0.25) ─────────────────────────────
  // Distance scaled by the reference bbox diagonal so small parts are
  // not graded more strictly than large ones.
  const double diagX = reference.maxX - reference.minX;
  const double diagY = reference.maxY - reference.minY;
  const double scale = std::max(1.0, std::hypot(diagX, diagY));
  const double distance =
      std::hypot(candidate->centerX - reference.centerX,
                 candidate->centerY - reference.centerY);
  const double centroidScore = std::max(0.0, 1.0 - distance / scale);
  score += 0.25 * centroidScore;

  // ── Hole count (weight 0.1) ──────────────────────────────────
  if (reference.innerLoopCount == candidate->innerLoopCount) {
    score += 0.1;
  }

  return score;
}

}  // namespace

namespace {

// Approximate signature of a hole loop: centroid + mean radius of the
// sampled points.  Used to recognize a standalone profile region that
// duplicates an inner loop of another region.
struct HoleSignature {
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
};

std::optional<HoleSignature> hole_signature(
    const std::vector<SketchProfilePoint>& loop) {
  if (loop.empty()) {
    return std::nullopt;
  }
  double cx = 0.0;
  double cy = 0.0;
  for (const auto& point : loop) {
    cx += point.x;
    cy += point.y;
  }
  cx /= loop.size();
  cy /= loop.size();
  double radius = 0.0;
  for (const auto& point : loop) {
    radius += std::hypot(point.x - cx, point.y - cy);
  }
  radius /= loop.size();
  return HoleSignature{cx, cy, radius};
}

// A standalone region duplicates a hole loop of another region when its
// center/radius match the loop's within a small tolerance.
bool matches_hole(const CamProfileReference& region,
                  const HoleSignature& hole) {
  const double centerDistance =
      std::hypot(region.centerX - hole.center_x,
                 region.centerY - hole.center_y);
  const double radiusTolerance =
      0.05 + 0.02 * std::max(region.area > 0.0 ? std::sqrt(region.area / 3.14159265358979323846)
                                              : 0.0,
                             hole.radius);
  return centerDistance < radiusTolerance &&
         std::abs(std::sqrt(region.area / 3.14159265358979323846) -
                  hole.radius) < radiusTolerance;
}

}  // namespace

bool capture_profile_references_from_selection(const DocumentState& document,
                                               CamOperation& op) {
  // Two selection modes:
  //   1. regions picked in the active sketch
  //      (selected_sketch_profile_ids), or
  //   2. the CAM-workspace flow — a closed sketch is SELECTED as a
  //      feature and every profile of that sketch is captured.
  std::vector<std::string> profileIds = document.selected_sketch_profile_ids;
  bool allProfilesOfSelectedSketch = false;
  if (profileIds.empty()) {
    const FeatureEntry* selectedSketch = nullptr;
    for (const auto& feature : document.feature_history) {
      if (feature.id == document.selected_feature_id &&
          feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
        selectedSketch = &feature;
        break;
      }
    }
    if (selectedSketch == nullptr) {
      return false;
    }
    allProfilesOfSelectedSketch = true;
    for (const auto& region : selectedSketch->sketch_parameters->profiles) {
      profileIds.push_back(region.id);
    }
  }

  // Whole-sketch capture: a profile detector reports holes TWICE — as
  // inner loops of the surrounding region AND as standalone regions.
  // Cutting both would trace every hole twice; skip standalone regions
  // that duplicate an inner loop of another captured region.  Explicit
  // profile selections are honored as-is.
  std::vector<HoleSignature> holeSignatures;
  if (allProfilesOfSelectedSketch) {
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch" ||
          !feature.sketch_parameters.has_value()) {
        continue;
      }
      for (const auto& region : feature.sketch_parameters->profiles) {
        for (const auto& loop : region.inner_loops) {
          const auto signature = hole_signature(loop);
          if (signature.has_value()) {
            holeSignatures.push_back(signature.value());
          }
        }
      }
    }
  }

  bool capturedAny = false;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& region : sketch.profiles) {
      const bool selected =
          allProfilesOfSelectedSketch ||
          std::find(profileIds.begin(), profileIds.end(), region.id) !=
              profileIds.end();
      if (!selected) {
        continue;
      }
      const auto reference = capture_profile_reference(feature.id, region);
      if (!reference.has_value()) {
        continue;
      }
      if (allProfilesOfSelectedSketch) {
        bool duplicatesHole = false;
        for (const auto& hole : holeSignatures) {
          if (matches_hole(reference.value(), hole)) {
            duplicatesHole = true;
            break;
          }
        }
        if (duplicatesHole) {
          continue;  // already cut as an inner loop of its own region
        }
      }
      GeometryReference stored;
      stored.persistent_id = region.id;
      SketchProfileAttestation attestation;
      attestation.sketch_feature_id = reference->sketchFeatureId;
      attestation.profile_id = reference->profileId;
      attestation.center_x = reference->centerX;
      attestation.center_y = reference->centerY;
      attestation.area = reference->area;
      attestation.min_x = reference->minX;
      attestation.min_y = reference->minY;
      attestation.max_x = reference->maxX;
      attestation.max_y = reference->maxY;
      attestation.boundary_edge_kinds = reference->boundaryEdgeKinds;
      attestation.inner_loop_count = reference->innerLoopCount;
      attestation.source_circle_id = reference->sourceCircleId;
      stored.attestation = attestation;
      op.geometry_references.machining_regions.push_back(stored);
      capturedAny = true;
    }
  }
  return capturedAny;
}

ProfileResolutionResult resolve_profile_reference(
    const CamProfileReference& reference,
    const SketchFeatureParameters& sketch) {
  ProfileResolutionResult result;
  for (const auto& region : sketch.profiles) {
    result.candidates.push_back(
        {&region, score_profile_candidate(reference, region)});
  }
  std::sort(result.candidates.begin(), result.candidates.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });

  if (result.candidates.empty() ||
      result.candidates.front().score < kProfileScoreThreshold) {
    result.candidates.clear();
    result.outcome = ProfileResolutionOutcome::NotFound;
    return result;
  }

  // Ambiguity requires a NEAR TIE at the top: the witness centroid
  // decisively distinguishes separated regions (identical shapes
  // elsewhere in the sketch score well below the best), so only
  // candidates within 5% of the best make the resolution ambiguous —
  // never guess, let the user re-pick.
  const double best = result.candidates.front().score;
  if (result.candidates.size() > 1 &&
      result.candidates[1].score >= kProfileScoreThreshold &&
      result.candidates[1].score >= best * (1.0 - kProfileAmbiguityRatio)) {
    result.outcome = ProfileResolutionOutcome::Ambiguous;
    return result;
  }

  result.outcome = ProfileResolutionOutcome::Found;
  return result;
}

}  // namespace polysmith::core
