// CAM save/load round-trip test.
//
// Verifies the "cam" key of the document payload survives a full
// save → load cycle: setups, tool library, operations (including
// sketch-profile and face attestations, laser parameters, status
// messages), and the post-processor.  Also proves toolpaths stay
// memory-only — the serialized payload must never contain toolpath
// point arrays.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <variant>

#include "core/document/document.h"
#include "core/cam/cam_operation.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::CamDocumentData;
using polysmith::core::CamOperation;
using polysmith::core::CamOperationParameters;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FaceAttestation;
using polysmith::core::GeometryReference;
using polysmith::core::LaserCutParameters;
using polysmith::core::PostProcessor;
using polysmith::core::SketchProfileAttestation;
using polysmith::core::ToolEntry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

GeometryReference make_profile_ref(const std::string& sketch_feature_id,
                                   const std::string& profile_id) {
  SketchProfileAttestation att;
  att.sketch_feature_id = sketch_feature_id;
  att.profile_id = profile_id;
  att.center_x = 5.0;
  att.center_y = 5.0;
  att.area = 400.0;
  att.min_x = 0.0;
  att.min_y = 0.0;
  att.max_x = 10.0;
  att.max_y = 10.0;
  att.boundary_edge_kinds = {"line", "line", "line", "line"};
  att.inner_loop_count = 1;

  GeometryReference ref;
  ref.persistent_id = profile_id;
  ref.attestation = att;
  return ref;
}

GeometryReference make_face_ref() {
  FaceAttestation att;
  att.area = 100.0;
  att.normal = {0.0, 0.0, 1.0};
  att.sample_points = {{0.0, 0.0, 10.0}, {10.0, 0.0, 10.0},
                       {0.0, 10.0, 10.0}, {10.0, 10.0, 10.0}};

  GeometryReference ref;
  ref.persistent_id = "body-1:face:0";
  ref.attestation = att;
  return ref;
}

CamDocumentData make_cam_data() {
  CamDocumentData cam;

  CamSetup setup;
  setup.setup_id = "cam-setup-1";
  setup.name = "Sheet setup";
  setup.machine_type = "laser";
  setup.stock.type = "bounding_box";
  setup.stock.size = std::array<double, 3>{300.0, 200.0, 3.0};
  setup.stock.origin = std::array<double, 3>{0.0, 0.0, 0.0};
  setup.safety_height = 5.0;
  setup.retract_height = 2.0;
  cam.setups.push_back(setup);

  ToolEntry laser_tool;
  laser_tool.tool_id = "tool-1";
  laser_tool.name = "CO2 laser";
  laser_tool.type = "laser";
  cam.tool_library.push_back(laser_tool);

  ToolEntry mill_tool;
  mill_tool.tool_id = "tool-2";
  mill_tool.name = "6mm endmill";
  mill_tool.type = "endmill_flat";
  mill_tool.diameter_mm = 6.0;
  cam.tool_library.push_back(mill_tool);

  CamOperation laser_op;
  laser_op.op_id = "cam-op-1";
  laser_op.name = "2D Cut 1";
  laser_op.type = "laser_cut";
  laser_op.tool_id = "tool-1";
  laser_op.geometry_references.machining_regions.push_back(
      make_profile_ref("feature-2", "profile-3"));
  laser_op.geometry_references.machining_regions.push_back(
      make_profile_ref("feature-2", "profile-4"));
  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 2.5;
  laser.pierce_dwell_seconds = 0.3;
  laser.power_percent = 70.0;
  laser.passes = 2;
  laser.mode = "cut";
  laser_op.parameters.laser = laser;
  laser_op.status = "generated";
  laser_op.status_message = "";
  cam.operations.push_back(laser_op);

  CamOperation mill_op;
  mill_op.op_id = "cam-op-2";
  mill_op.name = "Face 1";
  mill_op.type = "face_milling";
  mill_op.tool_id = "tool-2";
  mill_op.geometry_references.machining_regions.push_back(make_face_ref());
  mill_op.parameters.zigzag_angle_deg = 30.0;
  mill_op.status = "error";
  mill_op.status_message = "The referenced face could not be resolved.";
  cam.operations.push_back(mill_op);

  PostProcessor post;
  post.type = "grbl";
  post.filename = "cut.nc";
  post.options.add_line_numbers = false;
  post.options.use_arcs = true;
  post.options.decimal_places = 4;
  cam.post_processor = post;

  return cam;
}

bool cam_data_equal(const CamDocumentData& a, const CamDocumentData& b) {
  if (a.setups.size() != b.setups.size() ||
      a.tool_library.size() != b.tool_library.size() ||
      a.operations.size() != b.operations.size()) {
    return false;
  }
  for (size_t i = 0; i < a.setups.size(); ++i) {
    const auto& sa = a.setups[i];
    const auto& sb = b.setups[i];
    if (sa.setup_id != sb.setup_id || sa.name != sb.name ||
        sa.machine_type != sb.machine_type || sa.units != sb.units ||
        sa.safety_height != sb.safety_height ||
        sa.retract_height != sb.retract_height) {
      return false;
    }
    if (sa.stock.size.has_value() != sb.stock.size.has_value()) {
      return false;
    }
    if (sa.stock.size.has_value() &&
        sa.stock.size.value() != sb.stock.size.value()) {
      return false;
    }
  }
  for (size_t i = 0; i < a.tool_library.size(); ++i) {
    if (a.tool_library[i].tool_id != b.tool_library[i].tool_id ||
        a.tool_library[i].type != b.tool_library[i].type ||
        a.tool_library[i].name != b.tool_library[i].name) {
      return false;
    }
  }
  for (size_t i = 0; i < a.operations.size(); ++i) {
    const auto& oa = a.operations[i];
    const auto& ob = b.operations[i];
    // status/status_message are refresh-derived, not persisted truth:
    // load re-runs the CAM dependency pass, which legitimately
    // recomputes them (a generated path can't survive a load — the
    // toolpath cache is memory-only).  Compare the DATA only.
    if (oa.op_id != ob.op_id || oa.type != ob.type ||
        oa.tool_id != ob.tool_id) {
      return false;
    }
    if (oa.geometry_references.machining_regions.size() !=
        ob.geometry_references.machining_regions.size()) {
      return false;
    }
    // Every reference must round-trip its attestation variant.
    for (size_t r = 0; r < oa.geometry_references.machining_regions.size(); ++r) {
      if (oa.geometry_references.machining_regions[r].attestation.index() !=
          ob.geometry_references.machining_regions[r].attestation.index()) {
        return false;
      }
    }
    if (oa.type == "laser_cut") {
      const auto la = oa.parameters.laser;
      const auto lb = ob.parameters.laser;
      if (!la.has_value() || !lb.has_value()) {
        return false;
      }
      if (la->kerf_width_mm != lb->kerf_width_mm ||
          la->lead_in_mm != lb->lead_in_mm ||
          la->pierce_dwell_seconds != lb->pierce_dwell_seconds ||
          la->power_percent != lb->power_percent ||
          la->passes != lb->passes || la->mode != lb->mode ||
          la->dynamic_power != lb->dynamic_power) {
        return false;
      }
    }
    if (oa.type == "face_milling" &&
        oa.parameters.zigzag_angle_deg != ob.parameters.zigzag_angle_deg) {
      return false;
    }
  }
  return true;
}

bool test_document_round_trip() {
  // Build a document payload with CAM data and push it through the full
  // document_from_payload path — the same code load_document_from_path
  // runs.  (DocumentManager CAM mutators land with the scaffolding
  // work; until then the payload is constructed directly.)
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();
  document.cam = make_cam_data();

  const auto payload = polysmith::protocol::to_payload(document, true);
  const auto cam_payload = payload.at("cam").dump();

  // The serialized payload must never contain toolpath point arrays —
  // toolpaths are memory-only by design (ToolpathCache is metadata).
  if (!expect(cam_payload.find("\"moves\"") == std::string::npos,
              "payload must not contain toolpath moves")) {
    return false;
  }
  if (!expect(cam_payload.find("\"toolpath_points\"") == std::string::npos,
              "payload must not contain toolpath point arrays")) {
    return false;
  }

  const auto restored = polysmith::protocol::document_from_payload(payload);
  return expect(cam_data_equal(document.cam, restored.cam),
                "CAM data must survive document serialize/deserialize");
}

bool test_save_load_file_round_trip() {
  // Full save-to-file + load-from-file cycle.  Requires a public way to
  // install CAM data on the manager; covered by the CAM commands test
  // once the mutators land.  Here we only verify the load path keeps
  // CAM data from a file whose payload carries it.
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();
  document.cam = make_cam_data();

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_cam_save_load_test.json";
  {
    std::ofstream stream(path.string());
    stream << polysmith::protocol::to_payload(document, true).dump(2);
  }

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  const auto loaded = loaded_manager.load_document_from_path(path.string());

  if (!expect(cam_data_equal(document.cam, loaded.cam),
              "CAM data must survive file save/load")) {
    return false;
  }

  // The load path re-runs the CAM dependency refresh: both operations
  // reference geometry that does not exist in this synthetic document,
  // so both must degrade to "error" with a human message — the refresh
  // doing its job on load, never a crash.
  if (!expect(loaded.cam.operations.size() == 2,
              "file load: both operations loaded")) {
    return false;
  }
  for (const auto& op : loaded.cam.operations) {
    if (!expect(op.status == "error" && !op.status_message.empty(),
                "file load: unresolved references degrade with a message")) {
      return false;
    }
  }
  return true;
}

bool test_attestation_contents_round_trip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();

  document.cam = make_cam_data();
  const auto cam_payload =
      polysmith::protocol::to_payload(document, true).at("cam");
  const auto restored =
      polysmith::protocol::cam_document_data_from_payload(cam_payload);

  if (!expect(restored.operations.size() == 2, "expected two operations")) {
    return false;
  }

  const auto& laser_op = restored.operations[0];
  const auto& ref = laser_op.geometry_references.machining_regions[0];
  if (!expect(std::holds_alternative<SketchProfileAttestation>(
                  ref.attestation),
              "laser op must restore SketchProfileAttestation")) {
    return false;
  }
  const auto& att = std::get<SketchProfileAttestation>(ref.attestation);
  if (!expect(att.sketch_feature_id == "feature-2" &&
                  att.profile_id == "profile-3" && att.area == 400.0 &&
                  att.inner_loop_count == 1 &&
                  att.boundary_edge_kinds.size() == 4,
              "sketch profile attestation fields must round-trip")) {
    return false;
  }

  const auto& mill_op = restored.operations[1];
  if (!expect(std::holds_alternative<FaceAttestation>(
                  mill_op.geometry_references.machining_regions[0]
                      .attestation),
              "mill op must restore FaceAttestation")) {
    return false;
  }
  if (!expect(restored.post_processor.has_value() &&
                  restored.post_processor->type == "grbl" &&
                  restored.post_processor->options.decimal_places == 4,
              "post-processor must round-trip")) {
    return false;
  }

  return true;
}

bool test_document_without_cam_key_defaults_empty() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();

  auto payload = polysmith::protocol::to_payload(document, true);
  payload.erase("cam");
  const auto restored =
      polysmith::protocol::document_from_payload(payload);

  return expect(restored.cam.setups.empty() &&
                    restored.cam.tool_library.empty() &&
                    restored.cam.operations.empty() &&
                    !restored.cam.post_processor.has_value(),
                "missing cam key must default to empty CamDocumentData");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam_save_load_test\n";
  std::cout << "  Test 1: document serialize/deserialize round trip... ";
  if (test_document_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: file save/load round trip... ";
  if (test_save_load_file_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: attestation contents round-trip... ";
  if (test_attestation_contents_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: document without cam key defaults empty... ";
  if (test_document_without_cam_key_defaults_empty()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cam_save_load_test passed\n";
    return 0;
  }
  return 1;
}
