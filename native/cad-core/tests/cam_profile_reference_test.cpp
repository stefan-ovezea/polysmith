// CAM sketch-profile reference (witness capture/resolve) test.
//
// Profile ids churn because refresh_sketch_profiles() recomputes the
// whole region list after sketch edits, so CAM operations store
// witness data (centroid, area, bbox, boundary-kind signature, hole
// count) and re-resolve against freshly built regions.  These tests
// cover the complete resolution matrix: Found on the same region,
// Found after a move (both displacement signs), NotFound on unrelated
// geometry, and Ambiguous for two identical regions — never a guess.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::CamProfileReference;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::ProfileResolutionOutcome;
using polysmith::core::capture_profile_reference;
using polysmith::core::resolve_profile_reference;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

const polysmith::core::SketchFeatureParameters& sketch_params(
    const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      return feature.sketch_parameters.value();
    }
  }
  throw std::runtime_error("no sketch feature in document");
}

std::string sketch_feature_id(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch") {
      return feature.id;
    }
  }
  return "";
}

// ── Test 1: capture witness data from a rectangle + hole ──────────
//
// A rectangle (0,0)-(20,10) with a circular hole: the outer region is
// a polygon with one inner loop; the hole is a circle-sourced region.

bool test_capture_rectangle_with_hole() {
  constexpr double kPi = 3.14159265358979323846;
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);

  const auto& params = sketch_params(document);
  if (!expect(params.profiles.size() == 2,
              "capture: rectangle + circle produce exactly two regions")) {
    return false;
  }

  // Both regions are kind "polygon" (the circle region is a sampled
  // polygon too); identity comes from source_circle_id / hole count.
  const polysmith::core::SketchProfileRegion* outer = &params.profiles[0];
  const polysmith::core::SketchProfileRegion* circle = &params.profiles[1];
  if (params.profiles[0].source_circle_id.has_value()) {
    std::swap(outer, circle);
  }
  if (!expect(outer->inner_loops.size() == 1 &&
                  circle->source_circle_id.has_value(),
              "capture: outer region with one hole + circle-sourced region")) {
    return false;
  }

  const auto outerRef =
      capture_profile_reference(sketch_feature_id(document), *outer);
  if (!expect(outerRef.has_value(), "capture: outer region witness")) {
    return false;
  }
  // The hole is sampled as a polygon, so the subtraction approximates
  // the circle area — allow the sampling slack.
  if (!expect(near(outerRef->area, 200.0 - kPi * 4.0, 0.5),
              "capture: outer area = rect minus circle")) {
    std::cerr << "  area: " << outerRef->area << "\n";
    return false;
  }
  if (!expect(outerRef->innerLoopCount == 1,
              "capture: outer region records its hole")) {
    return false;
  }
  if (!expect(outerRef->boundaryEdgeKinds.size() == 4 &&
                  outerRef->boundaryEdgeKinds[0] == "line",
              "capture: boundary signature is four lines")) {
    return false;
  }

  const auto circleRef =
      capture_profile_reference(sketch_feature_id(document), *circle);
  if (!expect(circleRef.has_value() &&
                  circleRef->sourceCircleId.has_value(),
              "capture: circle region carries its source circle id")) {
    return false;
  }
  if (!expect(near(circleRef->area, kPi * 4.0, 1.0e-4) &&
                  near(circleRef->centerX, 10.0) &&
                  near(circleRef->centerY, 5.0),
              "capture: circle witness carries exact center and area")) {
    return false;
  }

  return true;
}

// ── Test 2: resolve on the same sketch ────────────────────────────

bool test_resolve_same_sketch_found() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  const auto& params = sketch_params(document);
  if (!expect(params.profiles.size() == 1, "resolve: one region")) {
    return false;
  }
  const auto reference =
      capture_profile_reference(sketch_feature_id(document), params.profiles[0]);
  if (!expect(reference.has_value(), "resolve: witness captured")) {
    return false;
  }

  const auto result = resolve_profile_reference(*reference, params);
  if (!expect(result.outcome == ProfileResolutionOutcome::Found,
              "resolve: Found on the same sketch")) {
    std::cerr << "  outcome: " << static_cast<int>(result.outcome) << "\n";
    return false;
  }
  if (!expect(result.candidates.size() == 1 &&
                  result.candidates[0].region->id == params.profiles[0].id &&
                  result.candidates[0].score >= 0.9,
              "resolve: exactly one candidate, the same region, high score")) {
    return false;
  }
  return true;
}

// ── Test 3: resolve after a move, both displacement signs ─────────
//
// Moving the profile (same entities, same area, shifted centroid) must
// still resolve — the entity signature carries the identity.  Covered
// at both signs of the displacement per the binding epsilon rule.

bool test_resolve_after_move_both_signs() {
  const double displacements[2] = {30.0, -30.0};
  for (double dx : displacements) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document =
        manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
    const auto& params = sketch_params(document);
    const auto reference = capture_profile_reference(
        sketch_feature_id(document), params.profiles[0]);
    if (!reference.has_value()) {
      std::cerr << "  no witness at dx=" << dx << "\n";
      return false;
    }

    // Move the rectangle via the move tool (entity ids unchanged).
    std::vector<std::string> lineIds;
    for (const auto& line : params.lines) {
      lineIds.push_back(line.id);
    }
    document = manager.move_sketch_entities(lineIds, dx, 0.0, 0.0, 0.0, 0.0);
    const auto& moved = sketch_params(document);

    const auto result = resolve_profile_reference(*reference, moved);
    if (!expect(result.outcome == ProfileResolutionOutcome::Found,
                "resolve after move: Found")) {
      std::cerr << "  dx=" << dx << " outcome "
                << static_cast<int>(result.outcome) << "\n";
      if (!result.candidates.empty()) {
        std::cerr << "  best score " << result.candidates[0].score << "\n";
      }
      return false;
    }
    if (!expect(result.candidates[0].score >= 0.7,
                "resolve after move: score stays above threshold")) {
      std::cerr << "  dx=" << dx << " score " << result.candidates[0].score
                << "\n";
      return false;
    }
  }
  return true;
}

// ── Test 4: NotFound on unrelated geometry ────────────────────────

bool test_resolve_unrelated_sketch_not_found() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  const auto reference = capture_profile_reference(
      sketch_feature_id(document), sketch_params(document).profiles[0]);
  if (!reference.has_value()) {
    return false;
  }

  // A different sketch whose only region is a small circle.
  DocumentManager other;
  other.create_document();
  other.start_sketch_on_plane("ref-plane-xy");
  DocumentState otherDoc = other.add_sketch_circle(0.0, 0.0, 3.0);
  const auto& otherParams = sketch_params(otherDoc);

  const auto result = resolve_profile_reference(*reference, otherParams);
  return expect(result.outcome == ProfileResolutionOutcome::NotFound,
                "resolve: NotFound on unrelated geometry");
}

// ── Test 5: separated identical regions resolve by centroid ──────

bool test_resolve_separated_identical_regions() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_rectangle(50.0, 0.0, 70.0, 10.0);
  const auto& params = sketch_params(document);
  if (!expect(params.profiles.size() == 2,
              "separated: two regions detected")) {
    return false;
  }

  const auto reference = capture_profile_reference(
      sketch_feature_id(document), params.profiles[0]);
  if (!reference.has_value()) {
    return false;
  }
  const auto result = resolve_profile_reference(*reference, params);
  if (!expect(result.outcome == ProfileResolutionOutcome::Found,
              "separated: the witness centroid disambiguates")) {
    std::cerr << "  outcome: " << static_cast<int>(result.outcome) << "\n";
    return false;
  }
  return expect(result.candidates[0].region->id == params.profiles[0].id,
                "separated: the right region wins");
}

// ── Test 6: a genuine near-tie is ambiguous, never a guess ────────

bool test_resolve_true_ambiguity() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_rectangle(50.0, 0.0, 70.0, 10.0);
  const auto& params = sketch_params(document);
  if (!expect(params.profiles.size() == 2, "ambiguity: two regions")) {
    return false;
  }

  auto reference = capture_profile_reference(
      sketch_feature_id(document), params.profiles[0]);
  if (!reference.has_value()) {
    return false;
  }
  // Craft a witness whose centroid sits exactly BETWEEN the two
  // identical regions: both score a near-tie, and the resolver must
  // refuse to pick one.
  reference->centerX = 35.0;  // midpoint of the two rect centers
  reference->centerY = 5.0;

  const auto result = resolve_profile_reference(*reference, params);
  return expect(result.outcome == ProfileResolutionOutcome::Ambiguous,
                "ambiguity: near-tie candidates never guessed");
}

// ── Test 7: capture falls back to the selected sketch feature ─────

bool test_capture_from_selected_sketch_feature() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);

  const std::string sketchId = sketch_feature_id(document);
  document = manager.select_feature(sketchId);
  if (!expect(document.selected_feature_id == sketchId,
              "capture-from-feature: sketch selected")) {
    return false;
  }

  polysmith::core::CamOperation op;
  const bool captured = polysmith::core::capture_profile_references_from_selection(
      document, op);
  if (!expect(captured, "capture-from-feature: captured")) {
    return false;
  }
  // The circle is BOTH a standalone region and an inner loop of the
  // outer region — the whole-sketch capture keeps only the outer
  // region (its inner loop cuts the hole), never a duplicate cut.
  if (!expect(op.geometry_references.machining_regions.size() == 1,
              "capture-from-feature: hole region deduped")) {
    return false;
  }

  // An EXPLICIT profile selection still captures the circle itself.
  const auto& sketchParams = sketch_params(document);
  const auto circleRegion = std::find_if(
      sketchParams.profiles.begin(), sketchParams.profiles.end(),
      [](const auto& region) { return region.source_circle_id.has_value(); });
  if (!expect(circleRegion != sketchParams.profiles.end(),
              "capture-explicit: circle region found")) {
    return false;
  }
  document = manager.select_sketch_profile(circleRegion->id,
                                           /*additive=*/false);
  polysmith::core::CamOperation explicitOp;
  const bool explicitCaptured =
      polysmith::core::capture_profile_references_from_selection(
          document, explicitOp);
  return expect(explicitCaptured &&
                    explicitOp.geometry_references.machining_regions.size() ==
                        1,
                "capture-explicit: selected circle captured as-is");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam_profile_reference_test\n";
  std::cout << "  Test 1: capture rectangle + hole witness... ";
  if (test_capture_rectangle_with_hole()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: resolve on same sketch... ";
  if (test_resolve_same_sketch_found()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: resolve after move (both signs)... ";
  if (test_resolve_after_move_both_signs()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: NotFound on unrelated geometry... ";
  if (test_resolve_unrelated_sketch_not_found()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: separated identical regions resolve... ";
  if (test_resolve_separated_identical_regions()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: true near-tie is ambiguous... ";
  if (test_resolve_true_ambiguity()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: capture from selected sketch feature... ";
  if (test_capture_from_selected_sketch_feature()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cam_profile_reference_test passed\n";
    return 0;
  }
  return 1;
}
