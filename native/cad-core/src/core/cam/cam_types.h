#pragma once

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  TNP-Safe Geometry References
// ══════════════════════════════════════════════════════════════════

/// Axis-aligned bounding box (world coords, mm).
struct Bounds3D {
  double min_x = 0.0, min_y = 0.0, min_z = 0.0;
  double max_x = 0.0, max_y = 0.0, max_z = 0.0;
};

/// Witness data to re-identify a face after topology changes.
struct FaceAttestation {
  Bounds3D bounds;
  double area = 0.0;                              // mm² at capture time
  std::array<double, 3> normal = {0.0, 0.0, 1.0}; // surface normal at centre
  std::vector<std::array<double, 3>> sample_points; // 4-6 points on the face
};

/// Witness data to re-identify an edge after topology changes.
struct EdgeAttestation {
  std::array<double, 3> start_point = {0.0, 0.0, 0.0};
  std::array<double, 3> end_point   = {0.0, 0.0, 0.0};
  double length = 0.0;
  std::array<double, 3> tangent = {1.0, 0.0, 0.0};
  std::optional<std::vector<std::array<double, 3>>> adjacent_face_normals;
};

/// Witness data to re-identify a sketch profile region after sketch
/// edits.  Profile ids churn because refresh_sketch_profiles()
/// recomputes the whole region list; the geometry is the stable
/// identity.  All coordinates are sketch-local, so resolution compares
/// against freshly built regions in the same sketch frame.
struct SketchProfileAttestation {
  std::string sketch_feature_id;  // owning sketch feature
  std::string profile_id;         // last-known id (best effort only)
  double center_x = 0.0, center_y = 0.0;         // sketch-local centroid
  double area = 0.0;                             // sketch-local area (mm²)
  double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;  // bbox
  std::vector<std::string> boundary_edge_kinds;  // walk-order signature
  int inner_loop_count = 0;                      // holes
  std::optional<std::string> source_circle_id;   // circle-sourced regions
};

/// TNP-safe reference to a 3D face, 3D edge, or sketch profile.
struct GeometryReference {
  std::string persistent_id;
  std::variant<FaceAttestation, EdgeAttestation, SketchProfileAttestation>
      attestation;
  /// "warn_user" | "require_user" | "fail_operation" | "auto_resolve"
  std::string fallback_strategy = "warn_user";
};

// ══════════════════════════════════════════════════════════════════
//  Setup & Machine Definition
// ══════════════════════════════════════════════════════════════════

/// "bounding_box" | "cylinder" | "from_solid" | "from_mesh"
using StockType = std::string;

struct StockDefinition {
  StockType type = "bounding_box";
  std::optional<std::array<double, 3>> origin;    // for bounding_box / cylinder
  std::optional<std::array<double, 3>> size;      // for bounding_box
  std::optional<double> diameter;                 // for cylinder
  std::optional<double> length;                   // for cylinder
  double margin = 3.0;                            // extra material around part
  std::optional<GeometryReference> solid_reference;  // for from_solid
  std::optional<std::string> mesh_reference;         // for from_mesh (file ref)
};

/// Machine axis travel limits (mm for linear, degrees for rotary).
struct MachineAxes {
  double x = 500.0;
  double y = 400.0;
  double z = 300.0;
  std::optional<double> a;
  std::optional<double> b;
  std::optional<double> c;
};

/// Work Coordinate System origin anchored to a CAD face.
struct WcsOrigin {
  std::string feature_id;            // CAD feature that defines origin
  GeometryReference face_reference;  // TNP-safe face reference
  /// Last-resolved machine origin, refreshed by the CAM dependency
  /// pass so the post-processor needs no face resolution at export time.
  std::optional<std::array<double, 3>> position;
};

/// "3_axis_mill" | "4_axis_mill" | "5_axis_mill" | "lathe_2_axis" |
/// "lathe_live_tooling" | "laser" | "plasma" | "printer"
using MachineType = std::string;

/// CAM setup — one per fixturing orientation.
struct CamSetup {
  std::string setup_id;
  std::string name;
  MachineType machine_type = "3_axis_mill";
  MachineAxes machine_axes;
  StockDefinition stock;
  WcsOrigin wcs_origin;
  double safety_height = 50.0;   // mm
  double retract_height = 5.0;   // mm
  std::string units = "mm";      // "mm" | "inch"
};

// ══════════════════════════════════════════════════════════════════
//  Tool Library
// ══════════════════════════════════════════════════════════════════

/// "endmill_flat" | "endmill_ball" | "endmill_bull" | "drill" |
/// "facemill" | "chamfer" | "threadmill" | "turning_insert" |
/// "laser" | "plasma"
using ToolType = std::string;

struct ToolEntry {
  std::string tool_id;
  std::string name;
  ToolType type = "endmill_flat";

  // Geometry (mm).
  double diameter_mm = 6.0;
  double corner_radius_mm = 0.0;
  double flute_length_mm = 20.0;
  double overall_length_mm = 60.0;
  double shank_diameter_mm = 6.0;

  // Material & coating.
  std::string material = "carbide";   // "carbide" | "hss" | "diamond" | "ceramic" | "other"
  std::optional<std::string> coating; // "tin" | "ticn" | "alticn" | "dlc" | "none"
  bool coolant_through = false;

  // Machine limits.
  double max_spindle_rpm = 15000.0;

  // Default cutting parameters (overridable per-operation).
  double default_feedrate_mm_per_min = 1000.0;
  double default_plunge_feedrate_mm_per_min = 500.0;
  double default_stepdown_mm = 1.0;       // axial depth of cut
  double default_stepover_percent = 50.0; // radial engagement as %
};

// ══════════════════════════════════════════════════════════════════
//  CAM Operations
// ══════════════════════════════════════════════════════════════════

/// "face_milling" | "pocket_2d" | "contour_2d" | "slot" | "drilling" |
/// "adaptive_clearing" | "parallel_3d" | "contour_3d" | "chamfer" |
/// "thread_milling" | "engrave" | "laser_cut"
using CamOperationType = std::string;

/// "pending" | "generated" | "needs_regenerate" | "error" | "deleted"
using CamOperationStatus = std::string;

/// Geometry targeting for a CAM operation.
struct CamGeometryReferences {
  std::vector<GeometryReference> machining_regions;   // faces/edges to machine
  std::vector<GeometryReference> avoidance_regions;   // faces/edges to avoid
  std::vector<GeometryReference> guide_curves;        // for curve-driven ops
  std::vector<GeometryReference> check_surfaces;      // for collision checking
};

/// Drilling point location with TNP attestation.
struct CamPointLocation {
  std::string vertex_id;
  std::array<double, 3> position = {0.0, 0.0, 0.0};
  std::array<double, 3> surface_normal = {0.0, 0.0, 1.0};
  std::optional<double> hole_diameter;
  GeometryReference sample_face;
  std::string fallback_strategy = "warn_user";
};

/// "zigzag" | "one_way" | "offset" | "adaptive" | "spiral"
using ClearingStrategy = std::string;

/// "g81_standard" | "g82_dwell" | "g83_peck" | "g73_high_speed_peck" |
/// "g84_tap" | "g85_bore" | "g87_back_bore"
using DrillingCycleType = std::string;

/// Laser cutting parameters (only meaningful when type == "laser_cut").
/// Follows the per-type optional block pattern of the other strategy
/// fields so the operation struct stays one unified shape.
struct LaserCutParameters {
  double kerf_width_mm = 0.15;        // cut width compensation, both sides
  double lead_in_mm = 2.0;            // straight lead-in length
  double lead_out_mm = 2.0;           // straight lead-out length
  double pierce_dwell_seconds = 0.0;  // G4 dwell after pierce
  double power_percent = 85.0;        // 0..100; GRBL S = value * 10
  int passes = 1;                     // repeat contour (same path in v1)
  std::string mode = "cut";           // "cut" | "score" | "engrave"
  double material_thickness_mm = 3.0;
  double cut_plane_offset_mm = 0.0;   // cut-plane Z relative to sketch plane
  bool dynamic_power = true;          // true -> M4 (power scales with feed)
};

struct CamOperationParameters {
  // Basic cutting.
  double spindle_rpm = 8000.0;
  double feedrate_mm_per_min = 1200.0;
  double plunge_feedrate_mm_per_min = 600.0;
  std::optional<double> stepdown_mm;           // for 2.5D ops
  std::optional<double> stepover_percent;      // for face/pocket ops
  double stock_allowance_mm = 0.2;             // material left for finishing
  std::string cutting_direction = "climb";     // "climb" | "conventional" | "mixed"
  bool finish_pass = false;
  bool multiple_passes = false;

  // Strategy (per-type).
  std::optional<ClearingStrategy> strategy;         // for pocket/adaptive
  std::optional<DrillingCycleType> cycle_type;      // for drilling
  std::optional<double> hole_depth_mm;              // for drilling
  std::optional<double> peck_depth_mm;              // for peck drilling
  std::optional<double> dwell_seconds;              // for dwell cycles
  std::optional<double> engagement_angle_deg;       // for adaptive clearing
  std::optional<double> zigzag_angle_deg;           // for face milling
  std::optional<LaserCutParameters> laser;          // for laser_cut

  // Coolant.
  std::string coolant = "off";  // "off" | "flood" | "mist" | "through_tool"
};

/// Operation ordering dependencies.
struct CamOperationDependencies {
  std::vector<std::string> parent_operation_ids;
  std::optional<std::string> requires_operation_id;  // uses stock from this op
  bool use_stock_from_previous = false;
};

/// Cached toolpath metadata.
struct ExternalStorage {
  std::string format = "binary";   // "binary" | "json" | "gcode"
  std::string file_reference;      // path or ID in external store
  bool compressed = false;
  std::optional<int64_t> size_bytes;
};

struct ToolpathCache {
  std::string toolpath_id;
  std::string status = "up_to_date";  // "up_to_date" | "needs_regenerate" | "generating" | "error"
  std::optional<std::string> generated_at;  // ISO-8601
  std::optional<Bounds3D> bounds;
  std::optional<double> total_length_mm;
  std::optional<double> estimated_time_seconds;
  std::optional<int> num_moves;
  std::optional<int> num_rapids;
  std::optional<int> num_feeds;
  std::optional<ExternalStorage> external_storage;
};

/// Unified CAM operation.
struct CamOperation {
  std::string op_id;
  std::string name;
  CamOperationType type = "face_milling";
  bool enabled = true;
  std::string tool_id;                         // references ToolEntry::tool_id
  CamGeometryReferences geometry_references;
  std::vector<CamPointLocation> point_locations;  // for drilling
  CamOperationParameters parameters;
  CamOperationDependencies dependencies;
  std::optional<ToolpathCache> toolpath_cache;
  CamOperationStatus status = "pending";
  /// Human-readable degrade info (the CAM analogue of
  /// FeatureEntry::dependency_warning).  Empty when healthy.
  std::string status_message;
};

// ══════════════════════════════════════════════════════════════════
//  Post-Processor
// ══════════════════════════════════════════════════════════════════

/// "fanuc" | "linuxcnc" | "mach3" | "mach4" | "grbl" | "marlin" | "custom"
using PostProcessorType = std::string;

struct PostProcessorOptions {
  bool add_line_numbers = true;
  bool use_arcs = true;
  bool absolute_coordinates = true;
  int tool_change_mcode = 6;
  int spindle_start_mcode = 3;
  int coolant_mcode_on = 8;
  int coolant_mcode_off = 9;
  int decimal_places = 3;
  std::optional<bool> separate_rapids;
  std::optional<std::string> header_string;
  std::optional<std::string> footer_string;
};

struct PostProcessor {
  PostProcessorType type = "fanuc";
  std::string filename;
  PostProcessorOptions options;
};

// ══════════════════════════════════════════════════════════════════
//  Simulation Data
// ══════════════════════════════════════════════════════════════════

struct CollisionReport {
  std::string op_id;
  std::string tool_id;
  std::array<double, 3> position = {0.0, 0.0, 0.0};
  std::string severity = "warning";  // "warning" | "error" | "critical"
  std::string message;
};

struct SimulationData {
  std::optional<std::string> stock_after_op_id;
  std::optional<std::string> stock_mesh_reference;
  std::optional<std::string> last_verification;  // ISO-8601
  bool collisions_detected = false;
  std::vector<CollisionReport> collision_report;
};

// ══════════════════════════════════════════════════════════════════
//  Full CAM Section (held by DocumentState)
// ══════════════════════════════════════════════════════════════════

/// All CAM data stored in the document.
struct CamDocumentData {
  std::vector<CamSetup> setups;
  std::vector<ToolEntry> tool_library;
  std::vector<CamOperation> operations;
  std::optional<PostProcessor> post_processor;
  std::optional<SimulationData> simulation;
};

}  // namespace polysmith::core
