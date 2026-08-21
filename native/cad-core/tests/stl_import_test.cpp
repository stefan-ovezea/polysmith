// Regression test for the STL import + mesh body features:
//   - import_stl (path-referenced mesh body, re-read from disk)
//   - project_body_into_sketch (section + silhouette modes)
//   - convert_mesh_to_body (mesh -> regular solid alongside)
//   - dependency_broken degradation when the source STL disappears
//   - serialization round-trip of the new feature parameters.
//
// The fixtures write real binary STL files into the temp directory
// (BRepPrimAPI_MakeBox -> BRepMesh_IncrementalMesh -> StlAPI_Writer),
// so every path here exercises the same reader the core uses.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Poly_Triangulation.hxx>
#include <StlAPI_Writer.hxx>
#include <TopLoc_Location.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <nlohmann/json.hpp>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "core/geometry/face_geometry.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/mesh_projection.h"
#include "core/viewport/viewport.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

int count_faces(const TopoDS_Shape& shape) {
  int count = 0;
  for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    ++count;
  }
  return count;
}

bool shape_has_solid(const TopoDS_Shape& shape) {
  for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
    return true;
  }
  return false;
}

// Writes a binary STL of a 40x20x10 box with its bottom at `z_min`
// and returns the file path. Mid-height boxes (z_min = -5) put the
// XY plane through the middle; z_min = 0 puts the bottom cap IN the
// XY plane (the coplanar-section case).
std::string write_box_stl(const std::string& name, double z_min) {
  const TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, z_min), 40.0, 20.0, 10.0)
          .Shape();
  BRepMesh_IncrementalMesh mesher(box, /*linearDeflection=*/0.1,
                                  /*isRelative=*/false,
                                  /*angularDeflection=*/0.5,
                                  /*isInParallel=*/false);
  (void)mesher;
  const auto path = std::filesystem::temp_directory_path() /
                    ("polysmith_stl_import_" + name + ".stl");
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  writer.Write(box, path.string().c_str());
  return path.string();
}

// Writes a binary STL of an L-shaped plate (two fused boxes, 5 mm
// tall, bottom cap in the z=0 plane) and returns the file path. The
// bottom face boundary has 6 corners; the triangulation splits its
// edges into several collinear segments each.
std::string write_l_shape_stl(const std::string& name) {
  const TopoDS_Shape plate =
      BRepAlgoAPI_Fuse(
          BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, 0.0), 40.0, 20.0, 5.0)
              .Shape(),
          BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -20.0, 0.0), 20.0, 10.0, 5.0)
              .Shape())
          .Shape();
  BRepMesh_IncrementalMesh mesher(plate, /*linearDeflection=*/0.1,
                                  /*isRelative=*/false,
                                  /*angularDeflection=*/0.5,
                                  /*isInParallel=*/false);
  (void)mesher;
  const auto path = std::filesystem::temp_directory_path() /
                    ("polysmith_stl_import_" + name + ".stl");
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  writer.Write(plate, path.string().c_str());
  return path.string();
}

bool test_import_compiles_body() {
  const std::string path = write_box_stl("import_compiles", -5.0);

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_stl(path, 1.0);

  const auto& feature = document.feature_history.back();
  if (!expect(feature.kind == "mesh_import" &&
                  feature.mesh_import_parameters.has_value() &&
                  feature.mesh_import_parameters->file_path == path,
              "import: feature must be mesh_import with the source path")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "import: exactly one compiled body")) {
    return false;
  }
  if (!expect(count_faces(compiled.bodies.front().shape) >= 12,
              "import: body must be the box mesh (>= 12 faces)")) {
    return false;
  }

  const auto viewport =
      polysmith::core::build_viewport_state(std::optional<DocumentState>(document));
  return expect(!viewport.meshes.empty(),
                "import: viewport must emit the mesh body");
}

bool test_section_projection_ref_plane() {
  const std::string path = write_box_stl("section_mid", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  // Origin ref-plane sketch: no stored plane_frame — exercises the
  // origin-frame fallback in resolve_sketch_projection_frame.
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(sketch.lines.size() == 4,
              "section: XY section of the box must be 4 lines")) {
    return false;
  }
  for (const auto& line : sketch.lines) {
    if (!expect(line.start_vertex_id.empty() ||
                    std::any_of(sketch.vertices.begin(), sketch.vertices.end(),
                                [&](const auto& v) {
                                  return v.id == line.start_vertex_id &&
                                         v.is_fixed;
                                }),
                "section: line endpoints must be fixed")) {
      return false;
    }
  }
  if (!expect(sketch.projections.size() == 1 &&
                  sketch.projections.front().source_id ==
                      "body:" + body_id + ":section" &&
                  sketch.projections.front().source_kind == "body",
              "section: one body projection record with the mode in "
              "its source id")) {
    return false;
  }
  if (!expect(sketch.dimensions.empty(),
              "section: projected lines must not auto-create dimensions")) {
    return false;
  }

  // The 4 lines must form the 40x20 rectangle (any orientation).
  double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
  for (const auto& line : sketch.lines) {
    min_x = std::min({min_x, line.start_x, line.end_x});
    max_x = std::max({max_x, line.start_x, line.end_x});
    min_y = std::min({min_y, line.start_y, line.end_y});
    max_y = std::max({max_y, line.start_y, line.end_y});
  }
  return expect(std::abs((max_x - min_x) - 40.0) < 1e-3 &&
                    std::abs((max_y - min_y) - 20.0) < 1e-3,
                "section: projected loop must span 40x20");
}

bool test_section_coplanar_fallback() {
  const std::string path = write_box_stl("section_coplanar", 0.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(!sketch.lines.empty(),
              "coplanar section: the bottom-cap outline must project")) {
    return false;
  }
  double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
  for (const auto& line : sketch.lines) {
    min_x = std::min({min_x, line.start_x, line.end_x});
    max_x = std::max({max_x, line.start_x, line.end_x});
    min_y = std::min({min_y, line.start_y, line.end_y});
    max_y = std::max({max_y, line.start_y, line.end_y});
  }
  return expect(std::abs((max_x - min_x) - 40.0) < 1e-3 &&
                    std::abs((max_y - min_y) - 20.0) < 1e-3,
                "coplanar section: outline must span 40x20");
}

bool test_silhouette_outline() {
  const std::string path = write_box_stl("silhouette_mid", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");

  // The silhouette computed on the freshly-read shape must match the
  // compiled shape (build_mesh_import_shape meshes the faces — an
  // unmeshed shape would yield an empty outline).
  {
    const auto body_it = std::find_if(
        document.feature_history.begin(), document.feature_history.end(),
        [&](const auto& feature) { return feature.id == body_id; });
    const TopoDS_Shape fresh = polysmith::core::build_feature_shape(*body_it);
    const auto polylines = polysmith::core::compute_mesh_silhouette_polylines(
        fresh, polysmith::core::PlaneFrame{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1});
    if (!expect(polylines.size() == 1,
                "silhouette: freshly-read shape must yield one outline "
                "loop")) {
      return false;
    }
  }

  document = manager.project_body_into_sketch(body_id, "silhouette");

  {
    const auto& sketch =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(sketch.lines.size() == 4,
                "silhouette: box outline along +Z must be 4 lines")) {
      return false;
    }
    if (!expect(sketch.dimensions.empty(),
                "silhouette: projected lines must not auto-create "
                "dimensions")) {
      return false;
    }
  }

  // NOTE: projecting the CONVERTED body (mesh_to_body) is supported by
  // project_body_into_sketch and verified against the real fan-panel
  // STL via manual probes; the in-suite test of that path crashes with
  // a latent heap corruption in the convert/no-op sequence that has
  // not been isolated yet (see Implementation Log).


  // The first projection's 4 lines still span the 40x20 footprint.
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
  for (const auto& line : sketch.lines) {
    min_x = std::min({min_x, line.start_x, line.end_x});
    max_x = std::max({max_x, line.start_x, line.end_x});
    min_y = std::min({min_y, line.start_y, line.end_y});
    max_y = std::max({max_y, line.start_y, line.end_y});
  }
  return expect(std::abs((max_x - min_x) - 40.0) < 1e-3 &&
                    std::abs((max_y - min_y) - 20.0) < 1e-3,
                "silhouette: outline must span the 40x20 footprint");
}

bool test_both_modes_coexist() {
  const std::string path = write_box_stl("both_modes", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  document = manager.project_body_into_sketch(body_id, "silhouette");

  // A box's section and silhouette are the SAME outline — the
  // duplicate-segment guard must treat the second projection as a
  // no-op (inserting coincident lines corrupts the sketch).
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  return expect(sketch.lines.size() == 4,
                "both modes: identical second projection must be a no-op "
                "(4 lines)") &&
         expect(sketch.projections.size() == 1 &&
                    sketch.projections[0].source_id ==
                        "body:" + body_id + ":section",
                "both modes: only the first projection record is kept");
}

bool test_convert_creates_solid_alongside() {
  const std::string path = write_box_stl("convert", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.convert_mesh_to_body(body_id);

  const auto converted_it_doc = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) { return feature.kind == "mesh_to_body"; });
  if (!expect(converted_it_doc != document.feature_history.end() &&
                  converted_it_doc->mesh_to_body_parameters.has_value() &&
                  !converted_it_doc->mesh_to_body_parameters->serialized_shape
                       .empty(),
              "convert: converted solid must be snapshotted at creation")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 2,
              "convert: mesh body AND converted body must both exist")) {
    return false;
  }
  const auto converted_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id != body_id; });
  if (!expect(converted_it != compiled.bodies.end(),
              "convert: converted body must be a separate body")) {
    return false;
  }
  if (!expect(shape_has_solid(converted_it->shape),
              "convert: converted body must contain a solid")) {
    return false;
  }
  if (!expect(count_faces(converted_it->shape) <= 12,
              "convert: unify must merge the 12 triangles into <= 12 "
              "planar faces")) {
    return false;
  }

  // The converted solid is independent (creation-time snapshot): the
  // mesh body can be deleted and the solid must survive.
  document = manager.delete_feature(body_id);
  const auto after_delete = compile_bodies(document);
  if (!expect(after_delete.bodies.size() == 1,
              "convert: deleting the mesh body must leave exactly the "
              "converted solid")) {
    return false;
  }
  return expect(shape_has_solid(after_delete.bodies.front().shape),
                "convert: the surviving body must still be a solid");
}

// Writes a binary STL of the box with INVERTED triangle winding
// (reversed solid), the orientation-inconsistent class of STL files.
std::string write_reversed_box_stl(const std::string& name) {
  const TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0), 40.0, 20.0, 10.0)
          .Shape()
          .Reversed();
  BRepMesh_IncrementalMesh mesher(box, /*linearDeflection=*/0.1,
                                  /*isRelative=*/false,
                                  /*angularDeflection=*/0.5,
                                  /*isInParallel=*/false);
  (void)mesher;
  const auto path = std::filesystem::temp_directory_path() /
                    ("polysmith_stl_import_" + name + ".stl");
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  writer.Write(box, path.string().c_str());
  return path.string();
}

bool test_converted_solid_orientation_normalized() {
  const std::string path = write_reversed_box_stl("convert_reversed");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);

  const auto compiled = compile_bodies(document);
  const auto converted_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id != body_id; });
  if (!expect(converted_it != compiled.bodies.end() &&
                  shape_has_solid(converted_it->shape),
              "orientation: converted body must contain a solid")) {
    return false;
  }
  // Inverted STL winding must NOT yield a negative-volume solid — the
  // converter normalizes orientation (negative volume breaks booleans
  // downstream).
  GProp_GProps props;
  BRepGProp::VolumeProperties(converted_it->shape, props);
  const double volume = props.Mass();
  return expect(volume > 0.0 && std::abs(volume - 8000.0) < 8000.0 * 0.02,
                "orientation: converted volume must be positive and exact");
}

bool test_missing_file_dependency_broken() {
  const std::string path_a = write_box_stl("broken_a", -5.0);
  const std::string path_b = write_box_stl("broken_b", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path_a, 1.0);
  const std::string body_a_id = document.feature_history.back().id;

  // Remove the source file, then bump the revision with a second import
  // so the dependency walker re-checks the first body.
  std::filesystem::remove(path_a);
  document = manager.import_stl(path_b, 1.0);

  const auto broken_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == body_a_id; });
  if (!expect(broken_it != document.feature_history.end() &&
                  broken_it->dependency_broken &&
                  !broken_it->dependency_warning.empty(),
              "broken: missing STL must mark the feature dependency_broken")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  const bool body_a_present = std::any_of(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == body_a_id; });
  if (!expect(!body_a_present,
              "broken: the body with the missing file must drop out of "
              "compilation")) {
    return false;
  }

  // Restore the file (same path) and bump again — the body recovers.
  write_box_stl("broken_a", -5.0);  // re-writes the same path
  document = manager.add_box_feature(
      polysmith::core::BoxFeatureParameters{1.0, 1.0, 1.0});

  const auto healed_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == body_a_id; });
  return expect(healed_it != document.feature_history.end() &&
                    !healed_it->dependency_broken,
                "broken: restoring the file must heal the dependency");
}

// 0-based face index of the body's bottom face (all vertices at z=0).
int find_bottom_face_index(const TopoDS_Shape& shape) {
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(shape, TopAbs_FACE, face_map);
  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    bool all_at_z0 = true;
    for (TopExp_Explorer vexp(face, TopAbs_VERTEX); vexp.More();
         vexp.Next()) {
      if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vexp.Current())).Z()) >
          1e-6) {
        all_at_z0 = false;
        break;
      }
    }
    if (all_at_z0) {
      return i - 1;
    }
  }
  return -1;
}

bool test_douglas_peucker_simplification() {
  // Dense chord sampling of a quarter circle r=50: DP at 0.05 mm
  // decimates 91 points to roughly one per ~5 degrees while keeping
  // the endpoints, and every original point stays within tolerance of
  // the simplified loop (DP's guarantee).
  std::vector<polysmith::core::FaceOutlinePoint> arc;
  for (int i = 0; i <= 90; ++i) {
    const double angle = i * 3.141592653589793 / 180.0;
    arc.push_back({50.0 * std::cos(angle), 50.0 * std::sin(angle), 0.0});
  }
  const auto simplified =
      polysmith::core::simplify_outline_polyline(arc, /*tolerance=*/0.05);
  if (!expect(simplified.size() >= 24 && simplified.size() <= 40,
              "dp: quarter-circle arc must decimate to ~24-40 points")) {
    std::cerr << "  got " << simplified.size() << " points\n";
    return false;
  }
  if (!expect(simplified.front().x == arc.front().x &&
                  simplified.front().y == arc.front().y &&
                  simplified.back().x == arc.back().x &&
                  simplified.back().y == arc.back().y,
              "dp: endpoints must survive")) {
    return false;
  }

  // Every original point must lie within tolerance of the simplified
  // loop (closed: the last segment returns to the first point).
  auto distance_to_segment = [](const polysmith::core::FaceOutlinePoint& p,
                                const polysmith::core::FaceOutlinePoint& a,
                                const polysmith::core::FaceOutlinePoint& b) {
    const double abx = b.x - a.x, aby = b.y - a.y;
    const double ab_sq = abx * abx + aby * aby;
    const double apx = p.x - a.x, apy = p.y - a.y;
    const double t =
        ab_sq <= 1e-24 ? 0.0 : std::clamp((apx * abx + apy * aby) / ab_sq, 0.0, 1.0);
    const double cx = a.x + t * abx - p.x, cy = a.y + t * aby - p.y;
    return std::sqrt(cx * cx + cy * cy);
  };
  double max_deviation = 0.0;
  const size_t m = simplified.size();
  for (const auto& point : arc) {
    double nearest = 1e9;
    for (size_t k = 0; k < m; ++k) {
      const auto& a = simplified[k];
      const auto& b = simplified[(k + 1) % m];
      nearest = std::min(nearest, distance_to_segment(point, a, b));
    }
    max_deviation = std::max(max_deviation, nearest);
  }
  if (!expect(max_deviation <= 0.05 + 1e-6,
              "dp: no original point may deviate beyond the tolerance")) {
    std::cerr << "  max deviation " << max_deviation << "\n";
    return false;
  }

  // Zero tolerance must keep everything.
  return expect(
      polysmith::core::simplify_outline_polyline(arc, /*tolerance=*/0.0)
              .size() == arc.size(),
      "dp: zero tolerance keeps every point");
}

bool test_detach_projection_keeps_sketch_on_delete() {
  const std::string path = write_box_stl("detach", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  {
    const auto& sketch =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(sketch.lines.size() == 4 && sketch.projections.size() == 1,
                "detach: projection must land before detaching")) {
      return false;
    }
  }

  document = manager.detach_body_projections(body_id);
  {
    const auto& sketch =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(sketch.lines.size() == 4 && sketch.projections.empty(),
                "detach: projections must be removed, lines kept")) {
      return false;
    }
  }

  // The source body can now be deleted — the sketch (and the extrude
  // built on it) must survive.
  document = manager.delete_feature(body_id);
  const auto compiled_after = compile_bodies(document);
  if (!expect(compiled_after.bodies.empty(),
              "detach: deleting the source body leaves no mesh bodies")) {
    return false;
  }
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(sketch.lines.size() == 4 && !sketch.projections.empty() == false,
              "detach: sketch lines survive the body deletion")) {
    return false;
  }
  const auto& profiles = sketch.profiles;
  if (!expect(!profiles.empty(),
              "detach: projected loop must remain a sketch profile")) {
    return false;
  }
  document = manager.extrude_profile(profiles.front().id, /*depth=*/5.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled_extrude = compile_bodies(document);
  return expect(compiled_extrude.bodies.size() == 1,
                "detach: extrude from the detached sketch must compile");
}

bool test_deleting_body_freezes_construction_plane() {
  // The L-shape fixture's bottom cap lies in the z=0 plane.
  const std::string path = write_l_shape_stl("freeze_plane");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  const auto compiled = compile_bodies(document);
  const auto converted_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == converted_id; });
  const int bottom_index = find_bottom_face_index(converted_it->shape);
  if (!expect(bottom_index >= 0, "freeze plane: bottom face must exist")) {
    return false;
  }
  const std::string face_id =
      converted_id + ":face:" + std::to_string(bottom_index);

  document = manager.create_offset_plane(face_id, /*offset=*/0.0);
  const std::string plane_id = document.feature_history.back().id;
  const auto frame_before =
      document.feature_history.back()
          .construction_plane_parameters->plane_frame;

  // Delete the body — the plane must freeze at its cached frame with
  // no broken reference.
  document = manager.delete_feature(converted_id);
  const auto plane_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == plane_id; });
  if (!expect(plane_it != document.feature_history.end() &&
                  plane_it->kind == "construction_plane" &&
                  plane_it->construction_plane_parameters.has_value(),
              "freeze plane: plane feature must survive")) {
    return false;
  }
  const auto& params = plane_it->construction_plane_parameters.value();
  if (!expect(params.plane_type == "detached" &&
                  params.source_plane_id.empty() && !plane_it->dependency_broken,
              "freeze plane: plane must be detached and healthy after "
              "its source body is deleted")) {
    return false;
  }
  // The frame is the bottom-face plane (origin on z=0, normal ±Z) and
  // must survive the freeze unchanged.
  const auto& frame = params.plane_frame;
  return expect(std::abs(frame.origin_z) < 1e-9 &&
                    std::abs(std::abs(frame.normal_z) - 1.0) < 1e-9 &&
                    frame.origin_x == frame_before.origin_x &&
                    frame.origin_y == frame_before.origin_y &&
                    frame.origin_z == frame_before.origin_z &&
                    frame.normal_x == frame_before.normal_x &&
                    frame.normal_y == frame_before.normal_y &&
                    frame.normal_z == frame_before.normal_z,
                "freeze plane: cached frame must survive the freeze");
}

bool test_detach_removes_face_projections() {
  const std::string path = write_l_shape_stl("detach_face");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  const auto compiled = compile_bodies(document);
  const auto converted_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == converted_id; });
  const int bottom_index = find_bottom_face_index(converted_it->shape);
  if (!expect(bottom_index >= 0, "detach face: bottom face must exist")) {
    return false;
  }
  const std::string face_id =
      converted_id + ":face:" + std::to_string(bottom_index);

  // Face-based sketch on the bottom face, then project the face.
  document = manager.start_sketch_on_face(
      face_id, polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
                   0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0,
                   1.0});
  document = manager.project_face_into_sketch(face_id);
  {
    const auto& sketch =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(sketch.projections.size() == 1 &&
                    sketch.projections.front().source_id == face_id,
                "detach face: face projection must land")) {
      return false;
    }
  }

  document = manager.detach_body_projections(converted_id);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  return expect(sketch.projections.empty() && sketch.lines.size() == 6 &&
                    sketch.dimensions.empty(),
                "detach face: face-sourced projection must be removed, "
                "6 merged lines kept, no auto dimensions");
}

bool test_converted_body_projection_no_op() {
  // Regression sequence from the Implementation Log (2026-08-18) known
  // issue: import -> ref-plane sketch -> body projection -> convert ->
  // project the CONVERTED body (duplicate-segment no-op) -> read sketch.
  // This exact sequence crashed with a heap corruption (0xC0000409) in
  // the in-suite layout only; the test restores it so the fix is pinned.
  const std::string path = write_box_stl("convert_noop", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  // The converted solid shares the mesh's z=0 cross-section and its +Z
  // silhouette — both projections must hit the duplicate-segment guard
  // and no-op instead of inserting coincident lines.
  document = manager.project_body_into_sketch(converted_id, "section");
  document = manager.project_body_into_sketch(converted_id, "silhouette");

  // The sketch is no longer the last feature (convert appended one) —
  // look it up by kind.
  const auto sketch_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) {
        return feature.kind == "sketch" &&
               feature.sketch_parameters.has_value();
      });
  if (!expect(sketch_it != document.feature_history.end(),
              "convert no-op: sketch feature must exist")) {
    return false;
  }
  const auto& sketch = sketch_it->sketch_parameters.value();
  if (!expect(sketch.lines.size() == 4,
              "convert no-op: projections of the converted body must be "
              "no-ops (4 lines from the mesh projection)")) {
    return false;
  }
  if (!expect(sketch.projections.size() == 1 &&
                  sketch.projections.front().source_id ==
                      "body:" + body_id + ":section",
              "convert no-op: only the mesh projection record exists")) {
    return false;
  }
  if (!expect(sketch.vertices.size() == 4,
              "convert no-op: the rectangle keeps its 4 corner vertices")) {
    return false;
  }
  bool all_fixed = true;
  for (const auto& vertex : sketch.vertices) {
    all_fixed = all_fixed && vertex.is_fixed;
  }
  return expect(all_fixed && !sketch.profiles.empty(),
                "convert no-op: vertices stay fixed and the loop stays "
                "a profile");
}

// Shared assertions for the no-op sequence variants: the sketch holds
// exactly `expected_lines` fixed lines and the single projection record
// `expected_source` — every later projection of the same footprint was
// a no-op.
bool expect_noop_sketch(const DocumentState& document, size_t expected_lines,
                        const std::string& expected_source,
                        const char* what) {
  const auto sketch_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) {
        return feature.kind == "sketch" &&
               feature.sketch_parameters.has_value();
      });
  if (!expect(sketch_it != document.feature_history.end(), what)) {
    return false;
  }
  const auto& sketch = sketch_it->sketch_parameters.value();
  if (!expect(sketch.lines.size() == expected_lines, what)) {
    std::cerr << "  got " << sketch.lines.size() << " lines\n";
    for (const auto& line : sketch.lines) {
      std::cerr << "    (" << line.start_x << "," << line.start_y << ")-("
                << line.end_x << "," << line.end_y << ")\n";
    }
    return false;
  }
  if (!expect(sketch.projections.size() == 1 &&
                  sketch.projections.front().source_id == expected_source,
              what)) {
    return false;
  }
  for (const auto& line : sketch.lines) {
    const auto fixed = [&](const std::string& vertex_id) {
      return std::any_of(sketch.vertices.begin(), sketch.vertices.end(),
                         [&](const auto& v) {
                           return v.id == vertex_id && v.is_fixed;
                         });
    };
    if (!expect(fixed(line.start_vertex_id) && fixed(line.end_vertex_id),
                what)) {
      return false;
    }
  }
  return true;
}

bool test_converted_body_no_op_l_shape() {
  // Same convert->no-op sequence on the L-shape fixture (6-corner
  // outline, more segments through the duplicate check).
  const std::string path = write_l_shape_stl("convert_noop_l");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  const size_t lines_after_mesh =
      document.feature_history.back().sketch_parameters->lines.size();
  std::cerr << "  [l-noop] after mesh section: " << lines_after_mesh
            << " lines\n";
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  document = manager.project_body_into_sketch(converted_id, "section");
  {
    const auto it = std::find_if(
        document.feature_history.begin(), document.feature_history.end(),
        [](const auto& f) { return f.kind == "sketch"; });
    std::cerr << "  [l-noop] after converted section: "
              << it->sketch_parameters->lines.size() << " lines\n";
  }
  document = manager.project_body_into_sketch(converted_id, "silhouette");
  {
    const auto it = std::find_if(
        document.feature_history.begin(), document.feature_history.end(),
        [](const auto& f) { return f.kind == "sketch"; });
    std::cerr << "  [l-noop] after converted silhouette: "
              << it->sketch_parameters->lines.size() << " lines\n";
  }

  return expect_noop_sketch(document, lines_after_mesh,
                            "body:" + body_id + ":section",
                            "L no-op: converted-body projections must be "
                            "no-ops");
}

bool test_converted_body_no_op_coplanar() {
  // Box sitting FLAT on the sketch plane: both the mesh projection and
  // the converted-body projection go through the coplanar-boundary
  // fallback before hitting the duplicate guard.
  const std::string path = write_box_stl("convert_noop_coplanar", 0.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  document = manager.project_body_into_sketch(converted_id, "section");

  return expect_noop_sketch(document, 4, "body:" + body_id + ":section",
                            "coplanar no-op: converted-body section must "
                            "be a no-op");
}

bool test_converted_body_no_op_silhouette_first() {
  // Silhouette projected first; the converted body then no-ops in BOTH
  // modes (its silhouette and section share the footprint).
  const std::string path = write_box_stl("convert_noop_sil", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "silhouette");
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  document = manager.project_body_into_sketch(converted_id, "silhouette");
  document = manager.project_body_into_sketch(converted_id, "section");

  return expect_noop_sketch(document, 4, "body:" + body_id + ":silhouette",
                            "silhouette-first no-op: converted-body "
                            "projections must be no-ops");
}

bool test_convert_before_sketch_no_op() {
  // Order variant: convert FIRST, then sketch, project the converted
  // body (real insertion), then the mesh body (no-op this time).
  const std::string path = write_box_stl("convert_noop_order", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(converted_id, "section");
  document = manager.project_body_into_sketch(body_id, "section");

  return expect_noop_sketch(document, 4, "body:" + converted_id + ":section",
                            "convert-first no-op: mesh projection after "
                            "the converted one must be a no-op");
}

// DIAGNOSTIC PROBE (not in main()): the L-shape flat-on-plane sequence
// where the converted-body section is only a PARTIAL duplicate (outline
// + leaked triangulation diagonals). Drives the corrupted sketch through
// profile detection and extrude to surface the downstream corruption.
bool probe_l_shape_partial_duplicate_extrude() {
  const std::string path = write_l_shape_stl("probe_l_extrude");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;
  document = manager.project_body_into_sketch(converted_id, "section");

  const auto sketch_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) {
        return feature.kind == "sketch" &&
               feature.sketch_parameters.has_value();
      });
  if (sketch_it == document.feature_history.end()) {
    std::cerr << "probe: sketch missing\n";
    return false;
  }
  const auto& sketch = sketch_it->sketch_parameters.value();
  std::cerr << "probe: lines=" << sketch.lines.size()
            << " vertices=" << sketch.vertices.size()
            << " profiles=" << sketch.profiles.size() << "\n";
  if (sketch.profiles.empty()) {
    std::cerr << "probe: no profiles detected\n";
    return false;
  }
  for (const auto& profile : sketch.profiles) {
    std::cerr << "probe: profile " << profile.id << " kind=" << profile.kind
              << " lines=";
    for (const auto& line_id : profile.line_ids) {
      std::cerr << line_id << " ";
    }
    std::cerr << "\n";
  }

  document = manager.extrude_profile(sketch.profiles.front().id,
                                     /*depth=*/5.0, /*mode=*/"",
                                     /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled = compile_bodies(document);
  std::cerr << "probe: extrude compiled bodies=" << compiled.bodies.size()
            << "\n";
  return true;
}

// DIAGNOSTIC PROBE (not in main()): dump the converted solid's face
// edge structure (wire vs internal edges) and the z=0 section result,
// once with the shape as-is and once with triangulations stripped
// (BRepTools::Clean) — isolates whether the leaked coplanar segments
// come from face-internal edges or from triangulation-driven section.
bool probe_section_internals(const std::string& fixture) {
  const std::string path = fixture == "box"
                               ? write_box_stl("probe_internals_box", 0.0)
                               : write_l_shape_stl("probe_internals_l");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  const auto compiled = compile_bodies(document);
  const auto body_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == converted_id; });
  if (body_it == compiled.bodies.end()) {
    std::cerr << "probe: converted body missing\n";
    return false;
  }

  int face_index = 0;
  for (TopExp_Explorer face_exp(body_it->shape, TopAbs_FACE);
       face_exp.More(); face_exp.Next()) {
    const TopoDS_Face& face = TopoDS::Face(face_exp.Current());
    // Wire edges (boundary) vs all explored edges -> internal count.
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> wire_edges;
    for (TopExp_Explorer wire_exp(face, TopAbs_WIRE); wire_exp.More();
         wire_exp.Next()) {
      for (TopExp_Explorer edge_exp(wire_exp.Current(), TopAbs_EDGE);
           edge_exp.More(); edge_exp.Next()) {
        wire_edges.Add(edge_exp.Current());
      }
    }
    int all_edges = 0;
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More();
         edge_exp.Next()) {
      ++all_edges;
    }
    // Coplanarity with z=0.
    bool coplanar = true;
    for (TopExp_Explorer vexp(face, TopAbs_VERTEX); vexp.More();
         vexp.Next()) {
      if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vexp.Current())).Z()) >
          1e-6) {
        coplanar = false;
        break;
      }
    }
    std::cerr << "face " << face_index++
              << ": edges=" << all_edges
              << " wire_edges=" << wire_edges.Extent()
              << " internal_edges=" << (all_edges - wire_edges.Extent())
              << (coplanar ? " COPLANAR(z=0)" : "") << "\n";
  }

  const polysmith::core::PlaneFrame xy{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
  const auto raw = polysmith::core::compute_mesh_section_polylines(
      body_it->shape, xy);
  std::cerr << "section raw: " << raw.size() << " polylines\n";
  for (const auto& polyline : raw) {
    std::cerr << "  pts=" << polyline.size() << ":";
    for (const auto& point : polyline) {
      std::cerr << " (" << point.X() << "," << point.Y() << ")";
    }
    std::cerr << "\n";
  }

  TopoDS_Shape cleaned = body_it->shape;
  BRepTools::Clean(cleaned);  // strip triangulations
  const auto cleaned_polylines =
      polysmith::core::compute_mesh_section_polylines(cleaned, xy);
  std::cerr << "section cleaned: " << cleaned_polylines.size()
            << " polylines\n";
  for (const auto& polyline : cleaned_polylines) {
    std::cerr << "  pts=" << polyline.size() << ":";
    for (const auto& point : polyline) {
      std::cerr << " (" << point.X() << "," << point.Y() << ")";
    }
    std::cerr << "\n";
  }
  return true;
}

// DIAGNOSTIC PROBE (not in main()): replicate the silhouette edge
// collection on the converted solid and dump every edge that passes
// the silhouette criterion (with its adjacent-triangle normal data),
// to expose why triangulation edges leak on converted bodies.
bool probe_silhouette_dump(const std::string& fixture) {
  const std::string path = fixture == "box"
                               ? write_box_stl("probe_sil_box", 0.0)
                               : write_l_shape_stl("probe_sil_l");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  const auto compiled = compile_bodies(document);
  const auto body_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == converted_id; });
  if (body_it == compiled.bodies.end()) {
    return false;
  }

  struct Rec {
    gp_Pnt a, b;
    std::vector<gp_Vec> normals;
  };
  std::map<std::array<std::int64_t, 6>, Rec> edges;
  const auto key_of = [](const gp_Pnt& p, const gp_Pnt& q) {
    std::array<std::int64_t, 6> k{
        (std::int64_t)std::llround(p.X() * 1e4),
        (std::int64_t)std::llround(p.Y() * 1e4),
        (std::int64_t)std::llround(p.Z() * 1e4),
        (std::int64_t)std::llround(q.X() * 1e4),
        (std::int64_t)std::llround(q.Y() * 1e4),
        (std::int64_t)std::llround(q.Z() * 1e4)};
    std::array<std::int64_t, 6> r{q.X() > p.X() ? k[3] : k[0], 0, 0, 0, 0, 0};
    (void)r;
    if (k[3] < k[0] || (k[3] == k[0] && k[4] < k[1]) ||
        (k[3] == k[0] && k[4] == k[1] && k[5] < k[2])) {
      std::swap(k[0], k[3]);
      std::swap(k[1], k[4]);
      std::swap(k[2], k[5]);
    }
    return k;
  };

  int face_index = 0;
  for (TopExp_Explorer face_exp(body_it->shape, TopAbs_FACE);
       face_exp.More(); face_exp.Next(), ++face_index) {
    const TopoDS_Face& face = TopoDS::Face(face_exp.Current());
    TopLoc_Location location;
    const auto triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      std::cerr << "face " << face_index << ": NO triangulation\n";
      continue;
    }
    const gp_Trsf transform = location.Transformation();
    std::cerr << "face " << face_index
              << ": triangles=" << triangulation->NbTriangles() << "\n";
    for (int t = 1; t <= triangulation->NbTriangles(); ++t) {
      int n1, n2, n3;
      triangulation->Triangle(t).Get(n1, n2, n3);
      gp_Pnt p0 = triangulation->Node(n1).Transformed(transform);
      gp_Pnt p1 = triangulation->Node(n2).Transformed(transform);
      gp_Pnt p2 = triangulation->Node(n3).Transformed(transform);
      const gp_Vec normal = gp_Vec(p0, p1).Crossed(gp_Vec(p0, p2));
      if (normal.SquareMagnitude() <= 1e-30) continue;
      const gp_Vec unit = normal / std::sqrt(normal.SquareMagnitude());
      const std::array<std::pair<gp_Pnt, gp_Pnt>, 3> sides{{
          {p0, p1}, {p1, p2}, {p2, p0}}};
      for (const auto& [a, b] : sides) {
        auto& rec = edges[key_of(a, b)];
        if (rec.normals.empty()) { rec.a = a; rec.b = b; }
        rec.normals.push_back(unit);
      }
    }
  }

  const gp_Vec view(0, 0, 1);
  int leaked = 0;
  for (const auto& [key, rec] : edges) {
    (void)key;
    bool silhouette = false;
    const char* reason = "";
    if (rec.normals.size() < 2) {
      silhouette = true;
      reason = "boundary(open)";
    } else {
      const double d0 = rec.normals[0].Dot(view);
      const double d1 = rec.normals[1].Dot(view);
      const bool both_front = d0 > 1e-12 && d1 > 1e-12;
      const bool both_back = d0 < -1e-12 && d1 < -1e-12;
      if (!both_front && !both_back) {
        if (std::abs(d0) <= 1e-12 && std::abs(d1) <= 1e-12) {
          silhouette = rec.normals[0].Crossed(rec.normals[1])
                           .SquareMagnitude() > 1e-24;
          reason = "edge-on distinct";
        } else {
          silhouette = true;
          reason = "side flip";
        }
      }
    }
    if (!silhouette) continue;
    ++leaked;
    std::cerr << "sil edge (" << rec.a.X() << "," << rec.a.Y() << ","
              << rec.a.Z() << ")-(" << rec.b.X() << "," << rec.b.Y() << ","
              << rec.b.Z() << ") normals=" << rec.normals.size()
              << " reason=" << reason;
    for (const auto& n : rec.normals) {
      std::cerr << " [" << n.X() << "," << n.Y() << "," << n.Z() << "]";
    }
    std::cerr << "\n";
  }
  std::cerr << "total silhouette edges: " << leaked << "\n";
  return true;
}

bool test_converted_face_outline_merges_collinear() {
  const std::string path = write_l_shape_stl("l_shape_outline");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 1.0);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  const std::string converted_id = document.feature_history.back().id;

  const auto compiled = compile_bodies(document);
  const auto converted_it = std::find_if(
      compiled.bodies.begin(), compiled.bodies.end(),
      [&](const auto& body) { return body.id == converted_id; });
  if (!expect(converted_it != compiled.bodies.end(),
              "outline merge: converted body must exist")) {
    return false;
  }

  // Find the bottom face (all vertices at z=0) and get its 0-based
  // face index in the body's face map.
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(converted_it->shape, TopAbs_FACE, face_map);
  int bottom_face_index = -1;
  for (int i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(face_map(i));
    bool all_at_z0 = true;
    for (TopExp_Explorer vexp(face, TopAbs_VERTEX); vexp.More();
         vexp.Next()) {
      if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vexp.Current())).Z()) >
          1e-6) {
        all_at_z0 = false;
        break;
      }
    }
    if (all_at_z0) {
      bottom_face_index = i - 1;
      break;
    }
  }
  if (!expect(bottom_face_index >= 0,
              "outline merge: bottom face must exist")) {
    return false;
  }

  const auto outline = polysmith::core::compute_face_outline(
      document, converted_id + ":face:" +
                    std::to_string(bottom_face_index));
  if (!expect(outline.has_value() && outline->kind == "polygon",
              "outline merge: bottom face must produce a polygon "
              "outline")) {
    return false;
  }
  // The L boundary has 6 corners; the triangle edges split the 6
  // straight runs into several collinear segments each — the merge
  // must collapse them back to 6.
  return expect(outline->polygon_corners.size() == 6,
                "outline merge: collinear runs must merge to the 6 "
                "L-shape corners");
}

bool test_serialization_round_trip() {
  const std::string path = write_box_stl("serialize", -5.0);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path, 2.5);
  const std::string body_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(body_id);
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_body_into_sketch(body_id, "section");

  const auto file_path = std::filesystem::temp_directory_path() /
                         "polysmith_mesh_roundtrip_test.polysmith";
  manager.save_document_to_path(file_path.string());

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded =
      loaded_manager.load_document_from_path(file_path.string());

  const auto import_it = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feature) { return feature.kind == "mesh_import"; });
  if (!expect(import_it != loaded.feature_history.end() &&
                  import_it->mesh_import_parameters.has_value() &&
                  import_it->mesh_import_parameters->file_path == path &&
                  std::abs(import_it->mesh_import_parameters->scale - 2.5) <
                      1e-12,
              "roundtrip: mesh_import path + scale must survive")) {
    return false;
  }

  const auto convert_it = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feature) { return feature.kind == "mesh_to_body"; });
  if (!expect(convert_it != loaded.feature_history.end() &&
                  convert_it->mesh_to_body_parameters.has_value() &&
                  convert_it->mesh_to_body_parameters->source_body_id ==
                      body_id &&
                  !convert_it->mesh_to_body_parameters->serialized_shape
                       .empty(),
              "roundtrip: mesh_to_body source + snapshot must survive")) {
    return false;
  }

  const auto sketch_it = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feature) {
        return feature.kind == "sketch" &&
               feature.sketch_parameters.has_value();
      });
  if (!expect(
          sketch_it != loaded.feature_history.end() &&
              !sketch_it->sketch_parameters->projections.empty() &&
              sketch_it->sketch_parameters->projections.front().source_kind ==
                  "body" &&
              !sketch_it->sketch_parameters->projections.front()
                   .generated_line_ids.empty(),
          "roundtrip: body projection record must survive")) {
    return false;
  }

  // Event payloads strip the opaque B-rep snapshot (the UI never reads
  // it); saved files keep it.
  const auto find_mesh_to_body = [](const nlohmann::json& payload) {
    for (const auto& feature : payload["feature_history"]) {
      if (feature["kind"] == "mesh_to_body") {
        return feature;
      }
    }
    return nlohmann::json();
  };
  const nlohmann::json ui_payload = polysmith::protocol::to_payload(document);
  const nlohmann::json full_payload =
      polysmith::protocol::to_payload(document, /*include_opaque=*/true);
  const nlohmann::json ui_feature = find_mesh_to_body(ui_payload);
  const nlohmann::json full_feature = find_mesh_to_body(full_payload);
  return expect(
      !ui_feature.is_null() &&
          ui_feature["mesh_to_body_parameters"]["serialized_shape"]
                  .get<std::string>()
                  .empty() &&
          !full_feature.is_null() &&
          !full_feature["mesh_to_body_parameters"]["serialized_shape"]
               .get<std::string>()
               .empty(),
      "roundtrip: event payloads must strip the snapshot; saved "
      "payloads must keep it");
}

}  // namespace

int main(int argc, char** argv) {
  // A single test name as argv[1] runs just that test — used to compare
  // the crash repro in-suite vs standalone (heap-layout sensitivity).
  const std::string only = argc > 1 ? argv[1] : "";
  if (!only.empty()) {
    if (only == "convert_noop") {
      return test_converted_body_projection_no_op() ? 0 : 1;
    }
    if (only == "noop_l") return test_converted_body_no_op_l_shape() ? 0 : 1;
    if (only == "noop_coplanar") {
      return test_converted_body_no_op_coplanar() ? 0 : 1;
    }
    if (only == "noop_sil") {
      return test_converted_body_no_op_silhouette_first() ? 0 : 1;
    }
    if (only == "noop_order") {
      return test_convert_before_sketch_no_op() ? 0 : 1;
    }
    if (only == "probe_l_extrude") {
      return probe_l_shape_partial_duplicate_extrude() ? 0 : 1;
    }
    if (only == "internals_box") return probe_section_internals("box") ? 0 : 1;
    if (only == "internals_l") return probe_section_internals("l") ? 0 : 1;
    if (only == "sil_box") return probe_silhouette_dump("box") ? 0 : 1;
    if (only == "sil_l") return probe_silhouette_dump("l") ? 0 : 1;
    std::cerr << "unknown test: " << only << "\n";
    return 2;
  }

  std::cerr << "[stl_import_test] test 1: import compiles body\n";
  if (!test_import_compiles_body()) return 1;
  std::cerr << "[stl_import_test] test 2: section on ref plane\n";
  if (!test_section_projection_ref_plane()) return 1;
  std::cerr << "[stl_import_test] test 3: coplanar fallback\n";
  if (!test_section_coplanar_fallback()) return 1;
  std::cerr << "[stl_import_test] test 4: silhouette\n";
  if (!test_silhouette_outline()) return 1;
  std::cerr << "[stl_import_test] test 5: both modes coexist\n";
  if (!test_both_modes_coexist()) return 1;
  std::cerr << "[stl_import_test] test 6: convert to solid\n";
  if (!test_convert_creates_solid_alongside()) return 1;
  std::cerr << "[stl_import_test] test 7: missing file degrades\n";
  if (!test_missing_file_dependency_broken()) return 1;
  std::cerr << "[stl_import_test] test 8: serialization round-trip\n";
  if (!test_serialization_round_trip()) return 1;
  std::cerr << "[stl_import_test] test 9: converted face outline merges\n";
  if (!test_converted_face_outline_merges_collinear()) return 1;
  std::cerr << "[stl_import_test] test 10: douglas-peucker\n";
  if (!test_douglas_peucker_simplification()) return 1;
  std::cerr << "[stl_import_test] test 11: detach keeps sketch on delete\n";
  if (!test_detach_projection_keeps_sketch_on_delete()) return 1;
  std::cerr << "[stl_import_test] test 12: detach removes face projections\n";
  if (!test_detach_removes_face_projections()) return 1;
  std::cerr << "[stl_import_test] test 13: delete freezes construction plane\n";
  if (!test_deleting_body_freezes_construction_plane()) return 1;
  std::cerr << "[stl_import_test] test 14: converted-body projection no-op\n";
  if (!test_converted_body_projection_no_op()) return 1;
  std::cerr << "[stl_import_test] test 15: no-op on L-shape fixture\n";
  if (!test_converted_body_no_op_l_shape()) return 1;
  std::cerr << "[stl_import_test] test 16: no-op coplanar fallback\n";
  if (!test_converted_body_no_op_coplanar()) return 1;
  std::cerr << "[stl_import_test] test 17: no-op silhouette-first\n";
  if (!test_converted_body_no_op_silhouette_first()) return 1;
  std::cerr << "[stl_import_test] test 18: no-op convert-before-sketch\n";
  if (!test_convert_before_sketch_no_op()) return 1;
  std::cerr << "[stl_import_test] test 19: converted solid orientation\n";
  if (!test_converted_solid_orientation_normalized()) return 1;

  std::cout << "stl_import_test passed\n";
  return 0;
}
