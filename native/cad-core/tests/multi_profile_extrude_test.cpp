#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "core/body_compiler.h"
#include "core/document.h"
#include "core/feature.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

std::vector<std::string> profile_ids(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      std::vector<std::string> ids;
      for (const auto& profile : feature.sketch_parameters->profiles) {
        ids.push_back(profile.id);
      }
      std::sort(ids.begin(), ids.end());
      return ids;
    }
  }
  return {};
}

int extrude_feature_count(const DocumentState& document) {
  return static_cast<int>(
      std::count_if(document.feature_history.begin(),
                    document.feature_history.end(),
                    [](const FeatureEntry& feature) {
                      return feature.kind == "extrude";
                    }));
}

bool test_new_body_splits_disconnected_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles before extrude")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "new_body");
  const auto compiled = compile_bodies(document);
  return expect(extrude_feature_count(document) == 2,
                "expected two extrude features for disconnected new bodies") &&
         expect(compiled.bodies.size() == 2,
                "expected two compiled bodies for disconnected new bodies");
}

bool test_join_groups_touching_profiles_without_merging_distant_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  manager.add_sketch_rectangle(10.0, 0.0, 20.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(40.0, 0.0, 50.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 3,
              "expected three sketch profiles before joined extrude")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "join");
  const auto compiled = compile_bodies(document);
  return expect(extrude_feature_count(document) == 2,
                "expected touching profiles to share one join feature") &&
         expect(compiled.bodies.size() == 2,
                "expected joined touching profiles plus distant profile to make two bodies");
}

bool test_join_adjacent_profiles_creates_one_body_feature() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(10.0, 0.0, 20.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles before adjacent join")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "join");
  const auto compiled = compile_bodies(document);
  const auto extrude_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [](const FeatureEntry& feature) { return feature.kind == "extrude"; });
  return expect(extrude_feature_count(document) == 1,
                "expected adjacent joined profiles to share one feature") &&
         expect(compiled.bodies.size() == 1,
                "expected adjacent joined profiles to make one body") &&
         expect(extrude_it != document.feature_history.end() &&
                    extrude_it->extrude_parameters.has_value() &&
                    extrude_it->extrude_parameters->mode == "new_body" &&
                    extrude_it->extrude_parameters->operation == "join",
                "expected untargeted join to remain a visible new body");
}

}  // namespace

int main() {
  if (!test_new_body_splits_disconnected_profiles()) {
    return 1;
  }
  if (!test_join_groups_touching_profiles_without_merging_distant_profiles()) {
    return 1;
  }
  if (!test_join_adjacent_profiles_creates_one_body_feature()) {
    return 1;
  }

  std::cout << "multi_profile_extrude_test passed\n";
  return 0;
}
