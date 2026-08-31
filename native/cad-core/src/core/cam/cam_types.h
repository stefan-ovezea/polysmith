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

/// "zigzag" | "one_way" | "offset" | "adaptive" | "spiral"
using ClearingStrategy = std::string;

/// "g81_standard" | "g82_dwell" | "g83_peck" | "g73_high_speed_peck" |
/// "g84_tap" | "g85_bore" | "g87_back_bore"
using DrillingCycleType = std::string;

/// Laser cutting parameters (only meaningful when type == "laser_cut").
/// Follows the per-type optional block pattern of the other strategy
/// fields so the operation struct stays one unified shape.
struct LaserCutParameters {
  // Process.
  std::string mode = "cut";  // "cut" | "score" | "engrave" — VALIDATED
  double power_percent = 85.0;  // 0..100; S scaling is power_max-driven
  std::optional<double> speed_mm_per_s;  // laser-native speed (mm/s);
                                         // absent → legacy feedrate fallback
  int passes = 1;              // contour repetitions, laser stays on
  bool dynamic_power = true;   // true -> M4 (power scales with feed)
  bool air_assist = false;     // M8/M9 around cuts (post-supported)

  // Kerf.
  double kerf_width_mm = 0.15;  // full cut width; halved per side
  std::string kerf_side = "auto";  // "auto" (holes inward, outers outward)
                                   // | "outside" | "inside" | "none"

  // Leads.
  double lead_in_mm = 2.0;
  double lead_out_mm = 2.0;
  std::string lead_in_style = "line";   // "line" | "arc" (tangent roll-in)
  std::string lead_out_style = "line";  // "line" | "arc"
  double lead_in_angle_deg = 0.0;       // entry angle vs contour tangent
  double lead_out_angle_deg = 0.0;      // exit angle vs contour tangent
                                         // (0 = tangent continuation)
  double overcut_mm = 0.0;              // extend past the start/end joint

  // Pierce.
  double pierce_dwell_seconds = 0.1;  // G4 dwell after pierce
  std::string pierce_position = "auto";  // "auto" | "lead_start" |
                                         // "nearest_centroid"

  // Tabs / bridges.
  bool tabs_enabled = false;
  double tab_width_mm = 0.5;     // tab length along the cut
  double tab_spacing_mm = 20.0;  // even distribution along the loop
  double tab_power_percent = 0.0;  // 0 = laser off over tab; >0 = micro-joint
  bool tabs_on_holes = false;    // standard: outer contours only

  // Engrave fill / hatch.
  std::string engrave_style = "line";  // "line" (contour trace) | "fill"
  double line_spacing_mm = 0.1;        // hatch spacing
  double fill_angle_deg = 0.0;         // hatch direction
  bool fill_bidirectional = true;      // scan without travel-back passes

  // Cut plane / material.
  double material_thickness_mm = 3.0;
  double cut_plane_offset_mm = 0.0;  // cut-plane Z relative to sketch plane

  // Ordering.
  std::string cut_order = "inner_first";  // "inner_first" |
                                          // "nearest_neighbor" | "by_area"
};

/// Laser machine settings — the physical machine, not the job.
/// Stored once per document (multi-setup work splits this later).
struct LaserMachineSettings {
  // Bed travel extents.  Used to validate that test patterns (and
  // later, job bounds) fit the machine.
  double work_area_x_mm = 400.0;
  double work_area_y_mm = 400.0;
  // The red pointer sits at this offset from the laser focal point:
  //   dot_position = laser_position + pointer_offset.
  // Users frame parts under the DOT, so the exporter shifts every
  // coordinate by -offset — the cut lands where the dot was.
  double pointer_offset_x_mm = 0.0;
  double pointer_offset_y_mm = 0.0;
};

/// Laser test-pattern parameters (type == "laser_test_pattern").
/// LightBurn-style material test cards: a grid of cells sweeping power
/// along columns (ascending left→right) and speed along rows
/// (ascending top→bottom) — read the best cell off the card.
struct LaserTestPatternParameters {
  std::string pattern = "engrave_grid";  // "engrave_grid" | "cut_grid" |
                                         // "kerf_gauge"
  double power_min_percent = 10.0;
  double power_max_percent = 100.0;
  int power_steps = 5;
  double speed_min_mm_per_s = 5.0;
  double speed_max_mm_per_s = 50.0;
  int speed_steps = 5;
  double cell_size_mm = 10.0;
  double cell_spacing_mm = 5.0;
  double start_x_mm = 5.0;
  double start_y_mm = 5.0;
  double line_spacing_mm = 0.1;  // engrave fill density
  // kerf_gauge only: the calibration square is cut with the CURRENT
  // kerf/power/speed so the measured plug reveals the true kerf
  // (kerf = (cell − plug) / 2).
  double kerf_width_mm = 0.15;
  double power_percent = 85.0;
  double speed_mm_per_s = 10.0;
  // Grid cards: engrave "P.. S.." labels under every cell.
  bool cell_labels = true;
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
  std::optional<LaserTestPatternParameters> test_pattern;  // laser_test_pattern

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
  /// Fixturing setup this operation belongs to.  Empty = the first
  /// setup (legacy documents created before multi-setup support).
  std::string setup_id;
  std::string tool_id;                         // references ToolEntry::tool_id
  CamGeometryReferences geometry_references;
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

/// The selected post processor.  Output shaping (templates, S scale,
/// decimals) comes from the DEFINITION FILE — posts are first-class
/// user-editable files, not per-document options.
struct PostProcessor {
  PostProcessorType type = "fanuc";
  std::string filename;
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
  std::optional<LaserMachineSettings> machine_settings;
};

}  // namespace polysmith::core
