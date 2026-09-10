#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_types.h"
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace polysmith::core {

struct DocumentState;

// ── Face reference with geometric attestation ─────────────────────
//
// Stores enough witness data to re-identify a face after topology
// changes. On recompute we walk every face on the body, score each
// candidate against this witness data, and return the best match
// (or report ambiguity when multiple faces score similarly).

struct CamFaceReference {
  // Which body this face belongs to (body id from CompiledBody).
  std::string bodyId;

  // Sample points distributed across the face surface at capture time.
  // Stored as world-space coordinates. On resolution we test how many
  // of these points lie on each candidate face.
  std::vector<std::array<double, 3>> samplePoints;

  // Approximate area at capture time (mm²). Used for coarse filtering.
  double capturedArea = 0.0;

  // Surface normal at the face center at capture time.
  std::array<double, 3> capturedNormal = {0.0, 0.0, 1.0};

  // Human-readable label for debugging (e.g. "top face of Box 1").
  std::string label;
};

// ── Resolution result ─────────────────────────────────────────────

struct ResolvedFace {
  int faceIndex = -1;     // 0-based index into the body's face map
  double score = 0.0;     // 0.0 (no match) to 1.0 (perfect match)
};

enum class FaceResolutionOutcome {
  Found,        // Exactly one face scored above threshold
  Ambiguous,    // Multiple faces scored above threshold
  NotFound      // No face scored above threshold
};

struct FaceResolutionResult {
  FaceResolutionOutcome outcome = FaceResolutionOutcome::NotFound;
  std::vector<ResolvedFace> candidates;  // sorted by score descending
};

// ── Edge reference with geometric attestation ─────────────────────
//
// Stores enough witness data to re-identify an edge after topology
// changes.  Full-circle edges (drilling hole rims) additionally carry
// a circle witness — center + axis + radius.  A closed rim has
// start == end, so the endpoint witness alone cannot identify it; the
// circle witness is what makes rim references resolvable at all.
// Partial arcs are rejected at capture: a drilling rim must be a
// closed circle.

struct CamEdgeReference {
  // Which body this edge belongs to (body id from CompiledBody).
  std::string bodyId;

  std::array<double, 3> startPoint = {0.0, 0.0, 0.0};
  std::array<double, 3> endPoint = {0.0, 0.0, 0.0};
  double length = 0.0;
  std::array<double, 3> tangent = {1.0, 0.0, 0.0};

  // Circle witness — set only for full-circle edges.
  std::optional<std::array<double, 3>> center;
  std::optional<std::array<double, 3>> axis;
  std::optional<double> radius;

  // Human-readable label for debugging (e.g. "top rim of Box 1").
  std::string label;
};

// ── Edge resolution result ────────────────────────────────────────

struct ResolvedEdge {
  int edgeIndex = -1;     // 0-based index into the body's edge map
  double score = 0.0;     // 0.0 (no match) to 1.0 (perfect match)
};

enum class EdgeResolutionOutcome {
  Found,        // Exactly one edge scored above threshold
  Ambiguous,    // Multiple edges scored above threshold
  NotFound      // No edge scored above threshold
};

struct EdgeResolutionResult {
  EdgeResolutionOutcome outcome = EdgeResolutionOutcome::NotFound;
  std::vector<ResolvedEdge> candidates;  // sorted by score descending
};

// ── Public API ────────────────────────────────────────────────────

// Capture witness data from a live face. The face must belong to the
// given body shape. Returns nullopt if the face is null or sampling
// fails (degenerate surface, OCCT error).
std::optional<CamFaceReference> capture_face_reference(
    const std::string& bodyId,
    const TopoDS_Shape& bodyShape,
    int faceIndex,
    const std::string& label = "");

// Re-resolve a face reference against the current body shape.
// Walks every face, scores it against the witness data, and returns
// all candidates that pass the threshold, sorted by score.
FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const TopoDS_Shape& bodyShape);

// Convenience: resolve against a compiled body set from DocumentState.
// Finds the body matching reference.bodyId, compiles it, and resolves.
FaceResolutionResult resolve_face_reference(
    const CamFaceReference& reference,
    const DocumentState& document);

// Capture witness data from a live edge. The edge must belong to the
// given body shape. Lines and full circles are captured; partial arcs
// return nullopt (an arc is not a stable drilling input).
std::optional<CamEdgeReference> capture_edge_reference(
    const std::string& bodyId,
    const TopoDS_Shape& bodyShape,
    int edgeIndex,
    const std::string& label = "");

// Re-resolve an edge reference against the current body shape.
EdgeResolutionResult resolve_edge_reference(
    const CamEdgeReference& reference,
    const TopoDS_Shape& bodyShape);

// Convenience: resolve against a compiled body set from DocumentState.
EdgeResolutionResult resolve_edge_reference(
    const CamEdgeReference& reference,
    const DocumentState& document);

// ── Scoring parameters (tunable) ──────────────────────────────────

// Maximum angular deviation for normals (radians).
inline constexpr double kMaxNormalAngle = 0.087;  // ~5 degrees

// Score threshold above which a candidate is considered a match.
inline constexpr double kScoreThreshold = 0.7;

// Maximum center drift (mm) for an edge circle witness to keep scoring
// on the center-distance term; beyond this the term is 0.  The center
// Z is what separates the top and bottom rims of one bore, so this
// also sets how far apart two rims must be to stay unambiguous.
inline constexpr double kEdgeMaxCenterDistance = 5.0;

// Maximum endpoint drift (mm, summed over both endpoints) for an edge
// LINE witness to keep scoring on the endpoint term; beyond this the
// term is 0.  Together with length + direction the threshold tolerates
// small recompute drift while still degrading broken references.
inline constexpr double kEdgeMaxEndpointDistance = 5.0;

// ── Toolpath (runtime, not serialized) ────────────────────────────

struct CamToolpathPoint {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct CamToolpathMove {
  bool isRapid = false;  // G0 vs G1
  std::vector<CamToolpathPoint> points;
};

struct CamToolpath {
  std::vector<CamToolpathMove> moves;
  double minX = 0.0, maxX = 0.0;
  double minY = 0.0, maxY = 0.0;
  double minZ = 0.0, maxZ = 0.0;
  int totalPoints = 0;
};

}  // namespace polysmith::core
