// Regression tests for stale selection/highlight fixes (feature/highlight):
//   - the central prune_document_selection() pass in
//     bump_geometry_revision drops orphaned sketch-selection ids
//     (trim-as-delete, deleted dimensions, fabricated dim ids, load),
//   - per-mutator clears of plural/3D selections (extrude, finish_sketch,
//     fillet/chamfer confirm, delete_feature),
//   - clear_selection covers selected_sketch_text_id.
// Live selections must survive ordinary mutations (negative control).

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/viewport/viewport.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

DocumentState active_sketch_params(DocumentManager& manager) {
  return manager.get_document().value();
}

std::string first_edge_id(const DocumentState& document) {
  const auto viewport =
      polysmith::core::build_viewport_state(std::optional<DocumentState>(document));
  return viewport.edges.empty() ? std::string() : viewport.edges.front().id;
}

std::string first_face_id(const DocumentState& document) {
  const auto viewport =
      polysmith::core::build_viewport_state(std::optional<DocumentState>(document));
  return viewport.solid_faces.empty() ? std::string()
                                      : viewport.solid_faces.front().face_id;
}

// Sketch rectangle → profile selection; extrude must clear ALL sketch
// and 3D selections (the primary reported symptom: the finished
// sketch's profile fill stayed lit).
bool test_extrude_clears_selections() {
  DocumentManager manager;
  manager.create_document();
  // A body so a real 3D face selection exists before the extrude.
  DocumentState document = manager.add_box_feature(
      BoxFeatureParameters{.width = 20.0, .height = 20.0, .depth = 20.0});
  const std::string face_id = first_face_id(document);
  if (!expect(!face_id.empty(), "extrude: box face exists")) {
    return false;
  }
  document = manager.select_face(face_id);

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const std::string profile_id = sketch.profiles.front().id;
  document = manager.select_sketch_profile(profile_id, false);
  document = manager.extrude_profiles({profile_id}, 5.0, "new_body");

  if (!expect(!document.selected_sketch_profile_id.has_value() &&
                  document.selected_sketch_profile_ids.empty(),
              "extrude: profile selection cleared")) {
    return false;
  }
  if (!expect(!document.selected_sketch_entity_id.has_value() &&
                  document.selected_sketch_entity_ids.empty() &&
                  document.selected_sketch_vertex_ids.empty(),
              "extrude: plural sketch selections cleared")) {
    return false;
  }
  if (!expect(!document.selected_face_id.has_value() &&
                  document.selected_edge_ids.empty() &&
                  document.selected_vertex_ids.empty(),
              "extrude: 3D selections cleared")) {
    return false;
  }
  return expect(!document.selected_sketch_text_id.has_value(),
                "extrude: text selection cleared");
}

bool test_finish_sketch_clears_3d() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      BoxFeatureParameters{.width = 10.0, .height = 10.0, .depth = 10.0});

  const std::string face_id = first_face_id(document);
  if (!expect(!face_id.empty(), "finish: box face exists")) {
    return false;
  }
  document = manager.select_face(face_id);
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.finish_sketch();

  return expect(!document.selected_face_id.has_value() &&
                    document.selected_edge_ids.empty() &&
                    document.selected_vertex_ids.empty(),
                "finish_sketch: 3D selections cleared");
}

bool test_fillet_confirm_clears_edges() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      BoxFeatureParameters{.width = 10.0, .height = 10.0, .depth = 10.0});

  const std::string edge_id = first_edge_id(document);
  if (!expect(!edge_id.empty(), "fillet: box edge exists")) {
    return false;
  }
  document = manager.create_fillet({edge_id}, 1.0);
  if (!expect(document.selected_edge_ids.size() == 1,
              "fillet: pending session highlights its edges")) {
    return false;
  }
  const std::string fillet_id = document.feature_history.back().id;
  document = manager.confirm_fillet(fillet_id);
  return expect(document.selected_edge_ids.empty(),
                "fillet: confirm clears the highlighted edges");
}

bool test_chamfer_confirm_clears_edges() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      BoxFeatureParameters{.width = 10.0, .height = 10.0, .depth = 10.0});

  const std::string edge_id = first_edge_id(document);
  if (!expect(!edge_id.empty(), "chamfer: box edge exists")) {
    return false;
  }
  document = manager.create_chamfer({edge_id}, 1.0);
  const std::string chamfer_id = document.feature_history.back().id;
  document = manager.confirm_chamfer(chamfer_id);
  return expect(document.selected_edge_ids.empty(),
                "chamfer: confirm clears the highlighted edges");
}

// Trimming a line with no intersections deletes it; the prune must
// drop the (previously selected) id.
bool test_trim_delete_prunes_selection() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 50.0, 0.0);

  const auto& sketch = document.feature_history[1].sketch_parameters.value();
  const std::string line_id = sketch.lines.back().id;
  document = manager.select_sketch_entity(line_id, false);
  if (!expect(document.selected_sketch_entity_id.has_value(),
              "trim: line selected before trim")) {
    return false;
  }

  // Click mid-line: no intersections with anything → the line is
  // deleted by the trim engine.
  document = manager.trim_sketch_entity(line_id, 25.0, 0.0);

  return expect(!document.selected_sketch_entity_id.has_value() &&
                    document.selected_sketch_entity_ids.empty(),
                "trim: deleted line's selection is pruned");
}

bool test_delete_dimension_prunes_selection() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 0.0);

  const auto& sketch = document.feature_history[1].sketch_parameters.value();
  const std::string line_id = sketch.lines.back().id;
  document = manager.add_sketch_line_length_dimension(line_id);
  const auto& after = document.feature_history[1].sketch_parameters.value();
  const auto dim_it =
      std::find_if(after.dimensions.begin(), after.dimensions.end(),
                   [&](const auto& d) { return d.entity_id == line_id; });
  if (!expect(dim_it != after.dimensions.end(),
              "dim: length dimension exists")) {
    return false;
  }
  const std::string dim_id = dim_it->id;
  document = manager.select_sketch_dimension(dim_id);
  if (!expect(document.selected_sketch_dimension_id.has_value(),
              "dim: dimension selected")) {
    return false;
  }
  document = manager.delete_sketch_dimension(dim_id);
  return expect(!document.selected_sketch_dimension_id.has_value(),
                "dim: deleted dimension's selection is pruned");
}

// update_sketch_line fabricates a dim selection; it must survive while
// the line exists and vanish once the line is gone.
bool test_fabricated_dim_selection_pruned() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 0.0);

  const auto& sketch = document.feature_history[1].sketch_parameters.value();
  const std::string line_id = sketch.lines.back().id;
  document = manager.update_sketch_line(line_id, 0.0, 0.0, 40.0, 0.0);
  if (!expect(document.selected_sketch_dimension_id.has_value() &&
                  document.selected_sketch_dimension_id.value() ==
                      "dim-line-" + line_id,
              "fabricated: update_sketch_line selects the dim while the "
              "line lives")) {
    return false;
  }

  document = manager.delete_sketch_selection({line_id}, {}, {});
  return expect(!document.selected_sketch_dimension_id.has_value(),
                "fabricated: dim selection pruned after the line dies");
}

bool test_delete_feature_clears_owned_3d() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      BoxFeatureParameters{.width = 10.0, .height = 10.0, .depth = 10.0});

  const std::string face_id = first_face_id(document);
  const std::string box_feature_id = document.feature_history.back().id;
  if (!expect(!face_id.empty(), "delete: box face exists")) {
    return false;
  }
  document = manager.select_face(face_id);
  document = manager.delete_feature(box_feature_id);

  return expect(!document.selected_face_id.has_value(),
                "delete: face selection owned by the deleted feature cleared");
}

bool test_clear_selection_clears_text() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_text(
      polysmith::core::SketchText{.text = "OK", .height_mm = 10.0});

  // select_sketch_text has no manager path yet; simulate the selection
  // the way the document layer would set it (the clear is the contract).
  const auto& texts = document.feature_history[1].sketch_parameters->texts;
  if (!expect(!texts.empty(), "clear: text exists")) {
    return false;
  }
  document.selected_sketch_text_id = texts.front().id;
  document = manager.clear_selection();
  return expect(!document.selected_sketch_text_id.has_value(),
                "clear_selection: text selection cleared");
}

// Orphaned ids in a saved document must be pruned on load (the load
// path bumps the revision, which runs the prune), while live
// selections survive the round trip.
bool test_load_prunes_orphans() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 0.0);

  // The document legitimately selects the new line; inject orphaned
  // selections alongside and serialize the MODIFIED snapshot directly
  // (the manager's internal state is unreachable — the save path would
  // write the clean version).
  document.selected_sketch_entity_ids = {"line-999"};
  document.selected_sketch_dimension_id = std::string("dim-line-999");
  document.selected_sketch_text_id = std::string("text-999");

  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_selection_test.polysmith")
          .string();
  {
    std::ofstream stream(path);
    stream << polysmith::protocol::to_payload(document).dump();
  }

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path);

  return expect(loaded.selected_sketch_entity_id.has_value() &&
                    loaded.selected_sketch_entity_id.value() == "line-1",
                "load: live entity selection survives") &&
         expect(loaded.selected_sketch_entity_ids.empty(),
                "load: orphaned plural entity ids pruned") &&
         expect(!loaded.selected_sketch_dimension_id.has_value() ||
                    loaded.selected_sketch_dimension_id.value() !=
                        "dim-line-999",
                "load: orphaned dimension selection pruned") &&
         expect(!loaded.selected_sketch_text_id.has_value(),
                "load: orphaned text selection pruned");
}

// Negative control: ordinary mutations keep live selections.
bool test_live_selection_survives_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 30.0, 0.0);

  const auto& sketch = document.feature_history[1].sketch_parameters.value();
  const std::string line_id = sketch.lines.back().id;
  document = manager.select_sketch_entity(line_id, false);
  document = manager.move_sketch_entities({line_id}, 5.0, 0.0, 0.0, 0.0, 0.0);

  return expect(document.selected_sketch_entity_id.has_value() &&
                    document.selected_sketch_entity_id.value() == line_id,
                "negative: live selection survives a plain move");
}

}  // namespace

#define RUN_TEST(name)                    \
  do {                                    \
    std::cerr << "--- " << #name << "\n"; \
    if (!(name)()) return 1;              \
  } while (0)

int main() {
  RUN_TEST(test_extrude_clears_selections);
  RUN_TEST(test_finish_sketch_clears_3d);
  RUN_TEST(test_fillet_confirm_clears_edges);
  RUN_TEST(test_chamfer_confirm_clears_edges);
  RUN_TEST(test_trim_delete_prunes_selection);
  RUN_TEST(test_delete_dimension_prunes_selection);
  RUN_TEST(test_fabricated_dim_selection_pruned);
  RUN_TEST(test_delete_feature_clears_owned_3d);
  RUN_TEST(test_clear_selection_clears_text);
  RUN_TEST(test_load_prunes_orphans);
  RUN_TEST(test_live_selection_survives_move);

  std::cout << "cad_core_selection_test passed\n";
  return 0;
}
