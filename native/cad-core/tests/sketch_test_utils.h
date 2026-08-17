#pragma once

// Shared helpers for sketch/profile regression tests.
//
// Profile-detection tests must assert the COMPLETE expected region set,
// not just the presence of one profile: the 2026-08 face-walk regression
// silently removed a full-circle profile while the suite stayed green
// because the trim test only asserted the outer polygon.  Use
// profiles_match() for any test that exercises region detection.

#include <set>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/sketch_profile_types.h"

namespace polysmith::test {

struct ExpectedProfile {
  // Exact set of entity ids the region is bounded by (line_ids).
  std::vector<std::string> entity_ids;
  // "polygon" or "circle" — SketchProfileRegion::kind.
  std::string kind;
  // When true the profile must carry a source_circle_id (full circles).
  bool has_source_circle_id = false;
};

// Checks that the document's sketch profile regions match `expected`
// exactly: same total count, and every expected entry (exact entity-id
// set + kind + source-circle flag) is present.  On mismatch `reason`
// describes the first discrepancy found.  Profiles are gathered across
// every sketch feature in the history.
inline bool profiles_match(const polysmith::core::DocumentState& document,
                           const std::vector<ExpectedProfile>& expected,
                           std::string* reason) {
  std::vector<const polysmith::core::SketchProfileRegion*> profiles;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    for (const auto& profile : feature.sketch_parameters->profiles) {
      profiles.push_back(&profile);
    }
  }

  if (profiles.size() != expected.size()) {
    *reason = "expected " + std::to_string(expected.size()) + " profiles, got " +
              std::to_string(profiles.size());
    return false;
  }

  std::vector<bool> matched(expected.size(), false);
  for (const auto* profile : profiles) {
    bool found = false;
    for (size_t index = 0; index < expected.size(); ++index) {
      if (matched[index]) continue;
      if (profile->kind != expected[index].kind) continue;
      std::set<std::string> ids(profile->line_ids.begin(),
                                profile->line_ids.end());
      std::set<std::string> want(expected[index].entity_ids.begin(),
                                 expected[index].entity_ids.end());
      if (ids != want) continue;
      if (profile->source_circle_id.has_value() !=
          expected[index].has_source_circle_id) {
        continue;
      }
      matched[index] = true;
      found = true;
      break;
    }
    if (!found) {
      std::string ids_str;
      for (const auto& id : profile->line_ids) {
        ids_str += id + " ";
      }
      *reason = "profile kind=" + profile->kind + " ids=[" + ids_str +
                "] src=" +
                (profile->source_circle_id ? *profile->source_circle_id
                                           : std::string("none")) +
                " matches no expected profile";
      return false;
    }
  }
  return true;
}

}  // namespace polysmith::test
