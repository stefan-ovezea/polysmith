#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

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

// ── Scoring parameters (tunable) ──────────────────────────────────

// Minimum fraction of sample points that must lie on a candidate face.
inline constexpr double kMinSampleHitRatio = 0.6;

// Maximum relative area difference for a candidate face.
inline constexpr double kMaxAreaRatio = 0.3;  // ±30%

// Maximum angular deviation for normals (radians).
inline constexpr double kMaxNormalAngle = 0.087;  // ~5 degrees

// Score threshold above which a candidate is considered a match.
inline constexpr double kScoreThreshold = 0.7;

// ── CAM Setup & Stock ──────────────────────────────────────────
//
// Setup is document-level configuration, not a regular operation.
// It must exist before any milling operation can be created.
// Stock defines the raw material bounding box the part is cut from.

struct CamStockDefinition {
  // Bounding box dimensions (mm).
  double width = 100.0;
  double height = 100.0;
  double depth = 20.0;
  // Extra material beyond the part bounds on each axis.
  double offsetX = 5.0;
  double offsetY = 5.0;
  double offsetZ = 5.0;
};

struct CamSetup {
  std::string id;
  CamStockDefinition stock;
  // Work Coordinate System origin in world coordinates.
  double wcsOriginX = 0.0;
  double wcsOriginY = 0.0;
  double wcsOriginZ = 0.0;
  // Z height for rapid moves between operations.
  double safetyPlaneZ = 10.0;
  // 3-axis only for v1.
  int axisCount = 3;
};

// ── Tool library ──────────────────────────────────────────────

struct CamToolDefinition {
  std::string id;
  std::string name;
  int toolNumber = 1;

  // Geometry (mm).
  double diameter = 6.0;
  double fluteLength = 20.0;
  double overallLength = 60.0;
  double cornerRadius = 0.0;

  // Cutting data (defaults, overridable per-operation).
  double spindleSpeed = 12000;   // RPM
  double feedRate = 1000;        // mm/min
  double stepover = 2.0;         // mm
  double stepdown = 1.0;         // mm

  // Type hint for UI grouping.
  enum class Type { EndMill, BallMill, Drill, VBit, FaceMill, ChamferMill, ThreadMill };
  Type type = Type::EndMill;
};

// ── Toolpath (generated geometry, not serialized) ──────────────

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

// ── CAM operation types ────────────────────────────────────────

enum class CamOperationType {
  FaceMilling,
  Pocket,
  Contour,
  Drill,
  AdaptiveClearing
};

// ── Face milling parameters ────────────────────────────────────

struct FaceMillingParams {
  double depth = 0.5;         // mm — depth of cut
  double stepover = 2.0;      // mm — distance between passes
  double angleDeg = 0.0;      // degrees — zigzag angle (0 = along X)
};

// ── Unified CAM operation ──────────────────────────────────────

struct CamOperationEntry {
  std::string id;
  std::string name;
  CamOperationType type = CamOperationType::FaceMilling;
  std::string toolId;
  // TNP witness data for the target face.
  std::string bodyId;
  int faceIndex = -1;
  std::vector<std::array<double, 3>> samplePoints;
  double capturedArea = 0.0;
  std::array<double, 3> capturedNormal = {0.0, 0.0, 1.0};
  // Per-type parameters.
  std::optional<FaceMillingParams> faceMilling;
  // Cached toolpath — invalidated when inputs change.
  bool toolpathValid = false;
};

// ── Toolpath generation (declared here, implemented per-operation) ─

struct DocumentState;
struct CamOperationEntry;

CamToolpath generate_face_milling_toolpath(
    const CamOperationEntry& op,
    const DocumentState& document);

}  // namespace polysmith::core
