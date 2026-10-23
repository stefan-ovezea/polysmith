// Regression tests for S30b - profile references must degrade, not vanish -
// plus the general rule that made the old behaviour look acceptable.
//
// Reported defect (wiki/Sketch-Defect-Log.md D-001a): deleting one circle from a
// sketch took the WHOLE extrude with it. The stored profile id of the deleted
// circle stopped resolving, `refresh_linked_extrude_features` marked the feature
// `dependency_broken` and skipped the rebuild, so the plate disappeared instead
// of losing a hole.
//
// Two things were wrong, and the second is the general one:
//   1. the refresh treated "one profile gone" as "everything gone";
//   2. `compile_bodies` (and the legacy viewport primitive emitter) skipped any
//      feature with `status == "warning"`, so a feature that had rebuilt itself
//      successfully but wanted to say something lost its body anyway.
// "warning" is advisory; `dependency_broken` is the flag that means "there is
// nothing to build". All three tests below pin that split.
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

constexpr double kDepth = 20.0;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

FeatureEntry* find_feature_mutable(DocumentState& document, const std::string& kind) {
  for (auto& feature : document.feature_history) {
    if (feature.kind == kind) return &feature;
  }
  return nullptr;
}

const FeatureEntry* find_feature_of_kind(const DocumentState& document,
                                         const std::string& kind) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == kind) return &feature;
  }
  return nullptr;
}

double body_volume(const DocumentState& document) {
  double total = 0.0;
  for (const auto& body : polysmith::core::compile_bodies(document).bodies) {
    if (body.shape.IsNull()) continue;
    GProp_GProps properties;
    BRepGProp::VolumeProperties(body.shape, properties);
    total += properties.Mass();
  }
  return total;
}

std::vector<std::string> profile_ids_of(const DocumentState& document) {
  std::vector<std::string> ids;
  for (const auto& feature : document.feature_history) {
    if (!feature.sketch_parameters.has_value()) continue;
    for (const auto& profile : feature.sketch_parameters->profiles) {
      ids.push_back(profile.id);
    }
  }
  return ids;
}

std::string sketch_id_of(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return feature.id;
  }
  return {};
}

// Rectangle 0,0 -> 80,60 with one circle inside: plate-with-a-hole plus the
// disc, i.e. two profiles.
DocumentState build_holed_plate(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 80.0, 60.0);
  manager.add_sketch_circle(40.0, 30.0, 10.0);
  DocumentState document = manager.finish_sketch();

  const std::vector<std::string> profiles = profile_ids_of(document);
  expect(profiles.size() == 2,
         "fixture: a rectangle with one circle yields 2 profiles");
  return document = manager.extrude_profiles(profiles, kDepth, "new_body");
}

// S30b: deleting the hole circle keeps the plate.
bool test_deleting_a_hole_circle_keeps_the_plate() {
  DocumentManager manager;
  DocumentState document = build_holed_plate(manager);
  const std::string sketch_id = sketch_id_of(document);
  if (!expect(!sketch_id.empty(), "fixture: sketch id resolved")) return false;

  const FeatureEntry* extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "before: extrude carries parameters")) {
    return false;
  }
  if (!expect(extrude->extrude_parameters->profile_ids.size() == 2,
              "before: extrude consumes both profiles")) {
    return false;
  }
  const double volume_before = body_volume(document);
  if (!expect(volume_before > 0.0, "before: the holed plate has volume")) {
    return false;
  }

  document = manager.reenter_sketch(sketch_id);
  document = manager.delete_sketch_selection({"circle-1"}, {}, {});
  document = manager.finish_sketch();

  extrude = find_feature_of_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "after delete: the extrude still exists")) {
    return false;
  }
  if (!expect(!extrude->dependency_broken,
              "after delete: NOT dependency_broken (regression: the plate vanished)")) {
    return false;
  }
  if (!expect(extrude->status == "warning",
              "after delete: flagged as a warning so the change is visible")) {
    return false;
  }
  if (!expect(extrude->dependency_warning.find("no longer exist") !=
                  std::string::npos,
              ("after delete: the warning explains the change - got: " +
               extrude->dependency_warning)
                  .c_str())) {
    return false;
  }
  if (!expect(extrude->extrude_parameters->profile_ids.size() == 1,
              "after delete: the extrude now consumes the surviving profile")) {
    return false;
  }

  const double volume_after = body_volume(document);
  if (!expect(volume_after > 0.0,
              "after delete: the body still has volume (regression: a warning "
              "status suppressed the build)")) {
    return false;
  }
  // The disc filled the hole exactly, so the two configurations have the SAME
  // volume; what changed is the shape, not the amount. The holed plate is
  // 80x60x20 minus the disc, the survivors-only plate is the full slab.
  const double slab_volume = 80.0 * 60.0 * kDepth;
  if (!expect(std::abs(volume_after - slab_volume) < 1.0,
              ("after delete: the body is the full slab (hole gone) - got " +
               std::to_string(volume_after) + " expected " +
               std::to_string(slab_volume))
                  .c_str())) {
    return false;
  }

  document = manager.undo();
  const double volume_restored = body_volume(document);
  return expect(std::abs(volume_restored - volume_before) < 1e-6,
                "after undo: the holed plate is back");
}

// The general rule: "warning" alone must not suppress geometry.
bool test_warning_status_alone_still_builds_the_body() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  DocumentState document = manager.finish_sketch();
  document = manager.extrude_profiles(profile_ids_of(document), kDepth,
                                      "new_body");

  FeatureEntry* extrude = find_feature_mutable(document, "extrude");
  if (!expect(extrude != nullptr, "general: extrude present")) return false;

  extrude->status = "warning";
  extrude->dependency_broken = false;
  extrude->dependency_warning = "advisory only";

  const auto compiled = polysmith::core::compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "general: a warning status does NOT suppress the body")) {
    return false;
  }
  double volume = 0.0;
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) continue;
    GProp_GProps properties;
    BRepGProp::VolumeProperties(body.shape, properties);
    volume += properties.Mass();
  }
  return expect(volume > 0.0, "general: the warned body has volume");
}

// And a broken dependency still suppresses it.
bool test_broken_dependency_suppresses_the_body() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  DocumentState document = manager.finish_sketch();
  document = manager.extrude_profiles(profile_ids_of(document), kDepth,
                                      "new_body");

  FeatureEntry* extrude = find_feature_mutable(document, "extrude");
  if (!expect(extrude != nullptr, "broken: extrude present")) return false;
  extrude->status = "warning";
  extrude->dependency_broken = true;

  return expect(polysmith::core::compile_bodies(document).bodies.empty(),
                "broken: a broken dependency still suppresses the body");
}

}  // namespace

int main() {
  if (!test_deleting_a_hole_circle_keeps_the_plate()) return 1;
  if (!test_warning_status_alone_still_builds_the_body()) return 1;
  if (!test_broken_dependency_suppresses_the_body()) return 1;

  std::cout << "extrude_profile_degrade_test passed\n";
  return 0;
}