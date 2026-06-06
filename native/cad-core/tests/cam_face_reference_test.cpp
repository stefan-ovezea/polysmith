#include <cmath>
#include <iostream>
#include <string>

#include "core/body_compiler.h"
#include "core/cam_operation.h"
#include "core/document.h"
#include "core/feature.h"

namespace {

using polysmith::core::CamFaceReference;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FaceResolutionOutcome;
using polysmith::core::capture_face_reference;
using polysmith::core::compile_bodies;
using polysmith::core::resolve_face_reference;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

// ── Test 1: Capture and re-resolve on the same body ───────────────
//
// Create a box, capture face 0 (the top face), resolve immediately.
// Should find exactly one candidate with the same face index.

bool test_capture_and_resolve_same_body() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature({.width = 20.0,
                                                     .height = 20.0,
                                                     .depth = 10.0});

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "expected one body from box")) {
    return false;
  }

  const auto& body = compiled.bodies[0];

  // Box has 6 faces (OCCT order). Face 4 is typically the top face (+Z).
  const int topFaceIndex = 4;
  auto ref = capture_face_reference(body.id, body.shape, topFaceIndex, "top");
  if (!expect(ref.has_value(), "capture_face_reference returned nullopt")) {
    return false;
  }

  if (!expect(ref->samplePoints.size() >= 9,
              "expected at least 9 sample points")) {
    return false;
  }
  if (!expect(ref->capturedArea > 0.0, "expected positive captured area")) {
    return false;
  }

  // Resolve against the same body.
  const auto result = resolve_face_reference(*ref, body.shape);

  if (!expect(result.outcome == FaceResolutionOutcome::Found,
              "expected Found outcome on same body")) {
    std::cerr << "  outcome was: "
              << static_cast<int>(result.outcome) << "\n";
    std::cerr << "  candidates: " << result.candidates.size() << "\n";
    return false;
  }

  if (!expect(result.candidates.size() == 1,
              "expected exactly one candidate")) {
    return false;
  }

  if (!expect(result.candidates[0].faceIndex == topFaceIndex,
              "expected same face index")) {
    std::cerr << "  got faceIndex: " << result.candidates[0].faceIndex
              << " (expected " << topFaceIndex << ")\n";
    return false;
  }

  if (!expect(result.candidates[0].score >= 0.9,
              "expected high score (>= 0.9) for same face")) {
    std::cerr << "  score: " << result.candidates[0].score << "\n";
    return false;
  }

  return true;
}

// ── Test 2: Resolve against a different body ──────────────────────
//
// Capture a face on one body, resolve against a different body.
// Should return NotFound.

bool test_resolve_wrong_body() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "expected one body")) {
    return false;
  }

  const auto& body = compiled.bodies[0];
  auto ref = capture_face_reference(body.id, body.shape, 0, "face on box 1");
  if (!expect(ref.has_value(), "capture returned nullopt")) {
    return false;
  }

  // Create a separate document with a tiny body — no face will
  // match the 20×20 area or the sample point distribution.
  DocumentManager manager2;
  manager2.create_document();
  DocumentState document2 =
      manager2.add_box_feature({.width = 2.0, .height = 2.0, .depth = 2.0});

  const auto compiled2 = compile_bodies(document2);
  if (!expect(compiled2.bodies.size() == 1, "expected one body in doc2")) {
    return false;
  }

  // Resolve reference (from body 1) against body 2's shape.
  const auto result = resolve_face_reference(*ref, compiled2.bodies[0].shape);

  // Sample points from a 20×20 face won't lie on a 5×5 face, and
  // area will differ dramatically. Should be NotFound.
  if (!expect(result.outcome == FaceResolutionOutcome::NotFound,
              "expected NotFound when resolving on wrong body")) {
    std::cerr << "  outcome: " << static_cast<int>(result.outcome)
              << ", candidates: " << result.candidates.size() << "\n";
    for (const auto& c : result.candidates) {
      std::cerr << "    faceIndex=" << c.faceIndex
                << " score=" << c.score << "\n";
    }
    return false;
  }

  return true;
}

// ── Test 3: Resolve via DocumentState convenience overload ────────
//
// Same as test 1 but using the DocumentState-based resolve overload.
// Verifies the body-lookup path works.

bool test_resolve_via_document_state() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 30.0, .height = 20.0, .depth = 15.0});

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "expected one body")) {
    return false;
  }

  const auto& body = compiled.bodies[0];
  auto ref = capture_face_reference(body.id, body.shape, 2, "side face");
  if (!expect(ref.has_value(), "capture returned nullopt")) {
    return false;
  }

  // Resolve through the DocumentState API.
  const auto result = resolve_face_reference(*ref, document);

  if (!expect(result.outcome == FaceResolutionOutcome::Found,
              "expected Found via DocumentState API")) {
    std::cerr << "  outcome: " << static_cast<int>(result.outcome)
              << ", candidates: " << result.candidates.size() << "\n";
    return false;
  }

  if (!expect(result.candidates.size() == 1,
              "expected exactly one candidate")) {
    return false;
  }

  if (!expect(result.candidates[0].score >= 0.9,
              "expected high score")) {
    return false;
  }

  return true;
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam_face_reference_test\n";
  std::cout << "  Test 1: capture and resolve same body... ";
  if (test_capture_and_resolve_same_body()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: resolve on wrong body... ";
  if (test_resolve_wrong_body()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: resolve via DocumentState API... ";
  if (test_resolve_via_document_state()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cam_face_reference_test passed\n";
    return 0;
  }
  return 1;
}
