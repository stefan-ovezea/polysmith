#pragma once

#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

#include "core/document/plane_frame.h"

namespace polysmith::core {

struct HoleFeatureParameters {
  std::string target_body_id;
  std::string source_face_id;
  PlaneFrame plane_frame;
  double center_x = 0.0;
  double center_y = 0.0;
  std::string hole_type = "simple";
  std::string extent_type = "blind";
  double diameter = 5.0;
  double depth = 10.0;
  double counterbore_diameter = 8.0;
  double counterbore_depth = 2.0;
  double countersink_diameter = 8.0;
  double countersink_angle_degrees = 82.0;
  std::string standard = "custom";
  std::string standard_size;
  std::string hole_fit = "clearance";
  bool thread_enabled = false;
  std::string thread_spec;
  double thread_pitch = 0.0;
  double major_diameter = 0.0;
  double minor_diameter = 0.0;
  double thread_depth = 10.0;
  std::string thread_representation = "cosmetic";
  bool is_pending = false;
};

struct HelixFeatureParameters {
  std::string axis_source_id;
  double axis_start_x = 0.0;
  double axis_start_y = 0.0;
  double axis_start_z = 0.0;
  double axis_end_x = 0.0;
  double axis_end_y = 0.0;
  double axis_end_z = 1.0;
  double radius = 2.5;
  double pitch = 1.0;
  double height = 10.0;
  double turns = 10.0;
  std::string handedness = "right";
  double start_angle_degrees = 0.0;
  std::vector<double> points;
};

struct ThreadFeatureParameters {
  std::string target_body_id;
  std::string axis_source_id;
  std::string mode = "external";
  std::string standard = "custom";
  std::string size;
  double major_diameter = 5.0;
  double minor_diameter = 4.0;
  double pitch = 0.8;
  double length = 10.0;
  double thread_angle_degrees = 60.0;
  double start_offset = 0.0;
  std::string handedness = "right";
  std::string representation = "cosmetic";
  bool is_pending = false;
};

struct FastenerFeatureParameters {
  std::string standard = "metric";
  std::string size = "M5";
  double diameter = 5.0;
  double minor_diameter = 4.2;
  double pitch = 0.8;
  double length = 20.0;
  double thread_length = 16.0;
  std::string head_type = "socket_head";
  std::string drive_type = "hex_socket";
  std::string thread_representation = "cosmetic";
};

struct MoveFeatureParameters {
  std::string target_body_id;
  double translation_x = 0.0;
  double translation_y = 0.0;
  double translation_z = 0.0;
  double rotation_x_degrees = 0.0;
  double rotation_y_degrees = 0.0;
  double rotation_z_degrees = 0.0;
  bool is_pending = false;
};

struct BodyCopyFeatureParameters {
  std::string source_body_id;
  std::string copy_mode = "linked";
  std::string source_body_name;
  std::string serialized_shape;
  double local_x_axis_x = 1.0;
  double local_x_axis_y = 0.0;
  double local_x_axis_z = 0.0;
  double local_y_axis_x = 0.0;
  double local_y_axis_y = 1.0;
  double local_y_axis_z = 0.0;
  double local_z_axis_x = 0.0;
  double local_z_axis_y = 0.0;
  double local_z_axis_z = 1.0;
};

struct FilletFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> edge_ids;
  double radius;
  bool is_pending = false;
};

struct ChamferFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> edge_ids;
  double distance;
  bool is_pending = false;
};

struct ShellFeatureParameters {
  std::string target_body_id;
  std::vector<std::string> removed_face_ids;
  double thickness;
  bool is_pending = false;
};

// Imported STL mesh body.  Only the source path is persisted — the
// mesh itself is re-read from disk at every compile, so the document
// stays small and the feature degrades gracefully (dependency_broken)
// when the file moves or is deleted.
struct MeshImportFeatureParameters {
  std::string file_path;
  double scale = 1.0;  // STL is unitless; assumed mm, scaled by this.
};

// Converts a mesh_import body into a regular solid body alongside the
// mesh body (the mesh body itself is kept). The converted solid is
// snapshotted at creation (same pattern as a standalone body copy), so
// it is independent afterwards — the source mesh body can be deleted
// without losing it.
struct MeshToBodyFeatureParameters {
  std::string source_body_id;     // mesh body the conversion came from
  std::string serialized_shape;   // B-rep snapshot of the converted solid
};

// Imported STEP body. The file is parsed ONCE at import time; the
// translated shape (in mm) is kept as a live handle for compile-time
// use, and a B-rep snapshot is persisted into the part file so the
// document stays self-contained. The source path is provenance only —
// moving or deleting the .step file never breaks the part (no
// dependency_broken machinery, unlike mesh_import).
struct StepImportFeatureParameters {
  std::string file_path;         // original file — display only
  std::string source_units;      // units the file was written in (e.g. "INCH")
  std::string serialized_shape;  // B-rep snapshot for persistence
  TopoDS_Shape imported_shape;   // live handle; null after load — the
                                 // compiler falls back to the snapshot
  int solid_count = 0;           // for the feature summary
  int face_count = 0;            // for the feature summary
};

}  // namespace polysmith::core
