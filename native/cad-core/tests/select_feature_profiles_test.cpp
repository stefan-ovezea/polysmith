// Regression test for D-001f: selecting a feature from the timeline must not
// empty a profile-consuming feature's input.
//
// The extrude edit panel's input IS the profile selection (D-001e), so
// `select_feature` clearing that selection made any timeline click empty the
// panel. Selecting such a feature now SELECTS the profiles it consumes; other
// feature kinds still clear them.
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

const FeatureEntry* find_kind(const DocumentState& document,
                              const std::string& kind) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == kind) return &feature;
  }
  return nullptr;
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

bool test_selecting_an_extrude_selects_its_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  DocumentState document = manager.finish_sketch();
  const std::vector<std::string> profiles = profile_ids_of(document);
  document = manager.extrude_profiles(profiles, 20.0, "new_body");

  const FeatureEntry* extrude = find_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "extrude: parameters present")) {
    return false;
  }
  const std::vector<std::string> consumed =
      extrude->extrude_parameters->profile_ids;
  const std::string extrude_id = extrude->id;
  const std::string sketch_id = sketch_id_of(document);

  // Selecting the SKETCH first proves the clearing behaviour is still there for
  // a feature that consumes no profiles.
  document = manager.select_feature(sketch_id);
  if (!expect(document.selected_sketch_profile_ids.empty(),
              "select sketch: the profile selection is cleared")) {
    return false;
  }

  // Selecting the extrude carries its own profiles into the selection.
  document = manager.select_feature(extrude_id);
  if (!expect(document.selected_sketch_profile_ids == consumed,
              "select extrude: the profile selection equals the consumed "
              "profiles (regression: it was cleared, emptying the panel)")) {
    return false;
  }
  return expect(document.selected_sketch_profile_id.has_value() &&
                    document.selected_sketch_profile_id.value() ==
                        consumed.front(),
                "select extrude: the primary profile id is set too");
}

bool test_selecting_a_non_profile_feature_clears_the_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 40.0, 30.0);
  DocumentState document = manager.finish_sketch();
  const std::vector<std::string> profiles = profile_ids_of(document);
  document = manager.extrude_profiles(profiles, 20.0, "new_body");
  const std::string extrude_id = find_kind(document, "extrude")->id;
  const std::string sketch_id = sketch_id_of(document);

  document = manager.select_feature(extrude_id);
  if (!expect(!document.selected_sketch_profile_ids.empty(),
              "non-profile: the extrude selection carries profiles")) {
    return false;
  }
  // The sketch consumes no profiles, so selecting it clears them again - the
  // pre-D-001f behaviour is intact for every other feature kind.
  document = manager.select_feature(sketch_id);
  return expect(document.selected_sketch_profile_ids.empty(),
                "non-profile: selecting the sketch clears the selection");
}
// A polygon profile id embeds its boundary vertex keys, so any edit that moves a
// vertex rewrites it. Selecting a region and THEN editing the geometry it is made
// of left the selection holding an id the next recompute had already replaced, and
// the following command failed with "Sketch profile not found" - on a plain click,
// with no tool armed. An id must resolve to the region that is still there.
bool test_a_stored_profile_id_survives_a_geometry_edit() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 50.0);
  // Two lines from the circle's boundary to a point outside bound a "wedge"
  // region that is closed by the circle; the rest of the disc is a second region.
  manager.add_sketch_line(0.0, 50.0, -80.0, 0.0);
  manager.add_sketch_line(-80.0, 0.0, 0.0, -50.0);
  DocumentState document = manager.finish_sketch();

  const auto wedge_of = [](const DocumentState& state) {
    std::string id;
    for (const auto& feature : state.feature_history) {
      if (!feature.sketch_parameters.has_value()) continue;
      for (const auto& profile : feature.sketch_parameters->profiles) {
        if (profile.line_ids.size() >= 2) id = profile.id;
      }
    }
    return id;
  };
  const auto circle_of = [](const DocumentState& state) {
    for (const auto& feature : state.feature_history) {
      if (!feature.sketch_parameters.has_value()) continue;
      if (!feature.sketch_parameters->circles.empty()) {
        return feature.sketch_parameters->circles.front().id;
      }
    }
    return std::string{};
  };

  const std::string stored_id = wedge_of(document);
  if (!expect(!stored_id.empty(), "stale id: the wedge region exists")) {
    return false;
  }

  // Resize the circle with the sketch RE-OPENED - that is when the user can
  // reshape a region whose id they have already selected. The wedge's boundary
  // moves, so its id is rewritten while the region count stays at two.
  const std::string sketch_id = sketch_id_of(document);
  document = manager.reenter_sketch(sketch_id);
  document = manager.update_sketch_circle(circle_of(document), 0.0, 0.0, 60.0);
  const std::string current_id = wedge_of(document);
  if (!expect(!current_id.empty() && current_id != stored_id,
              "stale id: the resize rewrites the region id")) {
    return false;
  }
  document = manager.finish_sketch();

  // Selecting with the stored id must land on the region that is still there.
  document = manager.select_sketch_profile(stored_id, false);
  if (!expect(document.selected_sketch_profile_ids.size() == 1,
              "stale id: the click selects one region")) {
    return false;
  }
  if (!expect(document.selected_sketch_profile_ids.front() == current_id,
              ("stale id: the selection holds the CURRENT id - got " +
               document.selected_sketch_profile_ids.front())
                  .c_str())) {
    return false;
  }

  // …and extruding with it must build the wedge, not fail.
  document = manager.extrude_profiles({stored_id}, 10.0, "new_body");
  const FeatureEntry* extrude = find_kind(document, "extrude");
  if (!expect(extrude != nullptr && extrude->extrude_parameters.has_value(),
              "stale id: the extrude is created")) {
    return false;
  }
  const auto& consumed = extrude->extrude_parameters->profile_ids;
  return expect(consumed.size() == 1 && consumed.front() == current_id,
                "stale id: the extrude records the current region id");
}

}  // namespace

int main() {
  // The commands under test THROW on a miss ("Sketch profile not found"), and an
  // uncaught throw aborts the process with no output at all. Catch it so a
  // regression reads as a failure with its message instead of a silent crash.
  try {
    if (!test_selecting_an_extrude_selects_its_profiles()) return 1;
    if (!test_selecting_a_non_profile_feature_clears_the_profiles()) return 1;
    if (!test_a_stored_profile_id_survives_a_geometry_edit()) return 1;
  } catch (const std::exception& error) {
    std::cerr << "threw: " << error.what() << "\n";
    return 1;
  }

  std::cout << "select_feature_profiles_test passed\n";
  return 0;
}