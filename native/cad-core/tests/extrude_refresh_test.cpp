// Regression test for the linked-extrude refresh.
//
// Reported defect (wiki/Sketch-Defect-Log.md D-001b): editing a sketch silently
// changed the extrude that consumed it — a 0.287 mm circle-radius edit halved
// the body (side1.distance reset from 20 to its 10 mm "unset" sentinel) and
// split it into three bodies (operation reset from join to new_body), and the
// resulting degenerate profile then blanked the window when the extrude panel
// was opened (D-001c).
//
// Cause: `refresh_linked_extrude_features` replaced the whole
// ExtrudeFeatureParameters struct with one carrying only the profile-derived
// fields, restoring `mode` and `target_body_id` and nothing else. It now copies
// the existing parameters and lets `preserve_extrude_source_geometry` overwrite
// only what the profile determines.
//
// The assertions are deliberately split: the parameter ones name the exact
// fields that were lost, and the geometry one proves the body keeps its depth
// (volume would otherwise drop by half).

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

constexpr double kDepth = 20.0;
constexpr double kRectangleWidth = 80.0;
constexpr double kRectangleHeight = 60.0;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

const FeatureEntry* find_feature_of_kind(const DocumentState& document,
                                         const std::string& kind) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == kind) {
      return &feature;
    }
  }
  return nullptr;
}

const FeatureEntry* find_sketch(const DocumentState& document) {
  const FeatureEntry* found = nullptr;
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) {
      found = &feature;
    }
  }
  return found;
}

std::vector<std::string> profile_ids_of(const DocumentState& document) {
  std::vector<std::string> ids;
  const FeatureEntry* sketch = find_sketch(document);
  if (sketch == nullptr) {
    return ids;
  }
  for (const auto& profile : sketch->sketch_parameters->profiles) {
    ids.push_back(profile.id);
  }
  return ids;
}

// Rectangle 0,0 → 80,60 with two circles inside it: the plate-with-holes region
// plus the two discs, i.e. three profiles whose union is a solid slab.
DocumentState build_three_profile_extrude(DocumentManager& manager,
                                          std::string* sketch_id,
                                          std::string* extrude_id) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, kRectangleWidth, kRectangleHeight);
  manager.add_sketch_circle(20.0, 30.0, 10.0);
  DocumentState document = manager.add_sketch_circle(55.0, 30.0, 10.0);
  document = manager.finish_sketch();

  const std::vector<std::string> profiles = profile_ids_of(document);
  expect(profiles.size() == 3,
         "fixture: a rectangle with two circles must yield 3 profiles");

  document = manager.extrude_profiles(profiles, kDepth, "join");

  const FeatureEntry* sketch = find_sketch(document);
  const FeatureEntry* extrude = find_feature_of_kind(document, "extrude");
  if (sketch_id != nullptr && sketch != nullptr) {
    *sketch_id = sketch->id;
  }
  if (extrude_id != nullptr && extrude != nullptr) {
    *extrude_id = extrude->id;
  }
  return document;
}

// The exact fields that the refresh used to reset.
bool test_extrude_parameters_survive_a_sketch_edit() {
  DocumentManager manager;
  std::string sketch_id;
  std::string extrude_id;
  DocumentState document =
      build_three_profile_extrude(manager, &sketch_id, &extrude_id);
  if (!expect(!sketch_id.empty() && !extrude_id.empty(),
              "fixture: sketch and extrude ids resolved")) {
    return false;
  }

  const FeatureEntry* extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "before: extrude carries parameters")) {
    return false;
  }
  const double side1_before = extrude->extrude_parameters->side1.distance;
  const std::string operation_before =
      extrude->extrude_parameters->operation;
  const std::string extent_mode_before =
      extrude->extrude_parameters->extent_mode;
  if (!expect(std::abs(side1_before - kDepth) < 1e-9,
              "before: side1.distance equals the requested depth")) {
    return false;
  }
  if (!expect(operation_before == "join",
              "before: operation is join")) {
    return false;
  }

  // The reported edit: one circle's radius changed by 0.287 mm.
  document = manager.reenter_sketch(sketch_id);
  document = manager.update_sketch_circle("circle-2", 55.0, 30.0, 9.0);

  extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "after edit: extrude still present")) {
    return false;
  }
  const auto& parameters = *extrude->extrude_parameters;
  if (!expect(std::abs(parameters.side1.distance - side1_before) < 1e-9,
              "after edit: side1.distance preserved (regression: reset to the "
              "10 mm sentinel, halving the body)")) {
    return false;
  }
  if (!expect(parameters.operation == operation_before,
              "after edit: operation preserved (regression: reset to new_body, "
              "splitting one body into three)")) {
    return false;
  }
  if (!expect(parameters.extent_mode == extent_mode_before,
              "after edit: extent_mode preserved")) {
    return false;
  }
  if (!expect(std::abs(parameters.depth - kDepth) < 1e-9,
              "after edit: depth preserved")) {
    return false;
  }
  return expect(!extrude->dependency_broken,
                "after edit: the sketch edit leaves the extrude healthy");
}

// The same defect seen in geometry: the body must keep its full depth.
bool test_body_keeps_its_depth_after_a_sketch_edit() {
  DocumentManager manager;
  std::string sketch_id;
  std::string extrude_id;
  DocumentState document =
      build_three_profile_extrude(manager, &sketch_id, &extrude_id);
  if (!expect(!sketch_id.empty(), "fixture: sketch id resolved")) {
    return false;
  }

  document = manager.reenter_sketch(sketch_id);
  document = manager.update_sketch_circle("circle-2", 55.0, 30.0, 9.0);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "after edit: exactly one body still compiles (regression: three "
              "bodies appeared)")) {
    return false;
  }

  Bnd_Box box;
  BRepBndLib::Add(compiled.bodies.front().shape, box);
  double xmin = 0.0, ymin = 0.0, zmin = 0.0, xmax = 0.0, ymax = 0.0, zmax = 0.0;
  box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  if (!expect(std::abs((zmax - zmin) - kDepth) < 0.05,
              "after edit: the body spans the full depth (regression: 10 mm "
              "instead of 20 mm — the visible 'thin wall')")) {
    return false;
  }

  GProp_GProps properties;
  BRepGProp::VolumeProperties(compiled.bodies.front().shape, properties);
  const double volume = std::abs(properties.Mass());
  // Three profiles whose union is the full slab, i.e. the holes are filled by
  // the discs: W · H · depth.
  const double expected = kRectangleWidth * kRectangleHeight * kDepth;
  if (!expect(std::abs(volume - expected) / expected < 0.01,
              "after edit: the body volume matches the full slab (regression: "
              "about half of it)")) {
    std::cerr << "  volume=" << volume << " expected≈" << expected << "\n";
    return false;
  }
  return true;
}

// The document payload ships extrude parameters without their derived profile
// geometry. The extrude panel therefore round-trips empty arrays when it
// restores its snapshot (cancel), which used to be rejected with
// "Polygon extrude requires at least three profile points" (2026-10-23).
bool test_update_tolerates_stripped_profile_geometry() {
  DocumentManager manager;
  std::string sketch_id;
  std::string extrude_id;
  DocumentState document =
      build_three_profile_extrude(manager, &sketch_id, &extrude_id);
  if (!expect(!extrude_id.empty(), "fixture: extrude id resolved")) {
    return false;
  }

  const FeatureEntry* extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "before: extrude carries parameters")) {
    return false;
  }

  // Exactly what the renderer-facing payload contains: everything that is not
  // derived survives, the profile geometry does not.
  polysmith::core::ExtrudeFeatureParameters stripped =
      *extrude->extrude_parameters;
  stripped.profile_points.clear();
  stripped.inner_loops.clear();
  stripped.boundary_edges.clear();
  stripped.inner_loop_edges.clear();
  stripped.sketch_edge_ids.clear();
  stripped.additional_profile_points.clear();
  stripped.additional_inner_loops.clear();
  stripped.additional_boundary_edges.clear();
  stripped.sketch_lines.clear();
  stripped.sketch_arcs.clear();
  stripped.sketch_circles.clear();
  stripped.arc_descriptors.clear();

  try {
    document = manager.update_extrude_parameters(extrude_id, stripped);
  } catch (const std::exception& error) {
    std::cerr << "update threw: " << error.what() << "\n";
    return expect(false,
                  "stripped-geometry update must re-derive, not throw");
  }

  extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "after update: extrude carries parameters")) {
    return false;
  }
  const auto& parameters = *extrude->extrude_parameters;
  if (!expect(parameters.profile_points.size() >= 3,
              "after update: profile geometry re-derived from the profile "
              "ids")) {
    return false;
  }
  if (!expect(std::abs(parameters.side1.distance - kDepth) < 1e-9,
              "after update: side1.distance still the requested depth")) {
    return false;
  }
  if (!expect(parameters.operation == "join",
              "after update: operation preserved")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1,
                "after update: the body still compiles");
}

// Re-binding profiles is the repair path for a feature that lost a source
// region. It must clear the stale failure state (otherwise the body stays
// suppressed and "confirm" looks inert) and must not reset the extent.
bool test_rebinding_profiles_heals_a_broken_extrude() {
  DocumentManager manager;
  std::string sketch_id;
  std::string extrude_id;
  DocumentState document =
      build_three_profile_extrude(manager, &sketch_id, &extrude_id);
  if (!expect(!sketch_id.empty() && !extrude_id.empty(),
              "fixture: sketch and extrude ids resolved")) {
    return false;
  }

  // Break it the way a user does: delete one of the two circles.
  document = manager.reenter_sketch(sketch_id);
  document = manager.delete_sketch_selection({"circle-1"}, {}, {});
  const FeatureEntry* broken = find_feature_of_kind(document, "extrude");
  if (!expect(broken != nullptr && broken->dependency_broken,
              "fixture: deleting a source circle flags the extrude broken")) {
    return false;
  }

  // Re-bind to the regions that survive: the plate (now one hole) and the
  // remaining circle.
  const std::vector<std::string> surviving = profile_ids_of(document);
  if (!expect(surviving.size() == 2,
              "fixture: two regions survive the deletion")) {
    return false;
  }
  document = manager.update_extrude_profiles(extrude_id, surviving);

  const FeatureEntry* healed = find_feature_of_kind(document, "extrude");
  if (!expect(healed != nullptr && healed->extrude_parameters.has_value(),
              "after re-bind: extrude carries parameters")) {
    return false;
  }
  if (!expect(!healed->dependency_broken,
              "after re-bind: the stale dependency_broken flag is cleared "
              "(regression: the body stays suppressed and Confirm looks "
              "inert)")) {
    return false;
  }
  if (!expect(std::abs(healed->extrude_parameters->side1.distance - kDepth) <
                  1e-9,
              "after re-bind: side1.distance preserved")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1,
                "after re-bind: a body compiles again");
}

}  // namespace

int main() {
  if (!test_extrude_parameters_survive_a_sketch_edit()) return 1;
  if (!test_body_keeps_its_depth_after_a_sketch_edit()) return 1;
  if (!test_update_tolerates_stripped_profile_geometry()) return 1;
  if (!test_rebinding_profiles_heals_a_broken_extrude()) return 1;
  std::cout << "extrude_refresh_test passed\n";
  return 0;
}
