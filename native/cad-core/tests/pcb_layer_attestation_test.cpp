// Regression suite for the PCB layer geometry reference (P2c, identity half).
//
// A CAM operation that cuts an imported layer references the WHOLE layer by
// feature id plus a witness of what the layer held (ring count + bounds), and
// the rings are re-derived from the source file on every recompute — the
// project's re-resolve-every-time rule.  What matters here is therefore the
// behaviour of that re-identification:
//
//   * a healthy layer resolves, and reports "unchanged";
//   * a layer whose source was re-plotted still resolves, but says "changed" so
//     the caller can warn instead of silently cutting different copper;
//   * a deleted feature, or a source file that vanished, degrades with a
//     human-readable reason rather than throwing or resolving to something
//     wrong.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "core/cam/cam_resolution.h"
#include "core/cam/cam_types.h"
#include "core/document/document.h"
#include "core/pcb/pcb_layer_io.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::CamGeometryReferences;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::GeometryReference;
using polysmith::core::PcbLayerAttestation;
using polysmith::protocol::cam_geometry_references_from_payload;
using polysmith::protocol::to_payload;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << "  FAIL: " << message << "\n";
  return false;
}

std::string fixture(const std::string& name) {
  return "native/cad-core/tests/data/pcb/" + name;
}

bool have_fixtures() {
  std::error_code error;
  return std::filesystem::exists(fixture("copper_trimmed.gbr"), error);
}

const FeatureEntry* find_layer(const DocumentState& document) {
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.kind == "pcb_layer" && feature.pcb_layer_parameters.has_value()) {
      return &feature;
    }
  }
  return nullptr;
}

/// Builds the attestation the PCB workspace would capture for a layer.
PcbLayerAttestation attestation_for(const FeatureEntry& layer) {
  const auto& params = *layer.pcb_layer_parameters;
  PcbLayerAttestation attestation;
  attestation.layer_feature_id = layer.id;
  attestation.role = polysmith::core::pcb_role_id(params.role);
  attestation.ring_count = params.ring_count;
  attestation.min_x = params.min_x;
  attestation.min_y = params.min_y;
  attestation.max_x = params.max_x;
  attestation.max_y = params.max_y;
  return attestation;
}

bool test_healthy_layer_resolves_unchanged() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }

  const auto resolved = polysmith::core::resolve_pcb_layer_attestation(
      attestation_for(*layer), document);
  bool ok = expect(resolved.found, "a healthy layer resolves");
  ok &= expect(resolved.ring_count == 20, "20 rings re-derived from the source");
  ok &= expect(!resolved.changed, "the witness still matches");
  // The re-derived bounds must agree with what the import recorded.  This plot
  // starts at negative X, so no positive-only assumption is safe here.
  const auto& params = *layer->pcb_layer_parameters;
  ok &= expect(std::abs(resolved.min_x - params.min_x) < 0.01 &&
                   std::abs(resolved.min_y - params.min_y) < 0.01 &&
                   std::abs(resolved.max_x - params.max_x) < 0.01 &&
                   std::abs(resolved.max_y - params.max_y) < 0.01,
               "the re-derived bounds match the import summary");
  ok &= expect(resolved.max_x > 90.0 && resolved.max_y > 90.0,
               "and they cover the board");
  ok &= expect(polysmith::core::pcb_layer_reference_failure_message(resolved).empty(),
               "no failure message when it resolved");
  return ok;
}

bool test_replot_still_cuts_but_reports_changed() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // A layer whose source was replaced is a *different* layer, not a broken
  // reference: the operation must keep cutting (the fix for a re-poured board
  // is the new copper, not a vanished toolpath) while telling the user.
  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() / "pcb_attestation_replot.gbr";
  std::error_code error;
  std::filesystem::copy_file(fixture("edge_cuts.gbr"), scratch,
                             std::filesystem::copy_options::overwrite_existing, error);

  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_pcb_layer(scratch.string());
  const FeatureEntry* layer = find_layer(imported);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const PcbLayerAttestation attestation = attestation_for(*layer);

  // Re-plot the same path with a different layer (a paste layer: 605 rings).
  std::filesystem::copy_file(fixture("paste_bottom.gbr"), scratch,
                             std::filesystem::copy_options::overwrite_existing, error);

  const auto resolved =
      polysmith::core::resolve_pcb_layer_attestation(attestation, imported);
  bool ok = expect(resolved.found, "the replotted layer still resolves");
  ok &= expect(resolved.changed, "and reports that it changed");
  ok &= expect(resolved.ring_count != attestation.ring_count,
               "the re-derived ring count is the new one");
  std::filesystem::remove(scratch, error);
  return ok;
}

bool test_missing_feature_and_missing_source_degrade() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // Vanished feature: the reference names a layer that is no longer there.
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  PcbLayerAttestation stale = attestation_for(*layer);
  stale.layer_feature_id = "feature-does-not-exist";
  const auto missing_feature =
      polysmith::core::resolve_pcb_layer_attestation(stale, document);
  bool ok = expect(!missing_feature.found, "an unknown feature does not resolve");
  ok &= expect(polysmith::core::pcb_layer_reference_failure_message(missing_feature)
                   .find("no longer in the document") != std::string::npos,
               "and says the layer is gone");

  // Vanished source file: the layer is still in the document, but nothing can
  // be re-derived.
  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() / "pcb_attestation_vanish.gbr";
  std::error_code error;
  std::filesystem::copy_file(fixture("edge_cuts.gbr"), scratch,
                             std::filesystem::copy_options::overwrite_existing, error);
  manager.create_document();
  const DocumentState second = manager.import_pcb_layer(scratch.string());
  const FeatureEntry* second_layer = find_layer(second);
  if (second_layer == nullptr) {
    return expect(false, "second layer imported");
  }
  const PcbLayerAttestation attestation = attestation_for(*second_layer);
  std::filesystem::remove(scratch, error);

  const auto missing_source =
      polysmith::core::resolve_pcb_layer_attestation(attestation, second);
  ok &= expect(!missing_source.found, "a vanished source does not resolve");
  ok &= expect(polysmith::core::pcb_layer_reference_failure_message(missing_source)
                   .find("not found") != std::string::npos,
               "and names the missing file");
  return ok;
}

bool test_drill_layer_resolves_with_hits_not_rings() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // A drill file has no rings at all; resolution must accept it on its hits,
  // or a pcb_drill operation could never resolve.
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_pcb_layer(fixture("pth.drl"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "drill layer imported");
  }
  const auto resolved = polysmith::core::resolve_pcb_layer_attestation(
      attestation_for(*layer), document);
  bool ok = expect(resolved.found, "a drill layer resolves on its holes");
  ok &= expect(resolved.ring_count == 0, "no rings");
  ok &= expect(resolved.hit_count == 511 && resolved.tool_count == 13,
               "511 holes across 13 tools re-derived from the file");
  return ok;
}

bool test_attestation_survives_the_wire_format() {
  // The identity must survive a save/load, or an operation loses its layer
  // every time the project is reopened.
  PcbLayerAttestation attestation;
  attestation.layer_feature_id = "feature-7";
  attestation.role = "copper_top";
  attestation.ring_count = 2017;
  attestation.min_x = 1.25;
  attestation.min_y = 2.5;
  attestation.max_x = 99.5;
  attestation.max_y = 98.75;

  GeometryReference reference;
  reference.persistent_id = "persistent-1";
  reference.attestation = attestation;

  CamGeometryReferences references;
  references.machining_regions.push_back(reference);
  const nlohmann::json payload = to_payload(references);
  const CamGeometryReferences restored =
      cam_geometry_references_from_payload(payload);

  bool ok = expect(restored.machining_regions.size() == 1, "one reference survives");
  if (restored.machining_regions.size() != 1) {
    return false;
  }
  const auto& restored_ref = restored.machining_regions.front();
  ok &= expect(restored_ref.persistent_id == "persistent-1", "persistent id survives");
  if (!expect(std::holds_alternative<PcbLayerAttestation>(restored_ref.attestation),
              "it comes back as a PCB layer reference, not a face or sketch")) {
    return false;
  }
  const auto& back = std::get<PcbLayerAttestation>(restored_ref.attestation);
  ok &= expect(back.layer_feature_id == "feature-7", "feature id survives");
  ok &= expect(back.role == "copper_top", "role id survives");
  ok &= expect(back.ring_count == 2017, "ring count survives");
  ok &= expect(std::abs(back.max_x - 99.5) < 1e-9 && std::abs(back.min_y - 2.5) < 1e-9,
               "bounds survive");
  return ok;
}

}  // namespace

int main() {
  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"healthy_layer_resolves_unchanged", test_healthy_layer_resolves_unchanged},
      {"replot_still_cuts_but_reports_changed", test_replot_still_cuts_but_reports_changed},
      {"missing_feature_and_missing_source_degrade",
       test_missing_feature_and_missing_source_degrade},
      {"drill_layer_resolves_with_hits_not_rings",
       test_drill_layer_resolves_with_hits_not_rings},
      {"attestation_survives_the_wire_format",
       test_attestation_survives_the_wire_format},
  };

  int failures = 0;
  for (const Suite& suite : suites) {
    std::cout << "case " << suite.name << std::flush << "\n";
    if (!suite.run()) {
      ++failures;
    }
  }
  if (failures != 0) {
    std::cerr << failures << " case(s) failed\n";
    return 1;
  }
  std::cout << "pcb layer attestation: all cases passed\n";
  return 0;
}
