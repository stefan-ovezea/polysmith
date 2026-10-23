// Reference line flavours — centerline and infinite (parity: T1.5).
//
// Both are LINE FLAGS, not new entity kinds: the line is stored with its two
// clicked endpoints and `line_type` set, and it also carries the construction
// flag so every existing construction rule (profile exclusion, no automatic
// dimensions) applies without special-casing. The flavour only adds
// presentation (dash-dot / drawn past its endpoints) in the viewport.
//
// Profile-set assertions use profiles_match (complete region-set matching).

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

bool test_centerline_is_construction_with_a_flavour() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const DocumentState document = manager.add_sketch_line(
      0.0, 0.0, 40.0, 0.0, /*is_construction=*/false, "centerline");
  const auto params = sketch_params(document);
  if (!expect(params.lines.size() == 1, "centerline: one line created")) {
    return false;
  }
  const auto& line = params.lines[0];
  if (!expect(line.line_type == "centerline",
              "centerline: the flavour is stored on the line")) {
    return false;
  }
  if (!expect(line.is_construction,
              "centerline: forces the construction flag (profile + dimension "
              "rules follow it)")) {
    return false;
  }
  // Construction geometry must not seal a profile or get an auto dimension.
  if (!expect(params.profiles.empty(),
              "centerline: no profile is produced")) {
    return false;
  }
  return expect(params.dimensions.empty(),
                "centerline: no automatic length dimension");
}

bool test_infinite_line_keeps_its_stored_endpoints() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const DocumentState document = manager.add_sketch_line(
      1.0, 2.0, 11.0, 2.0, /*is_construction=*/false, "infinite");
  const auto params = sketch_params(document);
  const auto& line = params.lines[0];
  // The document stores the clicked endpoints; the extension is a
  // viewport-only concern, so scene bounds and camera framing stay sane.
  return expect(line.line_type == "infinite" && line.is_construction &&
                    std::abs(line.start_x - 1.0) < 1e-9 &&
                    std::abs(line.end_x - 11.0) < 1e-9,
                "infinite line: flavour stored, endpoints untouched, "
                "construction flag forced");
}

bool test_plain_line_is_unaffected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 30.0);
  const auto params = sketch_params(document);
  if (!expect(params.lines[0].line_type.empty(),
              "plain line: no flavour")) {
    return false;
  }
  if (!expect(!params.lines[0].is_construction,
              "plain line: still solid")) {
    return false;
  }
  // A standalone solid line is an open curve → no profile; the check is
  // that its flavour did not change any of that.
  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {};
  return expect(profiles_match(document, expected, &reason),
                ("plain open line profile set: " + reason).c_str());
}

bool test_centerline_does_not_seal_a_rectangle_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Rectangle plus a centerline through it: still exactly one profile.
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  document = manager.add_sketch_line(0.0, 10.0, 40.0, 10.0,
                                     /*is_construction=*/false, "centerline");
  const auto params = sketch_params(document);
  std::vector<std::string> rectangle_line_ids;
  for (const auto& line : params.lines) {
    if (line.line_type.empty()) {
      rectangle_line_ids.push_back(line.id);
    }
  }
  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {
      {.entity_ids = rectangle_line_ids, .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("centerline inside a rectangle profile set: " + reason).c_str());
}

}  // namespace

int main() {
  if (!test_centerline_is_construction_with_a_flavour()) return 1;
  if (!test_infinite_line_keeps_its_stored_endpoints()) return 1;
  if (!test_plain_line_is_unaffected()) return 1;
  if (!test_centerline_does_not_seal_a_rectangle_profile()) return 1;

  std::cout << "line_type_test passed\n";
  return 0;
}
