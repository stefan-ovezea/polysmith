#pragma once

#include <string>
#include <vector>

#include "core/cam/cam_types.h"

namespace polysmith::core {

struct CompiledBodies;
struct CompiledBody;
struct SketchProfileRegion;
struct FeatureEntry;

// ── Reference resolution shared by the CAM refresh pass and the
// generate driver ───────────────────────────────────────────────────
//
// Both consumers need the same answers: which live body face does a
// serialized FaceAttestation point at, and which live sketch region
// does a SketchProfileAttestation point at.  These helpers wrap the
// witness machinery (cam_operation.h / cam_profile_reference.h) so the
// callers never deal with scoring themselves.

struct ResolvedFaceRef {
  bool found = false;
  const CompiledBody* body = nullptr;  // valid when found
  int faceIndex = -1;                  // 0-based index in the body's face map
  double score = 0.0;
};

struct ResolvedProfileRef {
  bool found = false;
  const SketchProfileRegion* region = nullptr;  // valid when found
  double score = 0.0;
};

// Resolves a face attestation against every body in the compiled set.
// The serialized attestation carries no body id (bodies are rebuilt by
// compile_bodies on every recompute), so all bodies are candidates.
ResolvedFaceRef resolve_face_attestation(const FaceAttestation& attestation,
                                         const CompiledBodies& bodies);

// Resolves a profile attestation against a sketch feature's current
// region list.  Callers should ensure the sketch's profiles are
// current (refresh_sketch_profiles runs on load and on sketch edits
// via the history dependency pass).
ResolvedProfileRef resolve_profile_attestation(
    const SketchProfileAttestation& attestation, const FeatureEntry& sketch);

// Human-readable degradation message for a failed/ambiguous face
// reference, following the dependency_broken doctrine: never throw,
// never guess.
std::string face_reference_failure_message(const ResolvedFaceRef& face);
std::string profile_reference_failure_message(
    const ResolvedProfileRef& profile);

}  // namespace polysmith::core
