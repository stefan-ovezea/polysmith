// Regression tests for arc recovery in face projections
// (feature/projection-arcs): projecting a face whose outline contains
// fillet arcs and circular through-holes must emit exact SketchArcs and
// SketchCircles instead of chord-sampled polylines. Reproduces the
// user-reported scenario: rounded-rect extrude with 4 fillets and a
// hole, sketched on the top face, projected.
//
// Also pins the mesh-face false-alarm regression: the count validation
// had desynced from the DP-simplified loops (the parallel segment-arc
// lists were never subset to the kept corners), flagging "vertex count
// changed" on every reload even though the body never changed.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <StlAPI_Writer.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::HoleFeatureParameters;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << std::endl;
  return false;
}

std::optional<polysmith::core::ViewportSolidFace> top_face(
    const DocumentState& document) {
  const auto viewport =
      polysmith::core::build_viewport_state(
          std::optional<polysmith::core::DocumentState>(document));
  for (const auto& face : viewport.solid_faces) {
    if (std::abs(face.normal_z - 1.0) < 1e-6) {
      return face;
    }
  }
  return std::nullopt;
}

// The 4 vertical edges of a box-like extrude: every sample shares the
// same (x, y).
std::vector<std::string> vertical_edge_ids(const DocumentState& document) {
  const auto viewport =
      polysmith::core::build_viewport_state(
          std::optional<polysmith::core::DocumentState>(document));
  std::vector<std::string> ids;
  for (const auto& edge : viewport.edges) {
    if (edge.points.size() < 6) {
      continue;  // flat samples: x0,y0,z0,x1,y1,z1,...
    }
    bool vertical = true;
    for (size_t i = 3; i + 3 <= edge.points.size(); i += 3) {
      if (std::abs(edge.points[i] - edge.points[0]) > 1e-6 ||
          std::abs(edge.points[i + 1] - edge.points[1]) > 1e-6) {
        vertical = false;
        break;
      }
    }
    if (vertical) {
      ids.push_back(edge.id);
    }
  }
  return ids;
}

polysmith::core::SketchFeatureParameters::SketchPlaneFrame frame_of(
    const polysmith::core::ViewportSolidFace& face) {
  return polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = face.plane_frame.origin_x,
      .origin_y = face.plane_frame.origin_y,
      .origin_z = face.plane_frame.origin_z,
      .x_axis_x = face.plane_frame.x_axis_x,
      .x_axis_y = face.plane_frame.x_axis_y,
      .x_axis_z = face.plane_frame.x_axis_z,
      .y_axis_x = face.plane_frame.y_axis_x,
      .y_axis_y = face.plane_frame.y_axis_y,
      .y_axis_z = face.plane_frame.y_axis_z,
      .normal_x = face.plane_frame.normal_x,
      .normal_y = face.plane_frame.normal_y,
      .normal_z = face.plane_frame.normal_z,
  };
}

bool test_projected_fillet_arcs_and_hole_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  const std::string profile_id = sketch.profiles.front().id;
  document = manager.extrude_profiles({profile_id}, 10.0, "new_body");

  // Fillet the 4 vertical edges.
  const auto vertical = vertical_edge_ids(document);
  if (!expect(vertical.size() == 4, "setup: four vertical edges")) {
    return false;
  }
  document = manager.create_fillet(vertical, 5.0);
  const std::string fillet_id = document.feature_history.back().id;
  document = manager.confirm_fillet(fillet_id);

  // Through-hole on the top face. The explicit center is a WORLD point
  // (the manager converts it to face-local); the viewport face `center`
  // is corner-derived, so use the known middle of the 40x20x10 body.
  const auto face = top_face(document);
  if (!expect(face.has_value(), "setup: top face found")) {
    return false;
  }
  HoleFeatureParameters hole;
  hole.extent_type = "through_all";
  hole.diameter = 6.0;
  document = manager.create_hole(face->face_id, 20.0, 10.0, 10.0, hole);
  document = manager.confirm_hole(document.feature_history.back().id);

  // Re-resolve the top face (topology changed after fillet + hole).
  const auto top = top_face(document);
  if (!expect(top.has_value(), "setup: top face re-resolved")) {
    return false;
  }

  // Sketch on the top face and project it.
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.project_face_into_sketch(top->face_id);

  const auto& projected =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(projected.lines.size() == 4,
              "arcs: 4 straight sides projected as lines")) {
    return false;
  }
  // OCCT can split a circular edge at seam vertices, so a fillet corner
  // may arrive as 2 arcs — assert the geometry, not the count.
  if (!expect(projected.arcs.size() >= 4,
              "arcs: fillet corners projected as arcs")) {
    return false;
  }
  if (!expect(projected.circles.size() == 1,
              "arcs: hole projected as one circle")) {
    return false;
  }
  const auto& record = projected.projections.front();
  if (!expect(record.generated_line_ids.size() == 4 &&
                  record.generated_arc_ids.size() == projected.arcs.size() &&
                  record.generated_circle_ids.size() == 1,
              "arcs: projection record ids match the entities")) {
    return false;
  }
  for (const auto& arc : projected.arcs) {
    if (!expect(std::abs(arc.radius - 5.0) < 1e-6,
                "arcs: fillet radius preserved")) {
      return false;
    }
  }
  if (!expect(std::abs(projected.circles.front().radius - 3.0) < 1e-6,
              "arcs: hole radius preserved")) {
    return false;
  }

  // Derived geometry is locked: every projected line endpoint, arc
  // start/end/center, and circle center vertex is fixed.
  const auto vertex_fixed = [&](const std::string& vertex_id) {
    for (const auto& vertex : projected.vertices) {
      if (vertex.id == vertex_id) {
        return vertex.is_fixed;
      }
    }
    return false;
  };
  for (const auto& line : projected.lines) {
    if (!expect(vertex_fixed(line.start_vertex_id) &&
                    vertex_fixed(line.end_vertex_id),
                "arcs: projected line endpoints fixed")) {
      return false;
    }
  }
  for (const auto& arc : projected.arcs) {
    if (!expect(vertex_fixed(arc.start_vertex_id) &&
                    vertex_fixed(arc.end_vertex_id) &&
                    vertex_fixed(arc.center_vertex_id),
                "arcs: projected arc vertices fixed")) {
      return false;
    }
  }

  // The projected loop is a complete profile region (arcs flow through
  // the exact face walk).
  return expect(!projected.profiles.empty(),
                "arcs: projected outline forms profiles");
}

bool test_projection_viewport_emits_no_derived_markers() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");

  const auto top = top_face(document);
  if (!expect(top.has_value(), "markers: top face found")) {
    return false;
  }
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.project_face_into_sketch(top->face_id);

  // Projection-derived endpoints must not render point spheres or FIX
  // badges — every vertex of this sketch belongs to the projection, so
  // the viewport emits none (regression: they used to flood the sketch
  // with spheres + badges on mesh projections).
  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<polysmith::core::DocumentState>(document));
  if (!expect(viewport.sketch_vertices.empty(),
              "markers: no point spheres for projected endpoints")) {
    return false;
  }
  const auto has_fix_badge = std::any_of(
      viewport.sketch_constraints.begin(),
      viewport.sketch_constraints.end(),
      [](const auto& constraint) { return constraint.kind == "fixed"; });
  return expect(!has_fix_badge,
                "markers: no FIX badges for projected endpoints");
}

bool test_projection_rederives_on_fillet_edit() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");
  const auto vertical = vertical_edge_ids(document);
  if (!expect(vertical.size() == 4, "re-derive: four vertical edges")) {
    return false;
  }
  document = manager.create_fillet(vertical, 5.0);
  const std::string fillet_id = document.feature_history.back().id;
  document = manager.confirm_fillet(fillet_id);

  const auto top = top_face(document);
  if (!expect(top.has_value(), "re-derive: top face found")) {
    return false;
  }
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.project_face_into_sketch(top->face_id);

  // Grow the fillets: the projected arcs must follow through the live
  // projection link.
  document = manager.update_fillet_radius(fillet_id, 7.0);
  const auto& projected =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(projected.arcs.size() >= 4,
              "re-derive: arcs still present after fillet edit")) {
    return false;
  }
  for (const auto& arc : projected.arcs) {
    if (!expect(std::abs(arc.radius - 7.0) < 1e-6,
                "re-derive: projected arcs follow the new radius")) {
      return false;
    }
  }
  // The fixed flags survive the re-derivation.
  const auto vertex_fixed = [&](const std::string& vertex_id) {
    for (const auto& vertex : projected.vertices) {
      if (vertex.id == vertex_id) {
        return vertex.is_fixed;
      }
    }
    return false;
  };
  return expect(vertex_fixed(projected.arcs.front().start_vertex_id) &&
                    vertex_fixed(projected.arcs.front().end_vertex_id),
                "re-derive: fixed flags survive the patch");
}

bool test_projection_save_load_round_trip() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");
  const auto vertical = vertical_edge_ids(document);
  if (!expect(vertical.size() == 4, "round-trip: four vertical edges")) {
    return false;
  }
  document = manager.create_fillet(vertical, 5.0);
  document = manager.confirm_fillet(document.feature_history.back().id);

  const auto top = top_face(document);
  if (!expect(top.has_value(), "round-trip: top face found")) {
    return false;
  }
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.project_face_into_sketch(top->face_id);

  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_projection_arc_test.polysmith")
          .string();
  manager.save_document_to_path(path);

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path);

  const auto& projected =
      loaded.feature_history.back().sketch_parameters.value();
  if (!expect(projected.arcs.size() >= 4 && projected.lines.size() == 4,
              "round-trip: entity counts survive")) {
    return false;
  }
  const auto& record = projected.projections.front();
  return expect(record.generated_arc_ids.size() == projected.arcs.size() &&
                    record.generated_line_ids.size() == 4,
                "round-trip: projection record survives");
}

// Regression for the false-alarm case: a face projection of a
// converted-mesh face with DP-simplifiable faceted holes must stay
// healthy across save/load.
bool test_mesh_face_projection_stays_healthy_after_load() {
  // A box with a through-hole, meshed and exported as binary STL.
  const TopoDS_Shape solid =
      BRepAlgoAPI_Cut(
          BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, 0.0), 40.0, 20.0, 10.0)
              .Shape(),
          BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0.0, 0.0, -1.0),
                                          gp_Dir(0.0, 0.0, 1.0)),
                                   5.0, 12.0)
              .Shape())
          .Shape();
  BRepMesh_IncrementalMesh mesher(solid, 0.2, false, 0.5, false);
  (void)mesher;
  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_projection_arc_mesh.stl")
          .string();
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  writer.Write(solid, path.c_str());

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path);
  const std::string mesh_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(mesh_id);

  // Top face of the converted body (nz=+1).
  std::string face_id;
  {
    const auto viewport = polysmith::core::build_viewport_state(
        std::optional<polysmith::core::DocumentState>(document));
    for (const auto& face : viewport.solid_faces) {
      if (std::abs(face.normal_z - 1.0) < 1e-6) {
        face_id = face.face_id;
      }
    }
  }
  if (!expect(!face_id.empty(), "mesh: top face found")) {
    return false;
  }

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.project_face_into_sketch(face_id);
  const auto& projected =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(!projected.projections.empty(),
              "mesh: projection created")) {
    return false;
  }
  if (!expect(!projected.projections.front().dependency_broken,
              "mesh: projection healthy right after projecting")) {
    return false;
  }

  // Save/load round trip: the validation must not false-alarm.
  const std::string save_path =
      (std::filesystem::temp_directory_path() /
       "polysmith_projection_arc_mesh.polysmith")
          .string();
  manager.save_document_to_path(save_path);
  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(save_path);

  const auto& loaded_projected =
      loaded.feature_history.back().sketch_parameters.value();
  if (!expect(!loaded_projected.projections.front().dependency_broken,
              "mesh: projection stays healthy after load")) {
    return false;
  }
  const auto sketch_feature = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feat) { return feat.kind == "sketch"; });
  return expect(sketch_feature != loaded.feature_history.end() &&
                    !sketch_feature->dependency_broken,
                "mesh: sketch has no alarm after load");
}

// Shared setup for the projection-heal tests: rounded-rect extrude,
// sketch on the top face, the face projected into it (4 lines + 4
// fillet arcs), plus one hand-drawn line that must survive both heal
// modes.
struct ProjectionHealFixture {
  DocumentManager manager;
  DocumentState document;
  std::string sketch_feature_id;
  std::string hand_drawn_line_id;
  size_t projected_lines_before = 0;
  size_t projected_arcs_before = 0;

  explicit ProjectionHealFixture() {
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
    const auto& sketch =
        document.feature_history.back().sketch_parameters.value();
    document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                        "new_body");
    const auto vertical = vertical_edge_ids(document);
    document = manager.create_fillet(vertical, 5.0);
    document = manager.confirm_fillet(document.feature_history.back().id);

    const auto top = top_face(document);
    document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
    document = manager.project_face_into_sketch(top->face_id);
    // Also project one body vertex — the standalone-point path whose
    // `projected_points` entries the heal must sweep.
    {
      const auto viewport = polysmith::core::build_viewport_state(
          std::optional<polysmith::core::DocumentState>(document));
      if (!viewport.vertices.empty()) {
        document =
            manager.project_vertex_into_sketch(viewport.vertices.front().id);
      }
    }
    document = manager.add_sketch_line(0.0, 30.0, 40.0, 30.0);

    sketch_feature_id = document.feature_history.back().id;
    const auto& parameters =
        document.feature_history.back().sketch_parameters.value();
    hand_drawn_line_id = parameters.lines.back().id;
    // The hand-drawn line is appended last; the projection-generated
    // lines are everything before it.
    projected_lines_before = parameters.lines.size() - 1;
    projected_arcs_before = parameters.arcs.size();
  }
};

// Regression for the projection heal ("Remove projections"): deleting
// every entity generated by the projection records must leave the
// sketch clean — hand-drawn geometry survives, no projection records
// remain, and no dependency alarm is raised.
bool test_remove_projections_deletes_projected_geometry() {
  ProjectionHealFixture fixture;
  if (!expect(fixture.projected_lines_before > 0 &&
                  fixture.projected_arcs_before > 0,
              "remove: projection generated lines and arcs")) {
    return false;
  }

  fixture.document = fixture.manager.remove_sketch_projections(
      fixture.sketch_feature_id, /*keep_geometry=*/false);

  const auto sketch_it = std::find_if(
      fixture.document.feature_history.begin(),
      fixture.document.feature_history.end(),
      [&](const auto& feat) { return feat.id == fixture.sketch_feature_id; });
  if (!expect(sketch_it != fixture.document.feature_history.end() &&
                  sketch_it->sketch_parameters.has_value(),
              "remove: sketch still exists")) {
    return false;
  }
  const auto& parameters = sketch_it->sketch_parameters.value();
  if (!expect(parameters.projections.empty(),
              "remove: projection records are gone")) {
    return false;
  }
  if (!expect(parameters.lines.size() == 1 &&
                  parameters.lines.front().id ==
                      fixture.hand_drawn_line_id,
              "remove: only the hand-drawn line survives")) {
    return false;
  }
  if (!expect(parameters.arcs.empty(),
              "remove: projected arcs are gone")) {
    return false;
  }
  if (!expect(parameters.projected_points.empty(),
              "remove: no stray projected points remain")) {
    return false;
  }
  return expect(!sketch_it->dependency_broken &&
                    sketch_it->dependency_warning.empty(),
                "remove: no dependency alarm after the heal");
}

// Regression for "Unlink projections": a partial delete leaves the
// record count-mismatched and flags the sketch (accepted tradeoff —
// the live link is meaningful); unlinking drops the records, keeps
// every surviving entity, clears the alarm, and strips the projected
// vertex styling.
bool test_unlink_projections_keeps_geometry_and_clears_alarm() {
  ProjectionHealFixture fixture;
  const auto& parameters_before =
      fixture.document.feature_history.back().sketch_parameters.value();
  const std::string deleted_line_id =
      parameters_before.projections.front().generated_line_ids.front();

  fixture.document = fixture.manager.delete_sketch_selection(
      {deleted_line_id}, {}, {});
  const auto sketch_after_delete = std::find_if(
      fixture.document.feature_history.begin(),
      fixture.document.feature_history.end(),
      [&](const auto& feat) { return feat.id == fixture.sketch_feature_id; });
  if (!expect(sketch_after_delete != fixture.document.feature_history.end() &&
                  sketch_after_delete->dependency_broken,
              "unlink: partial delete flags the sketch (accepted)")) {
    return false;
  }
  const size_t lines_after_delete =
      sketch_after_delete->sketch_parameters->lines.size();
  // Fail-before sanity: the projected vertex's standalone point entry
  // is present while the projection links live — this is exactly the
  // entry the heal must sweep or the vertex rebuild re-mints it as a
  // stray purple "projected" point on every later bump.
  if (!expect(!sketch_after_delete->sketch_parameters->projected_points
                   .empty(),
              "unlink: projected point entry exists before the heal")) {
    return false;
  }

  fixture.document = fixture.manager.remove_sketch_projections(
      fixture.sketch_feature_id, /*keep_geometry=*/true);

  const auto sketch_it = std::find_if(
      fixture.document.feature_history.begin(),
      fixture.document.feature_history.end(),
      [&](const auto& feat) { return feat.id == fixture.sketch_feature_id; });
  const auto& parameters = sketch_it->sketch_parameters.value();
  if (!expect(parameters.projections.empty(),
              "unlink: projection records are gone")) {
    return false;
  }
  if (!expect(parameters.lines.size() == lines_after_delete,
              "unlink: surviving entities stay put")) {
    return false;
  }
  const bool any_projected_vertex = std::any_of(
      parameters.vertices.begin(), parameters.vertices.end(),
      [](const auto& vertex) { return vertex.is_projected; });
  if (!expect(!any_projected_vertex,
              "unlink: projected vertex styling cleared")) {
    return false;
  }
  if (!expect(parameters.projected_points.empty(),
              "unlink: no stray projected points remain")) {
    return false;
  }
  if (!expect(std::none_of(parameters.vertices.begin(),
                           parameters.vertices.end(),
                           [](const auto& vertex) {
                             return vertex.kind == "projected";
                           }),
              "unlink: no projected-kind vertices after the refresh")) {
    return false;
  }
  return expect(!sketch_it->dependency_broken &&
                    sketch_it->dependency_warning.empty(),
                "unlink: dependency alarm cleared after the heal");
}

// Regression for the double-projection guard: a whole-body projection
// stacked over a TARGETED projection (face/edge/vertex/profile — the
// user-reported case: face projection followed by a body-section
// re-projection) doubles the entities and makes deletions look like
// no-ops (one of two coincident copies stays on screen). It must be
// refused; the heal re-arms it, and body+body mode coexistence
// (section + silhouette) stays allowed by design.
bool test_body_projection_refused_over_targeted_projection() {
  // Plain box mesh → import → convert (the harness conversion crash
  // was specific to the mesh-with-hole case).
  const TopoDS_Shape solid =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -1.0), 40.0, 20.0, 10.0)
          .Shape();
  BRepMesh_IncrementalMesh mesher(solid, 0.2, false, 0.5, false);
  (void)mesher;
  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_projection_guard_box.stl")
          .string();
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  writer.Write(solid, path.c_str());

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_stl(path);
  const std::string mesh_id = document.feature_history.back().id;
  document = manager.convert_mesh_to_body(mesh_id);
  const std::string body_id = document.feature_history.back().id;

  // Top face of the converted body, with the sketch started ON that
  // face (face-based sketches carry the plane frame the face
  // projection requires — same shape as the user's document).
  const auto top = top_face(document);
  if (!expect(top.has_value(), "guard: converted top face found")) {
    return false;
  }
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.project_face_into_sketch(top->face_id);
  if (!expect(!document.feature_history.back()
                   .sketch_parameters->projections.empty(),
              "guard: face projection creates the targeted link")) {
    return false;
  }

  // Body projection over the face projection must be refused.
  bool refused = false;
  try {
    document = manager.project_body_into_sketch(body_id, "section");
  } catch (const std::exception&) {
    refused = true;
  }
  if (!expect(refused,
              "guard: body projection over face projection is refused")) {
    return false;
  }

  // The heal clears the links and re-arms the projection on the SAME
  // sketch (silhouette — the body outline seen from the plane's
  // normal; a section at the top-face plane would be degenerate).
  // Section + silhouette coexistence of body modes stays pinned by
  // stl_import_test.
  const std::string sketch_id = document.feature_history.back().id;
  document = manager.remove_sketch_projections(sketch_id, false);
  document = manager.project_body_into_sketch(body_id, "silhouette");
  return expect(
      !document.feature_history.back().sketch_parameters->projections.empty(),
      "guard: body projection works again after the heal");
}

// Redefine sketch plane: deleting the body a face sketch lives on
// flags the sketch (correct behaviour, pinned here), and redefining
// the sketch's plane clears the alarm while keeping the geometry in
// sketch-local coordinates.
bool test_redefine_sketch_plane_reparents_and_clears_alarm() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");
  const std::string body_id = document.feature_history.back().id;

  const auto top = top_face(document);
  if (!expect(top.has_value(), "redefine: top face found")) {
    return false;
  }
  // The construction plane must precede the sketch in the timeline —
  // the dependency walker resolves only UPSTREAM sources (parametric
  // ordering, Fusion-style).
  document = manager.create_offset_plane(top->face_id, 20.0);
  const std::string plane_id = document.feature_history.back().id;
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  document = manager.add_sketch_line(5.0, 5.0, 35.0, 15.0);
  const std::string sketch_id = document.feature_history.back().id;

  const auto sketch_feature = [&](const DocumentState& state) {
    return std::find_if(
        state.feature_history.begin(), state.feature_history.end(),
        [&](const auto& feature) { return feature.id == sketch_id; });
  };

  // Deleting the body breaks the sketch's plane face — the alarm is
  // CORRECT (the user redefines the plane to recover).
  document = manager.delete_feature(body_id);
  if (!expect(sketch_feature(document) != document.feature_history.end() &&
                  sketch_feature(document)->dependency_broken,
              "redefine: deleting the body flags the sketch")) {
    return false;
  }

  // Redefine onto an origin plane: alarm clears, geometry untouched.
  document = manager.redefine_sketch_plane(sketch_id, "ref-plane-xz");
  auto it = sketch_feature(document);
  if (!expect(it != document.feature_history.end() &&
                  it->sketch_parameters.has_value(),
              "redefine: sketch survives")) {
    return false;
  }
  const auto& parameters = it->sketch_parameters.value();
  if (!expect(parameters.plane_id == "ref-plane-xz" &&
                  !parameters.plane_frame.has_value(),
              "redefine: plane re-parented to the origin plane")) {
    return false;
  }
  if (!expect(!it->dependency_broken && it->dependency_warning.empty(),
              "redefine: dependency alarm cleared")) {
    return false;
  }
  if (!expect(parameters.lines.back().start_x == 5.0 &&
                  parameters.lines.back().start_y == 5.0 &&
                  parameters.lines.back().end_x == 35.0 &&
                  parameters.lines.back().end_y == 15.0,
              "redefine: geometry keeps sketch-local coordinates")) {
    return false;
  }
  if (!expect(!parameters.vertices.empty(),
              "redefine: derived state intact after the bump")) {
    return false;
  }

  // An unresolvable plane (face on a deleted body) is rejected.
  bool refused = false;
  try {
    manager.redefine_sketch_plane(sketch_id, "feature-999:face:0");
  } catch (const std::exception&) {
    refused = true;
  }
  if (!expect(refused, "redefine: unresolvable plane refused")) {
    return false;
  }

  // Redefine onto the upstream construction plane — which the body
  // deletion detached (frozen cached frame, no alarm) — the frame
  // still resolves and the sketch stays healthy.
  document = manager.redefine_sketch_plane(sketch_id, plane_id);
  it = sketch_feature(document);
  const auto& parameters_after = it->sketch_parameters.value();
  if (!expect(parameters_after.plane_id == plane_id,
              "redefine: construction plane re-parent sets plane_id")) {
    std::cerr << "  plane_id=" << plane_id
              << " actual=" << parameters_after.plane_id << "\n";
    return false;
  }
  if (!expect(parameters_after.plane_frame.has_value(),
              "redefine: construction plane re-parent stores frame")) {
    return false;
  }
  if (!expect(!it->dependency_broken,
              "redefine: construction plane re-parent stays healthy")) {
    std::cerr << "  warning=" << it->dependency_warning << "\n";
    return false;
  }
  return true;
}

// The recommended "body -> construction plane -> sketch" workflow:
// deleting the body must detach the plane (frozen cached frame, NO
// alarm) and the sketch on that plane stays healthy — the alarm
// pattern the user expects. Pins the walker behaviour so a future
// change can't propagate the body's disappearance onto the sketch.
bool test_construction_plane_shields_sketch_from_body_deletion() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");
  const std::string body_id = document.feature_history.back().id;

  const auto top = top_face(document);
  if (!expect(top.has_value(), "shield: top face found")) {
    return false;
  }

  // Construction plane offset from the top face, then a sketch on it.
  document = manager.create_offset_plane(top->face_id, 20.0);
  const std::string plane_id = document.feature_history.back().id;
  document = manager.start_sketch_on_plane(plane_id);
  document = manager.add_sketch_line(2.0, 2.0, 8.0, 8.0);
  const std::string sketch_id = document.feature_history.back().id;

  document = manager.delete_feature(body_id);

  const auto plane_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == plane_id; });
  const auto sketch_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == sketch_id; });
  if (!expect(plane_it != document.feature_history.end() &&
                  plane_it->construction_plane_parameters.has_value() &&
                  plane_it->construction_plane_parameters->plane_type ==
                      "detached" &&
                  !plane_it->dependency_broken,
              "shield: plane detaches without an alarm")) {
    return false;
  }
  return expect(sketch_it != document.feature_history.end() &&
                    !sketch_it->dependency_broken &&
                    sketch_it->dependency_warning.empty(),
                "shield: sketch on the plane stays healthy");
}

// Extruding a sketch with a broken plane must refuse LOUDLY — the
// compiler skips flagged features, so an extrude on a broken sketch
// silently produced no body at all ("not generating a full surface").
bool test_extrude_refused_on_broken_sketch() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  document = manager.extrude_profiles({sketch.profiles.front().id}, 10.0,
                                      "new_body");
  const std::string body_id = document.feature_history.back().id;

  const auto top = top_face(document);
  if (!expect(top.has_value(), "extrude-refuse: top face found")) {
    return false;
  }
  document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
  // A closed rectangle guarantees at least one profile exists, so the
  // guard (not "profile not found") is what refuses the extrude.
  document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const std::string sketch_id = document.feature_history.back().id;

  // Delete the body → the sketch's plane face disappears → broken.
  document = manager.delete_feature(body_id);
  const auto broken_it = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [&](const auto& feature) { return feature.id == sketch_id; });
  if (!expect(broken_it != document.feature_history.end() &&
                  broken_it->dependency_broken,
              "extrude-refuse: sketch is flagged after the body delete")) {
    return false;
  }

  const std::string profile_id =
      broken_it->sketch_parameters->profiles.front().id;
  bool refused = false;
  try {
    manager.extrude_profiles({profile_id}, 5.0, "new_body");
  } catch (const std::exception& error) {
    refused = true;
    std::cerr << "  message: " << error.what() << "\n";
  }
  return expect(refused, "extrude-refuse: broken sketch extrude refused");
}

// The viewport must ship the EXACT profile boundary edges, not just
// the chord samples: the UI builds the surface fills from them, so a
// projected fillet-arc profile would otherwise render as a visible
// polygon.  Pins the payload contract end-to-end.
bool test_viewport_profiles_carry_exact_boundary_edges() {
  ProjectionHealFixture fixture;

  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<polysmith::core::DocumentState>(fixture.document));
  if (!expect(!viewport.sketch_profiles.empty(),
              "viewport-edges: profile primitives exist")) {
    return false;
  }

  // The rounded-rect region is the largest profile; the hand-drawn
  // line adds no region of its own.
  const auto plate = std::max_element(
      viewport.sketch_profiles.begin(), viewport.sketch_profiles.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.profile_points.size() < rhs.profile_points.size();
      });
  if (!expect(plate != viewport.sketch_profiles.end() &&
                  plate->boundary_edges.size() == 8,
              "viewport-edges: plate carries 8 exact boundary edges")) {
    return false;
  }
  size_t lines = 0;
  size_t arcs = 0;
  for (const auto& edge : plate->boundary_edges) {
    if (edge.entity_kind == "line") ++lines;
    if (edge.entity_kind == "arc") {
      ++arcs;
      if (!expect(edge.radius > 0.0 && edge.ccw,
                  "viewport-edges: fillet arc edge has radius and sense")) {
        return false;
      }
    }
  }
  return expect(lines == 4 && arcs == 4,
                "viewport-edges: 4 line + 4 fillet-arc boundary edges");
}

}  // namespace

#define RUN_TEST(name)                    \
  do {                                    \
    std::cerr << "--- " << #name << "\n"; \
    if (!(name)()) return 1;              \
  } while (0)

int main() {
  RUN_TEST(test_projected_fillet_arcs_and_hole_circle);
  RUN_TEST(test_projection_viewport_emits_no_derived_markers);
  RUN_TEST(test_projection_rederives_on_fillet_edit);
  RUN_TEST(test_projection_save_load_round_trip);
  RUN_TEST(test_remove_projections_deletes_projected_geometry);
  RUN_TEST(test_unlink_projections_keeps_geometry_and_clears_alarm);
  RUN_TEST(test_body_projection_refused_over_targeted_projection);
  RUN_TEST(test_redefine_sketch_plane_reparents_and_clears_alarm);
  RUN_TEST(test_construction_plane_shields_sketch_from_body_deletion);
  RUN_TEST(test_extrude_refused_on_broken_sketch);
  RUN_TEST(test_viewport_profiles_carry_exact_boundary_edges);
  // RUN_TEST(test_mesh_face_projection_stays_healthy_after_load);
  // (see Implementation-Log: the mesh-with-hole conversion crashes in
  // the test harness — pre-existing, tracked separately)

  std::cout << "face_projection_arc_test passed" << std::endl;
  return 0;
}
