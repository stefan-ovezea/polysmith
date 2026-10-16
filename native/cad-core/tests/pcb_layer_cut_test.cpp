// End-to-end suite for cutting an imported PCB layer (P2c cutting half).
//
// The claim under test is the whole chain: a `pcb_layer` feature is imported,
// a laser operation references the WHOLE layer, the generator re-derives the
// rings from the source file and turns each one into its own planned contour,
// and the resulting toolpath is real world-XY geometry — one closed
// laser-on run per ring, offset outward by the kerf.
//
// The board profile (Edge_Cuts) is the natural first case: 20 rings, arcs
// included, and the outline is what a first cut would actually trace.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/cam_types.h"
#include "core/cam/toolpath.h"
#include "core/document/document.h"
#include "core/pcb/pcb_layer_io.h"

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::GeometryReference;
using polysmith::core::LaserCutParameters;
using polysmith::core::PcbLayerAttestation;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMoveKind;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << "  FAIL: " << message << "\n";
  return false;
}

bool near(double actual, double expected, double tolerance) {
  return std::abs(actual - expected) <= tolerance;
}

std::string fixture(const std::string& name) {
  return "native/cad-core/tests/data/pcb/" + name;
}

bool have_fixtures() {
  std::error_code error;
  return std::filesystem::exists(fixture("edge_cuts.gbr"), error);
}

const FeatureEntry* find_layer(const DocumentState& document) {
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.kind == "pcb_layer" && feature.pcb_layer_parameters.has_value()) {
      return &feature;
    }
  }
  return nullptr;
}

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

/// Laser setup + tool, then an operation cutting the layer described by
/// `attestation`.  Takes the attestation BY VALUE: this helper replaces the
/// caller's DocumentState (every mutator returns a fresh one), which
/// invalidates any pointer into the previous feature list.
std::string add_laser_operation(DocumentManager& manager, DocumentState& document,
                                const PcbLayerAttestation& attestation,
                                const LaserCutParameters& laser) {
  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "PCB outline cut";
  op.type = "laser_cut";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.laser = laser;

  GeometryReference reference;
  reference.persistent_id = attestation.layer_feature_id;
  reference.attestation = attestation;
  op.geometry_references.machining_regions.push_back(reference);

  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

/// Maximum x of any laser-on move.
double laser_on_max_x(const Toolpath& toolpath, bool* any) {
  double maximum = -1e9;
  *any = false;
  for (const auto& move : toolpath.moves) {
    if (move.laser_on) {
      maximum = std::max(maximum, move.x);
      *any = true;
    }
  }
  return maximum;
}

bool test_layer_rings_cut_as_one_contour_each() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const int rings = layer->pcb_layer_parameters->ring_count;
  // Everything read off `layer` must be COPIED before the helper below: it
  // replaces the caller's DocumentState, which invalidates this pointer.
  const double layer_min_x = layer->pcb_layer_parameters->min_x;
  const double layer_min_y = layer->pcb_layer_parameters->min_y;
  const double layer_max_x = layer->pcb_layer_parameters->max_x;
  const double layer_max_y = layer->pcb_layer_parameters->max_y;
  const PcbLayerAttestation attestation = attestation_for(*layer);

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;  // offset 0.1 outward
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.power_percent = 80.0;
  const std::string op_id =
      add_laser_operation(manager, document, attestation, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), op_id, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "a laser cut on an imported layer generates")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // One closed laser-on run per CUTTABLE ring.  Not every assembled ring can be
  // cut: the profile is drawn with a 0.1 mm hairline aperture, so assembly
  // turns it into 0.1 mm-wide ribbons (an outer boundary plus an inner one per
  // outline), and an inner boundary narrower than the kerf collapses — it is
  // reported as a skipped contour rather than silently dropped.  So the
  // invariant is: contours + reported skips == assembled rings.
  int contours = 0;
  bool previous_on = false;
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && !previous_on) {
      ++contours;
    }
    previous_on = move.laser_on;
  }
  int skipped = 0;
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("was skipped") != std::string::npos) {
      ++skipped;
    }
  }
  if (skipped > 0) {
    std::cout << "  " << contours << " contours cut, " << skipped
              << " skipped (of " << rings << " assembled rings)\n";
  }
  {
    // Diagnostic: where did the beam spend its time?  Distinguishes "rings were
    // dropped" from "contours were chained with the beam left on".
    int rapids = 0;
    int laser_moves = 0;
    int dark_travel = 0;  // non-rapid moves with the beam OFF
    for (const auto& move : toolpath.moves) {
      if (move.kind == ToolpathMoveKind::Rapid) {
        ++rapids;
      }
      if (move.laser_on) {
        ++laser_moves;
      } else if (move.kind != ToolpathMoveKind::Rapid) {
        ++dark_travel;
      }
    }
    std::cout << "  moves=" << toolpath.moves.size() << " rapids=" << rapids
              << " laser_on=" << laser_moves << " beam_off_feeds=" << dark_travel
              << "\n";
  }
  bool ok = expect(contours + skipped == rings,
                   "every assembled ring is either cut or reported skipped (" +
                       std::to_string(rings) + " rings, " +
                       std::to_string(contours) + " cut, " +
                       std::to_string(skipped) + " skipped)");
  ok &= expect(contours > 0, "the layer does cut");

  // The cut is real world-XY geometry at the plane offset, not the sketch
  // plane's local frame.
  bool any_laser = false;
  const double max_x = laser_on_max_x(toolpath, &any_laser);
  ok &= expect(any_laser, "the toolpath actually fires the laser");
  ok &= expect(max_x > layer_max_x,
               "the kerf pushes the cut outside the drawn outline");
  ok &= expect(max_x <= layer_max_x + 0.25, "and by no more than the kerf");
  double min_x = 1e9;
  double max_y = -1e9;
  for (const auto& move : toolpath.moves) {
    min_x = std::min(min_x, move.x);
    max_y = std::max(max_y, move.y);
  }
  ok &= expect(min_x >= layer_min_x - 0.25 && max_y <= layer_max_y + 0.25,
               "no move strays beyond the layer by more than the kerf");
  ok &= expect(!toolpath.moves.empty(), "the toolpath is not empty");
  return ok;
}

bool test_drill_only_layer_reports_rather_than_cuts_nothing() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // A drill file has no contours.  Laser-cutting it must say so instead of
  // succeeding with an empty toolpath, which would read as a finished cut.
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_pcb_layer(fixture("pth.drl"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "drill layer imported");
  }
  LaserCutParameters laser;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const PcbLayerAttestation attestation = attestation_for(*layer);
  const std::string op_id =
      add_laser_operation(manager, document, attestation, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), op_id, /*preview=*/false);
  bool ok = expect(!outcome.result.ok,
                   "cutting a drill-only layer fails rather than cutting nothing");
  ok &= expect(outcome.result.error_message.find("loops") != std::string::npos ||
                   outcome.result.error_message.find("contour") != std::string::npos,
               "and explains that there is nothing to cut: " +
                   outcome.result.error_message);
  bool explained = false;
  for (const auto& warning : outcome.result.warnings) {
    explained = explained ||
                warning.find("drill data only") != std::string::npos;
  }
  ok &= expect(explained, "with a warning naming the drill data");
  return ok;
}

bool test_missing_layer_source_degrades_with_a_reason() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // The layer is deleted after the operation referenced it: generation must
  // come back with the resolver's message, not an empty success.
  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() / "pcb_cut_vanishing.gbr";
  std::error_code error;
  std::filesystem::copy_file(fixture("edge_cuts.gbr"), scratch,
                             std::filesystem::copy_options::overwrite_existing, error);

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_pcb_layer(scratch.string());
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const std::string layer_id = layer->id;
  LaserCutParameters laser;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const PcbLayerAttestation attestation = attestation_for(*layer);
  const std::string op_id =
      add_laser_operation(manager, document, attestation, laser);

  document = manager.remove_pcb_layer(layer_id);
  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), op_id, /*preview=*/false);
  bool ok = expect(!outcome.result.ok, "a deleted layer does not cut");
  ok &= expect(outcome.result.error_message.find("no longer in the document") !=
                   std::string::npos,
               "and says the layer is gone: " + outcome.result.error_message);
  std::filesystem::remove(scratch, error);
  return ok;
}

/// Total travel of rapid moves (beam off) — the number "path optimisation"
/// is trying to shrink on a layer of thousands of independent contours.
double rapid_travel(const Toolpath& toolpath) {
  double total = 0.0;
  for (size_t index = 1; index < toolpath.moves.size(); ++index) {
    const auto& previous = toolpath.moves[index - 1];
    const auto& move = toolpath.moves[index];
    if (move.kind != ToolpathMoveKind::Rapid) {
      continue;
    }
    total += std::hypot(move.x - previous.x, move.y - previous.y);
  }
  return total;
}

/// Cuts the same layer with each ordering strategy and reports the rapid
/// travel.  This is the measurement the optimisation work is judged against:
/// on a real copper layer every ring is an independent contour, so ordering is
/// the dominant cost.
bool test_cut_order_reduces_travel() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_pcb_layer(fixture("copper_trimmed.gbr"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const PcbLayerAttestation attestation = attestation_for(*layer);
  const double layer_span = layer->pcb_layer_parameters->max_x -
                            layer->pcb_layer_parameters->min_x;

  // Two operations on the same layer, one per strategy.
  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  document = manager.cam_setup_create(setup);
  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  const auto add_op = [&](const std::string& name, const std::string& order) {
    CamOperation op;
    op.name = name;
    op.type = "laser_cut";
    op.tool_id = document.cam.tool_library[0].tool_id;
    LaserCutParameters laser;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    laser.cut_order = order;
    op.parameters.laser = laser;
    GeometryReference reference;
    reference.persistent_id = attestation.layer_feature_id;
    reference.attestation = attestation;
    op.geometry_references.machining_regions.push_back(reference);
    document = manager.cam_operation_add(op);
    return document.cam.operations.back().op_id;
  };

  const std::string by_area_id = add_op("by area", "inner_first");
  const std::string nearest_id = add_op("nearest", "nearest_neighbor");

  const auto by_area = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), by_area_id, /*preview=*/false);
  const auto nearest = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), nearest_id, /*preview=*/false);
  if (!expect(by_area.result.ok && nearest.result.ok, "both orderings generate")) {
    return false;
  }

  const double area_travel = rapid_travel(by_area.result.toolpath);
  const double nearest_travel = rapid_travel(nearest.result.toolpath);
  std::cout << "  inner_first rapid travel: " << area_travel << " mm ("
            << (area_travel / layer_span) << " board spans)\n"
            << "  nearest_neighbor travel:  " << nearest_travel << " mm ("
            << (nearest_travel / layer_span) << " board spans)\n";

  // The layer is ~97 mm across.  What ships today: inner_first 462 board spans
  // of travel, nearest_neighbor 38.5 — so the guard below is "must stay at
  // least 5x better than area ordering, and must not regress materially from
  // the shipped figure".  It is a regression guard, NOT a target: the
  // optimisation work (2-opt refinement, start-point rotation) aims well under
  // these numbers.
  bool ok = expect(nearest_travel < area_travel * 0.2,
                   "nearest-neighbour is at least 5x better than area ordering");
  ok &= expect(nearest_travel < layer_span * 60.0,
               "and does not regress from the shipped 38.5 board spans (now " +
                   std::to_string(nearest_travel / layer_span) + ")");
  return ok;
}

bool test_centerline_mode_follows_the_drawn_line() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // A board profile is drawn with a hairline aperture: the file means "cut along
  // this line".  Regions mode traces both sides of the 0.1 mm ribbon the
  // aperture swept; centre-line mode follows the line itself, once, with no
  // kerf offset and no leads.
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
  const FeatureEntry* layer = find_layer(document);
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const PcbLayerAttestation attestation = attestation_for(*layer);
  const int assembled_rings = layer->pcb_layer_parameters->ring_count;
  const double layer_max_x = layer->pcb_layer_parameters->max_x;

  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  document = manager.cam_setup_create(setup);
  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  const auto add_op = [&](const std::string& mode) {
    CamOperation op;
    op.name = "PCB layer cut (" + mode + ")";
    op.type = "laser_cut";
    op.tool_id = document.cam.tool_library[0].tool_id;
    LaserCutParameters laser;
    laser.kerf_width_mm = 0.2;
    laser.lead_in_mm = 2.0;  // must be ignored in centre-line mode
    laser.lead_out_mm = 2.0;
    laser.layer_mode = mode;
    op.parameters.laser = laser;
    GeometryReference reference;
    reference.persistent_id = attestation.layer_feature_id;
    reference.attestation = attestation;
    op.geometry_references.machining_regions.push_back(reference);
    document = manager.cam_operation_add(op);
    return document.cam.operations.back().op_id;
  };

  const std::string regions_id = add_op("regions");
  const std::string centre_id = add_op("centerline");
  const auto regions = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), regions_id, /*preview=*/false);
  const auto centre = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), centre_id, /*preview=*/false);
  if (!expect(regions.result.ok && centre.result.ok,
              "both layer modes generate")) {
    std::cerr << "  regions: " << regions.result.error_message
              << "\n  centerline: " << centre.result.error_message << "\n";
    return false;
  }

  const auto count_contours = [](const Toolpath& toolpath) {
    int contours = 0;
    bool previous_on = false;
    for (const auto& move : toolpath.moves) {
      if (move.laser_on && !previous_on) {
        ++contours;
      }
      previous_on = move.laser_on;
    }
    return contours;
  };
  const int region_contours = count_contours(regions.result.toolpath);
  const int centre_contours = count_contours(centre.result.toolpath);
  std::cout << "  regions: " << region_contours << " contours (of "
            << assembled_rings << " assembled rings), centre-line: "
            << centre_contours << " contours\n";

  // Four closed outlines as centre-lines (board + 3 cut-outs) instead of the
  // eight boundaries of their ribbons.
  bool ok = expect(centre_contours == 4,
                   "centre-line mode cuts the 4 drawn outlines once each");
  ok &= expect(centre_contours < region_contours,
               "and cuts fewer contours than tracing both sides of the ribbon");

  bool any_laser = false;
  const double centre_max_x = laser_on_max_x(centre.result.toolpath, &any_laser);
  ok &= expect(any_laser, "centre-line mode fires the laser");
  // The discriminator: following the drawn line stays INSIDE the drawn extents
  // (half a hairline in), while a kerf-offset ribbon boundary goes outside.
  ok &= expect(centre_max_x < layer_max_x,
               "the cut follows the line, not the ribbon's outside edge");
  bool regions_laser = false;
  const double regions_max_x =
      laser_on_max_x(regions.result.toolpath, &regions_laser);
  ok &= expect(regions_laser && regions_max_x > layer_max_x,
               "while regions mode offsets outside by the kerf");
  return ok;
}

}  // namespace

int main() {
  // The generator registry is populated explicitly (static-lib link semantics
  // would drop a self-registering TU); the app does this once at startup.
  polysmith::core::register_builtin_cam_generators();

  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"layer_rings_cut_as_one_contour_each",
       test_layer_rings_cut_as_one_contour_each},
      {"drill_only_layer_reports_rather_than_cuts_nothing",
       test_drill_only_layer_reports_rather_than_cuts_nothing},
      {"missing_layer_source_degrades_with_a_reason",
       test_missing_layer_source_degrades_with_a_reason},
      {"cut_order_reduces_travel", test_cut_order_reduces_travel},
      {"centerline_mode_follows_the_drawn_line",
       test_centerline_mode_follows_the_drawn_line},
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
  std::cout << "pcb layer cut: all cases passed\n";
  return 0;
}
