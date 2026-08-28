#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_types.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core {

struct SketchProfileRegion;
struct DocumentState;

// ── Sketch profile references with geometric attestation ──────────
//
// The sketch-profile analogue of the face witness machinery.  Profile
// ids churn because refresh_sketch_profiles() recomputes the whole
// region list after any sketch edit, so an operation cannot store a
// bare "profile-3".  Instead it stores witness data (centroid, area,
// bounding box, boundary-kind signature, hole count) captured at
// create time, and re-resolves against freshly built regions on every
// recompute — same doctrine as faces, in sketch-local 2D.

struct CamProfileReference {
  std::string sketchFeatureId;  // owning sketch feature
  std::string profileId;        // last-known id (best effort only)
  double centerX = 0.0, centerY = 0.0;  // sketch-local centroid
  double area = 0.0;                    // sketch-local area (mm²)
  double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;  // bbox
  std::vector<std::string> boundaryEdgeKinds;  // walk-order signature
  int innerLoopCount = 0;                      // holes
  std::optional<std::string> sourceCircleId;   // circle-sourced regions
};

// Captures witness data from a live profile region of the given sketch.
std::optional<CamProfileReference> capture_profile_reference(
    const std::string& sketchFeatureId, const SketchProfileRegion& region);

// Converts a serialized attestation into a runtime witness.
std::optional<CamProfileReference> cam_profile_reference_from_attestation(
    const SketchProfileAttestation& attestation);

enum class ProfileResolutionOutcome {
  Found,       // exactly one region scored above threshold
  Ambiguous,   // multiple regions scored above threshold
  NotFound     // no region scored above threshold
};

struct ResolvedProfileCandidate {
  const SketchProfileRegion* region = nullptr;
  double score = 0.0;  // 0.0 (no match) to 1.0 (perfect match)
};

struct ProfileResolutionResult {
  ProfileResolutionOutcome outcome = ProfileResolutionOutcome::NotFound;
  std::vector<ResolvedProfileCandidate> candidates;  // sorted by score desc
};

// Re-resolves a profile witness against the sketch's current region
// list.  Callers should rebuild regions first via
// build_sketch_profile_regions when the stored list may be stale.
ProfileResolutionResult resolve_profile_reference(
    const CamProfileReference& reference,
    const SketchFeatureParameters& sketch);

struct CamOperation;

// Captures witness references for every region named in the document's
// selected_sketch_profile_ids and appends them to the operation's
// machining_regions.  Returns false when the selection matched no live
// profile (the caller reports a human-readable error).
bool capture_profile_references_from_selection(const DocumentState& document,
                                               CamOperation& op);

// Score threshold above which a candidate region is considered a
// match; a second candidate above the threshold AND within 5% of the
// best makes the resolution ambiguous (never guess — the user
// re-picks).  Separated regions are disambiguated by the witness
// centroid, so only near-ties stay ambiguous.
inline constexpr double kProfileScoreThreshold = 0.7;
inline constexpr double kProfileAmbiguityRatio = 0.05;

}  // namespace polysmith::core
