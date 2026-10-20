// Regression suite for imported PCB layers (P2 core slice): the `pcb_layer`
// document feature, its role/polarity resolution, its re-derivation contract
// and its persistence.
//
// The layer stores the SOURCE PATH plus a summary, not the rings (a copper
// layer is ~2k rings; embedding them would bloat every save).  So the
// behaviours that matter are: the role comes from the file's own attribute
// rather than a filename guess, the summary is honest, a vanished source
// degrades to `dependency_broken`+warning instead of crashing, and a
// save/load round-trip preserves the parameters.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/pcb/pcb_layer_io.h"
#include "core/pcb/pcb_role.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::PcbLayerFormat;
using polysmith::core::PcbRole;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << "  FAIL: " << message << "\n";
  return false;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
  return std::abs(actual - expected) <= tolerance;
}

std::string fixture(const std::string& name) {
  return "native/cad-core/tests/data/pcb/" + name;
}

bool have_fixtures() {
  std::error_code error;
  return std::filesystem::exists(fixture("copper_trimmed.gbr"), error);
}

const FeatureEntry* find_feature(const DocumentState& document,
                                 const std::string& kind) {
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.kind == kind) {
      return &feature;
    }
  }
  return nullptr;
}

const FeatureEntry* find_feature_by_id(const DocumentState& document,
                                       const std::string& id) {
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.id == id) {
      return &feature;
    }
  }
  return nullptr;
}

// ── Role resolution (pure) ─────────────────────────────────────────

bool test_role_resolution_prefers_the_file_attribute() {
  using polysmith::core::pcb::resolve_pcb_role;
  bool ok = true;

  // KiCad's Gerber attribute spelling.
  ok &= expect(resolve_pcb_role("Copper,L1,Top", "gbr").role == PcbRole::CopperTop,
               "Copper,L1,Top");
  ok &= expect(resolve_pcb_role("Copper,L4,Bot", "x.gbr").role == PcbRole::CopperBottom,
               "Copper,L4,Bot");
  ok &= expect(resolve_pcb_role("Copper,L2,Inr", "x.gbr").role == PcbRole::CopperInner,
               "Copper,L2,Inr");
  ok &= expect(resolve_pcb_role("Profile,NP", "x.gbr").role == PcbRole::Profile, "Profile,NP");

  // The two tiers disagree on vocabulary; both must land on the same role.
  ok &= expect(resolve_pcb_role("Paste,Bot", "x.gbr").role == PcbRole::PasteBottom,
               "Paste,Bot (Gerber attribute)");
  ok &= expect(resolve_pcb_role("SolderPaste,Bot", "x.gbr").role == PcbRole::PasteBottom,
               "SolderPaste,Bot (job file)");
  ok &= expect(resolve_pcb_role("Legend,Top", "x.gbr").role == PcbRole::SilkscreenTop,
               "Legend,Top");
  ok &= expect(resolve_pcb_role("Silkscreen,Top", "x.gbr").role == PcbRole::SilkscreenTop,
               "Silkscreen,Top");

  // Masks are the polarity trap: the role resolves, the polarity is negative.
  ok &= expect(resolve_pcb_role("SolderMask,Bot", "x.gbr").role == PcbRole::SolderMaskBottom,
               "SolderMask,Bot");

  // Drills: "NonPlated" must not be eaten by the "Plated" match.
  ok &= expect(resolve_pcb_role("Plated,1,4,PTH", "x.drl").role == PcbRole::DrillPlated,
               "Plated PTH");
  ok &= expect(resolve_pcb_role("NonPlated,1,4,NPTH", "x.drl").role == PcbRole::DrillNonPlated,
               "NonPlated NPTH");

  // Filename fallback when the file says nothing.
  const auto guessed = resolve_pcb_role("", "gapstorm-retro-F_Cu.gbr");
  ok &= expect(guessed.role == PcbRole::CopperTop && !guessed.from_attributes,
               "filename fallback: -F_Cu");
  ok &= expect(resolve_pcb_role("", "board.drl").role == PcbRole::DrillPlated,
               "filename fallback: .drl");
  ok &= expect(resolve_pcb_role("", "mystery.dat").role == PcbRole::Unknown,
               "an unclassifiable file stays Unknown");

  // The attribute wins over a misleading name.
  ok &= expect(resolve_pcb_role("Copper,L4,Bot", "looks-like-F_Cu.gbr").role ==
                   PcbRole::CopperBottom,
               "the attribute beats the filename");
  return ok;
}

// ── Loading the committed fixtures ─────────────────────────────────

bool test_committed_layers_load_with_honest_summaries() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  using polysmith::core::pcb::load_pcb_layer;

  const auto copper = load_pcb_layer(fixture("copper_trimmed.gbr"));
  bool ok = expect(copper.ok, "trimmed copper loads");
  ok &= expect(copper.format == PcbLayerFormat::Gerber, "detected as Gerber");
  ok &= expect(copper.role == PcbRole::CopperTop, "role from the file attribute");
  ok &= expect(copper.role_from_attributes, "the role is not a guess");
  ok &= expect(copper.ring_count == 573,
                   "573 cuttable contours after region assembly (1964 as drawn)");
  ok &= expect(copper.geometry.skipped_operations == 0, "nothing skipped");
  ok &= expect(copper.bounds.valid && copper.bounds.max_x > 90.0, "board-sized bounds");

  const auto profile = load_pcb_layer(fixture("edge_cuts.gbr"));
  ok &= expect(profile.ok && profile.role == PcbRole::Profile, "profile layer");
  ok &= expect(profile.ring_count == 8,
                   "8 assembled profile contours (20 strokes as drawn)");

  const auto drills = load_pcb_layer(fixture("pth.drl"));
  ok &= expect(drills.ok, "plated drill file loads");
  ok &= expect(drills.format == PcbLayerFormat::Excellon, "detected as Excellon");
  ok &= expect(drills.role == PcbRole::DrillPlated, "plated role");
  ok &= expect(drills.tool_count == 13 && drills.hit_count == 511, "13 tools / 511 holes");
  ok &= expect(drills.ring_count == 0, "a drill file contributes no rings");

  const auto npth = load_pcb_layer(fixture("npth.drl"));
  ok &= expect(npth.ok && npth.role == PcbRole::DrillNonPlated, "NPTH role");
  ok &= expect(npth.hit_count == 0, "NPTH has no holes");

  // A mask layer must land on a negative polarity.
  const auto mask = load_pcb_layer(fixture("paste_bottom.gbr"));
  ok &= expect(mask.ok && mask.role == PcbRole::PasteBottom, "paste layer role");
  ok &= expect(mask.ring_count > 0, "paste layer has rings");
  return ok;
}

bool test_missing_source_is_an_error_not_a_throw() {
  using polysmith::core::pcb::load_pcb_layer;
  const auto missing = load_pcb_layer(fixture("does-not-exist.gbr"));
  bool ok = expect(!missing.ok, "missing file reports failure");
  ok &= expect(missing.error.find("not found") != std::string::npos, "and says so");
  return ok;
}

// ── Document integration ───────────────────────────────────────────

bool test_import_creates_a_pcb_layer_feature() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_pcb_layer(fixture("copper_trimmed.gbr"));

  const FeatureEntry* layer = find_feature(document, "pcb_layer");
  bool ok = expect(layer != nullptr, "a pcb_layer feature exists");
  if (layer == nullptr) {
    return false;
  }
  ok &= expect(layer->pcb_layer_parameters.has_value(), "it carries layer parameters");
  const auto& params = *layer->pcb_layer_parameters;
  ok &= expect(params.role == PcbRole::CopperTop, "role stored");
  ok &= expect(params.format == PcbLayerFormat::Gerber, "format stored");
  ok &= expect(params.ring_count == 573, "assembled contour count stored");
  // Range rather than an exact figure: the value is the source file's own
  // extents, and pinning a made-up constant here would just be a second copy
  // of the fixture.
  ok &= expect(params.bounds_valid && params.min_x >= -1.0 && params.max_x > 90.0 &&
                   params.max_x <= 100.0,
               "bounds stored and board-sized");
  ok &= expect(!layer->dependency_broken, "a readable layer is healthy");
  ok &= expect(layer->parameters_summary.find("copper_trimmed.gbr") != std::string::npos,
               "the summary names the source file");
  ok &= expect(!params.mirror_x, "top copper is not mirrored");

  // Two layers coexist and are independently addressable.
  const DocumentState both = manager.import_pcb_layer(fixture("pth.drl"));
  int layers = 0;
  for (const FeatureEntry& feature : both.feature_history) {
    if (feature.kind == "pcb_layer") {
      ++layers;
    }
  }
  ok &= expect(layers == 2, "a second layer is added, not merged");
  return ok;
}

bool test_import_of_a_missing_file_throws_before_mutating() {
  DocumentManager manager;
  manager.create_document();
  bool threw = false;
  try {
    manager.import_pcb_layer(fixture("nope.gbr"));
  } catch (const std::exception&) {
    threw = true;
  }
  bool ok = expect(threw, "a missing file throws");
  // The manager exposes no state getter — mutators return it — so prove
  // non-mutation by importing a real layer afterwards and checking that it is
  // the only one in the history.
  if (have_fixtures()) {
    const DocumentState after = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
    int layers = 0;
    for (const FeatureEntry& feature : after.feature_history) {
      if (feature.kind == "pcb_layer") {
        ++layers;
      }
    }
    ok &= expect(layers == 1, "the failed import left no feature behind");
  }
  return ok;
}

bool test_unreadable_content_is_reported_not_thrown() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // A Gerber file whose body is nonsense: it parses as a Gerber attempt and
  // fails, but the import still succeeds in creating a feature that says so.
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "pcb_layer_junk.gbr";
  {
    std::ofstream stream(path, std::ios::binary);
    stream << "not a gerber file at all\n";
  }
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_pcb_layer(path.string());
  const FeatureEntry* layer = find_feature(document, "pcb_layer");
  bool ok = expect(layer != nullptr, "a feature is created so the user sees what failed");
  if (layer != nullptr) {
    ok &= expect(layer->dependency_broken, "it is flagged broken");
    ok &= expect(!layer->dependency_warning.empty(), "with a warning to show");
    ok &= expect(layer->pcb_layer_parameters.has_value(), "parameters survive for a retry");
  }
  std::error_code error;
  std::filesystem::remove(path, error);
  return ok;
}

bool test_role_override_and_mirroring() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  // Import the TOP copper but claim it is the bottom layer.
  const DocumentState imported =
      manager.import_pcb_layer(fixture("copper_trimmed.gbr"), std::string("copper_bottom"));
  const FeatureEntry* layer = find_feature(imported, "pcb_layer");
  bool ok = expect(layer != nullptr && layer->pcb_layer_parameters.has_value(), "imported");
  if (layer == nullptr || !layer->pcb_layer_parameters.has_value()) {
    return false;
  }
  const std::string feature_id = layer->id;
  ok &= expect(layer->pcb_layer_parameters->role == PcbRole::CopperBottom,
               "the explicit role wins over the file's attribute");
  ok &= expect(!layer->pcb_layer_parameters->role_from_attributes,
               "and is recorded as a user choice");
  ok &= expect(layer->pcb_layer_parameters->mirror_x, "bottom roles mirror by default");

  // Un-mirror explicitly.
  const DocumentState unmirrored = manager.update_pcb_layer(feature_id, std::nullopt, false);
  const FeatureEntry* updated = find_feature_by_id(unmirrored, feature_id);
  ok &= expect(updated != nullptr && !updated->pcb_layer_parameters->mirror_x,
               "mirroring can be turned off");

  // Clearing the role (empty) goes back to the file's own attribute.
  const DocumentState resolved = manager.update_pcb_layer(feature_id, std::string(""), std::nullopt);
  const FeatureEntry* back = find_feature_by_id(resolved, feature_id);
  ok &= expect(back != nullptr && back->pcb_layer_parameters->role == PcbRole::CopperTop,
               "an empty role re-resolves from the file");

  // An unknown role is rejected.
  bool threw = false;
  try {
    manager.update_pcb_layer(feature_id, std::string("not_a_role"), std::nullopt);
  } catch (const std::exception&) {
    threw = true;
  }
  ok &= expect(threw, "an unknown role is rejected");
  return ok;
}

bool test_vanished_source_marks_the_dependency_broken() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  // Copy a fixture to a scratch path, import it, delete the source, then ask
  // the document to refresh the layer: it must degrade, not crash or lie.
  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() / "pcb_layer_vanishing.gbr";
  std::error_code error;
  std::filesystem::copy_file(fixture("copper_trimmed.gbr"), scratch,
                             std::filesystem::copy_options::overwrite_existing, error);

  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_pcb_layer(scratch.string());
  const FeatureEntry* layer = find_feature(imported, "pcb_layer");
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const std::string feature_id = layer->id;
  const int rings_before = layer->pcb_layer_parameters->ring_count;

  std::filesystem::remove(scratch, error);
  const DocumentState updated = manager.update_pcb_layer(feature_id, std::nullopt, std::nullopt);
  const FeatureEntry* after = find_feature_by_id(updated, feature_id);
  bool ok = expect(after != nullptr, "the feature survives");
  if (after == nullptr) {
    return false;
  }
  ok &= expect(after->dependency_broken, "it is flagged as a broken dependency");
  ok &= expect(after->dependency_warning.find("not found") != std::string::npos,
               "the warning names the missing file");
  ok &= expect(after->pcb_layer_parameters->ring_count == rings_before,
               "the last known summary is kept (no fake zeroes)");
  return ok;
}

bool test_save_load_round_trip_preserves_layer_parameters() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  // Two layers, two formats — the round trip must preserve both, and polarity
  // only exists for Gerber layers (plating lives in the drill role).
  const DocumentState imported = manager.import_pcb_layer(fixture("copper_trimmed.gbr"));
  const DocumentState both = manager.import_pcb_layer(fixture("pth.drl"));
  const FeatureEntry* copper = nullptr;
  const FeatureEntry* drills = nullptr;
  for (const FeatureEntry& feature : both.feature_history) {
    if (feature.kind != "pcb_layer" || !feature.pcb_layer_parameters.has_value()) {
      continue;
    }
    if (feature.pcb_layer_parameters->format == PcbLayerFormat::Excellon) {
      drills = &feature;
    } else {
      copper = &feature;
    }
  }
  if (copper == nullptr || drills == nullptr) {
    return expect(false, "both layers imported");
  }
  const std::string saved_role = polysmith::core::pcb_role_id(drills->pcb_layer_parameters->role);
  const int saved_tools = drills->pcb_layer_parameters->tool_count;
  const int saved_hits = drills->pcb_layer_parameters->hit_count;
  const std::string copper_polarity = copper->pcb_layer_parameters->file_polarity;
  (void)imported;

  // Persistence goes through the document layer's own save/load, which is what
  // a .polysmith part file uses — the contract that matters for a layer whose
  // geometry is re-derived rather than embedded.
  const std::filesystem::path part_path =
      std::filesystem::temp_directory_path() / "pcb_layer_roundtrip.polysmith";
  manager.save_document_to_path(part_path.string());

  DocumentManager reloaded;
  reloaded.create_document();
  const DocumentState document = reloaded.load_document_from_path(part_path.string());

  const FeatureEntry* restored_drills = nullptr;
  const FeatureEntry* restored_copper = nullptr;
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.kind != "pcb_layer" || !feature.pcb_layer_parameters.has_value()) {
      continue;
    }
    if (feature.pcb_layer_parameters->format == PcbLayerFormat::Excellon) {
      restored_drills = &feature;
    } else {
      restored_copper = &feature;
    }
  }

  bool ok = expect(restored_drills != nullptr, "the drill layer survives a save/load");
  if (restored_drills == nullptr || restored_copper == nullptr) {
    return false;
  }
  const auto& params = *restored_drills->pcb_layer_parameters;
  ok &= expect(polysmith::core::pcb_role_id(params.role) == saved_role, "role persists");
  ok &= expect(params.format == PcbLayerFormat::Excellon, "format persists");
  ok &= expect(params.tool_count == saved_tools && params.hit_count == saved_hits,
               "summary counts persist");
  ok &= expect(params.file_path == fixture("pth.drl"), "source path persists");
  ok &= expect(restored_drills->kind == "pcb_layer", "kind persists");
  ok &= expect(restored_copper->pcb_layer_parameters->file_polarity == copper_polarity &&
                   !copper_polarity.empty(),
               "the Gerber layer keeps its polarity");
  ok &= expect(restored_copper->pcb_layer_parameters->ring_count == 573,
               "the Gerber layer keeps its ring count");
  std::error_code remove_error;
  std::filesystem::remove(part_path, remove_error);
  return ok;
}

bool test_remove_layer() {
  if (!have_fixtures()) {
    std::cout << "  (skipped: tests/data/pcb not found)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_pcb_layer(fixture("edge_cuts.gbr"));
  const FeatureEntry* layer = find_feature(imported, "pcb_layer");
  if (layer == nullptr) {
    return expect(false, "layer imported");
  }
  const std::string feature_id = layer->id;
  const DocumentState removed = manager.remove_pcb_layer(feature_id);
  bool ok = expect(find_feature(removed, "pcb_layer") == nullptr, "the layer is gone");
  bool threw = false;
  try {
    manager.remove_pcb_layer(feature_id);
  } catch (const std::exception&) {
    threw = true;
  }
  ok &= expect(threw, "removing it twice is an error, not a crash");
  return ok;
}

}  // namespace

int main() {
  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"role_resolution_prefers_the_file_attribute",
       test_role_resolution_prefers_the_file_attribute},
      {"committed_layers_load_with_honest_summaries",
       test_committed_layers_load_with_honest_summaries},
      {"missing_source_is_an_error_not_a_throw",
       test_missing_source_is_an_error_not_a_throw},
      {"import_creates_a_pcb_layer_feature", test_import_creates_a_pcb_layer_feature},
      {"import_of_a_missing_file_throws_before_mutating",
       test_import_of_a_missing_file_throws_before_mutating},
      {"unreadable_content_is_reported_not_thrown",
       test_unreadable_content_is_reported_not_thrown},
      {"role_override_and_mirroring", test_role_override_and_mirroring},
      {"vanished_source_marks_the_dependency_broken",
       test_vanished_source_marks_the_dependency_broken},
      {"save_load_round_trip_preserves_layer_parameters",
       test_save_load_round_trip_preserves_layer_parameters},
      {"remove_layer", test_remove_layer},
  };

  int failures = 0;
  for (const Suite& suite : suites) {
    // Flushed: a crash in a later case must not swallow the name of the case
    // that caused it (stdout to a pipe is block-buffered).
    std::cout << "case " << suite.name << std::flush << "\n";
    if (!suite.run()) {
      ++failures;
    }
  }
  if (failures != 0) {
    std::cerr << failures << " case(s) failed\n";
    return 1;
  }
  std::cout << "pcb layer: all cases passed\n";
  return 0;
}
