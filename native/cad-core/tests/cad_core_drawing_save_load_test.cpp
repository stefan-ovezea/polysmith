// Drawing save/load round-trip test.
//
// Verifies the "drawing" key of the document payload survives a full
// save → load cycle: drawings, sheets, views, annotations (including
// source edge witnesses with the circle witness, section definitions,
// title block data, annotation extensions), and the decimal
// separator.  Also proves:
//   - id counters restore on load (no colliding ids after reload)
//   - the drawing mutators follow the canonical shape (validate
//     before mutate, mint ids, cascade sheet/view/annotation deletes,
//     clear dead selection ids)
//   - generated projections stay memory-only: the runtime cache is
//     invalidated by undo/redo, never serialized.

#include <filesystem>
#include <fstream>
#include <iostream>

#include "core/document/document.h"
#include "core/drawing/drawing_runtime.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::Annotation;
using polysmith::core::AnnotationExtension;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingDocumentData;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::ProjectionResult;
using polysmith::core::SectionDefinition;
using polysmith::core::SourceEdgeWitness;
using polysmith::core::TitleBlock;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-9) {
  return std::abs(a - b) < tolerance;
}

SourceEdgeWitness make_witness(const std::string& body_id) {
  SourceEdgeWitness witness;
  witness.body_id = body_id;
  witness.src_edge_index = 4;
  witness.start_point = {10.0, 5.0, 10.0};
  witness.end_point = {10.0, 5.0, 10.0};  // closed rim: start == end
  witness.length = 12.566370614359172;
  witness.tangent = {1.0, 0.0, 0.0};
  witness.curve_kind = "circle";
  witness.center = std::array<double, 3>{10.0, 5.0, 10.0};
  witness.axis = std::array<double, 3>{0.0, 0.0, 1.0};
  witness.radius = 2.0;
  witness.param_range = {0.0, 6.283185307179586};
  return witness;
}

bool witness_equal(const SourceEdgeWitness& a, const SourceEdgeWitness& b) {
  if (a.body_id != b.body_id || a.src_edge_index != b.src_edge_index ||
      a.start_point != b.start_point || a.end_point != b.end_point ||
      !near(a.length, b.length) || a.tangent != b.tangent ||
      a.curve_kind != b.curve_kind || a.param_range != b.param_range) {
    return false;
  }
  if (a.center.has_value() != b.center.has_value() ||
      a.axis.has_value() != b.axis.has_value() ||
      a.radius.has_value() != b.radius.has_value()) {
    return false;
  }
  if (a.center.has_value() && a.center.value() != b.center.value()) {
    return false;
  }
  if (a.axis.has_value() && a.axis.value() != b.axis.value()) {
    return false;
  }
  if (a.radius.has_value() && !near(a.radius.value(), b.radius.value())) {
    return false;
  }
  return true;
}

Drawing make_drawing() {
  Drawing drawing;
  drawing.drawing_id = "drawing-1";
  drawing.name = "Bracket";

  DrawingSheet sheet;
  sheet.sheet_id = "drawing-sheet-1";
  sheet.name = "Sheet 1";
  sheet.paper_size = "A4";
  sheet.orientation = "portrait";
  sheet.projection_angle = "first_angle";
  sheet.view_ids = {"drawing-view-1"};
  sheet.title_block.legal_owner = "Acme Works";
  sheet.title_block.identification = "ACME-042";
  sheet.title_block.date = "2026-09-19";
  sheet.title_block.title = "Mounting Bracket";
  sheet.title_block.approver = "J. Doe";
  sheet.title_block.creator = "A. Smith";
  sheet.title_block.document_type = "Part Drawing";
  sheet.title_block.revision_rows = {
      {"A1", "A", "Initial release", "2026-09-01", "JD"},
      {"B2", "B", "Hole enlarged", "2026-09-19", "JD"},
  };
  drawing.sheets.push_back(sheet);

  DrawingView view;
  view.view_id = "drawing-view-1";
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {"body-1"};
  view.scale = 0.5;
  view.sheet_position = {30.0, 40.0};
  view.show_hidden = true;
  drawing.views.push_back(view);

  DrawingView section_view;
  section_view.view_id = "drawing-view-2";
  section_view.kind = "section";
  section_view.source_body_ids = {"body-1"};
  section_view.scale = 1.0;
  section_view.sheet_position = {120.0, 40.0};
  SectionDefinition section;
  section.cutting_plane_point = {5.0, 0.0, 0.0};
  section.cutting_plane_normal = {1.0, 0.0, 0.0};
  section.cut_away = false;
  section.label = "A";
  section.hatch_angle_deg = 30.0;
  section.hatch_spacing_mm = 2.5;
  section_view.section = section;
  drawing.views.push_back(section_view);

  Annotation annotation;
  annotation.annotation_id = "drawing-annotation-1";
  annotation.kind = "diameter";
  annotation.view_id = "drawing-view-1";
  annotation.source_edge_id = "drawing-edge-1";
  annotation.witness = make_witness("body-1");
  annotation.prefix = "⌀";
  AnnotationExtension ext;
  ext.kind = "ted";
  ext.fields = {{"boxed", "true"}};
  annotation.extensions.push_back(ext);
  annotation.text_offset = std::array<double, 2>{2.0, -3.0};
  annotation.arrow_flip = true;
  drawing.annotations.push_back(annotation);

  return drawing;
}

bool title_block_equal(const TitleBlock& a, const TitleBlock& b) {
  return a.legal_owner == b.legal_owner &&
         a.identification == b.identification && a.date == b.date &&
         a.title == b.title && a.approver == b.approver &&
         a.creator == b.creator && a.document_type == b.document_type &&
         a.revision_rows == b.revision_rows;
}

bool section_equal(const SectionDefinition& a, const SectionDefinition& b) {
  return a.cutting_plane_point == b.cutting_plane_point &&
         a.cutting_plane_normal == b.cutting_plane_normal &&
         a.cut_away == b.cut_away && a.label == b.label &&
         near(a.hatch_angle_deg, b.hatch_angle_deg) &&
         near(a.hatch_spacing_mm, b.hatch_spacing_mm) &&
         a.construction_plane_id == b.construction_plane_id;
}

bool view_equal(const DrawingView& a, const DrawingView& b) {
  if (a.view_id != b.view_id || a.kind != b.kind ||
      a.standard_view != b.standard_view ||
      a.source_body_ids != b.source_body_ids || !near(a.scale, b.scale) ||
      a.sheet_position != b.sheet_position ||
      a.show_hidden != b.show_hidden || a.warning != b.warning) {
    return false;
  }
  if (a.custom_frame.has_value() != b.custom_frame.has_value()) {
    return false;
  }
  if (a.custom_frame.has_value() &&
      (a.custom_frame->origin != b.custom_frame->origin ||
       a.custom_frame->normal != b.custom_frame->normal ||
       a.custom_frame->x_direction != b.custom_frame->x_direction)) {
    return false;
  }
  if (a.section.has_value() != b.section.has_value()) {
    return false;
  }
  if (a.section.has_value() && !section_equal(*a.section, *b.section)) {
    return false;
  }
  return a.broken_ref == b.broken_ref;
}

bool annotation_equal(const Annotation& a, const Annotation& b) {
  if (a.annotation_id != b.annotation_id || a.kind != b.kind ||
      a.view_id != b.view_id || a.source_edge_id != b.source_edge_id ||
      !witness_equal(a.witness, b.witness) || a.prefix != b.prefix ||
      a.dependency_broken != b.dependency_broken || a.warning != b.warning ||
      a.arrow_flip != b.arrow_flip) {
    return false;
  }
  if (a.witness_2.has_value() != b.witness_2.has_value() ||
      (a.witness_2.has_value() &&
       !witness_equal(*a.witness_2, *b.witness_2))) {
    return false;
  }
  if (a.text_override != b.text_override || a.text_offset != b.text_offset) {
    return false;
  }
  if (a.extensions.size() != b.extensions.size()) {
    return false;
  }
  for (size_t i = 0; i < a.extensions.size(); ++i) {
    if (a.extensions[i].kind != b.extensions[i].kind ||
        a.extensions[i].fields != b.extensions[i].fields) {
      return false;
    }
  }
  return true;
}

bool drawing_data_equal(const DrawingDocumentData& a,
                        const DrawingDocumentData& b) {
  if (a.active_drawing_id != b.active_drawing_id ||
      a.selected_view_id != b.selected_view_id ||
      a.selected_annotation_id != b.selected_annotation_id ||
      a.decimal_separator != b.decimal_separator ||
      a.drawings.size() != b.drawings.size()) {
    return false;
  }
  for (size_t d = 0; d < a.drawings.size(); ++d) {
    const auto& da = a.drawings[d];
    const auto& db = b.drawings[d];
    if (da.drawing_id != db.drawing_id || da.name != db.name ||
        da.sheets.size() != db.sheets.size() ||
        da.views.size() != db.views.size() ||
        da.annotations.size() != db.annotations.size()) {
      return false;
    }
    for (size_t s = 0; s < da.sheets.size(); ++s) {
      const auto& sa = da.sheets[s];
      const auto& sb = db.sheets[s];
      if (sa.sheet_id != sb.sheet_id || sa.name != sb.name ||
          sa.paper_size != sb.paper_size ||
          sa.orientation != sb.orientation ||
          sa.projection_angle != sb.projection_angle ||
          sa.view_ids != sb.view_ids ||
          !title_block_equal(sa.title_block, sb.title_block)) {
        return false;
      }
    }
    for (size_t v = 0; v < da.views.size(); ++v) {
      if (!view_equal(da.views[v], db.views[v])) {
        return false;
      }
    }
    for (size_t i = 0; i < da.annotations.size(); ++i) {
      if (!annotation_equal(da.annotations[i], db.annotations[i])) {
        return false;
      }
    }
  }
  return true;
}

bool test_document_round_trip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();
  document.drawing.drawings.push_back(make_drawing());
  document.drawing.active_drawing_id = "drawing-1";
  document.drawing.selected_view_id = "drawing-view-1";
  document.drawing.selected_annotation_id = "drawing-annotation-1";
  document.drawing.decimal_separator = ".";

  const auto payload = polysmith::protocol::to_payload(document, true);
  const auto drawing_payload = payload.at("drawing");

  // Generated projections are memory-only by design: the drawing
  // payload carries definitions, never projection output keys.
  if (!expect(!drawing_payload.contains("projections") &&
                  !drawing_payload.contains("projected_edges") &&
                  !drawing_payload.contains("hatch_regions"),
              "payload must not contain projection output")) {
    return false;
  }

  const auto restored = polysmith::protocol::document_from_payload(payload);
  return expect(drawing_data_equal(document.drawing, restored.drawing),
                "drawing data must survive document serialize/deserialize");
}

bool test_file_round_trip_and_counter_restore() {
  // Build the document through the mutators so the id counters are
  // exercised, save, reload in a fresh manager, then prove the
  // restored counters mint non-colliding ids.
  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_drawing_save_load_test.json";

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.drawing_create(make_drawing());
  {
    std::ofstream stream(path.string());
    stream << polysmith::protocol::to_payload(document, true).dump(2);
  }

  DocumentManager loaded;
  loaded.create_document();
  const DocumentState restored = loaded.load_document_from_path(path.string());

  if (!expect(restored.drawing.drawings.size() == 1,
              "file load: drawing restored")) {
    return false;
  }
  const auto& drawing = restored.drawing.drawings[0];
  if (!expect(drawing.sheets.size() == 1 && drawing.views.size() == 2 &&
                  drawing.annotations.size() == 1 &&
                  drawing.views[1].section.has_value() &&
                  drawing.views[1].section->label == "A" &&
                  !drawing.views[1].section->cut_away &&
                  near(drawing.views[1].section->hatch_angle_deg, 30.0) &&
                  drawing.annotations[0].prefix == "⌀" &&
                  drawing.annotations[0].extensions.size() == 1 &&
                  drawing.annotations[0].extensions[0].kind == "ted",
              "file load: drawing contents survive")) {
    return false;
  }

  // The restored counters: a new drawing must not collide with
  // "drawing-1" / "drawing-sheet-1" / "drawing-view-2" /
  // "drawing-annotation-1" from the loaded document.
  Drawing second;
  second.name = "Second";
  DrawingSheet second_sheet;
  second_sheet.name = "Sheet 1";
  second.sheets.push_back(second_sheet);
  const DocumentState after_create = loaded.drawing_create(second);
  const auto& created = after_create.drawing.drawings.back();
  if (!expect(created.drawing_id == "drawing-2",
              "counter restore: new drawing id must not collide")) {
    return false;
  }
  const DocumentState after_sheet = loaded.drawing_sheet_create(
      "drawing-2", DrawingSheet{});
  const auto& appended_sheet = after_sheet.drawing.drawings.back().sheets.back();
  // drawing_create minted "drawing-sheet-2" for the second drawing's
  // own sheet, so the explicit sheet_create continues at 3 — never
  // colliding with the loaded "drawing-sheet-1".
  return expect(appended_sheet.sheet_id == "drawing-sheet-3",
                "counter restore: new sheet id must not collide");
}

bool test_mutator_shape() {
  DocumentManager manager;
  manager.create_document();

  // drawing_create mints ids for empty ones and sets the active id.
  Drawing drawing = make_drawing();
  drawing.drawing_id.clear();
  drawing.sheets[0].sheet_id.clear();
  drawing.views[0].view_id.clear();
  drawing.views[1].view_id.clear();
  drawing.annotations[0].annotation_id.clear();
  const DocumentState created = manager.drawing_create(drawing);
  const auto& stored = created.drawing.drawings[0];
  if (!expect(stored.drawing_id == "drawing-1" &&
                  stored.sheets[0].sheet_id == "drawing-sheet-1" &&
                  stored.views[0].view_id == "drawing-view-1" &&
                  stored.views[1].view_id == "drawing-view-2" &&
                  stored.annotations[0].annotation_id ==
                      "drawing-annotation-1" &&
                  created.drawing.active_drawing_id == "drawing-1",
              "drawing_create mints ids and sets the active drawing")) {
    return false;
  }

  // Validation throws BEFORE the undo push: the unknown view must be
  // rejected and leave no undo step behind (drawing_create already
  // pushed exactly one).
  DrawingSheet bad_sheet;
  bad_sheet.name = "Bad";
  bad_sheet.view_ids = {"drawing-view-404"};
  bool threw = false;
  try {
    manager.drawing_sheet_create("drawing-1", bad_sheet);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!expect(threw &&
                  manager.get_document()->undo_step_names.size() == 1,
              "sheet_create with unknown view throws before the undo push")) {
    return false;
  }

  // drawing_set_active validates the id.
  threw = false;
  try {
    manager.drawing_set_active("drawing-404");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!expect(threw, "set_active rejects unknown drawing ids")) {
    return false;
  }

  // sheet_create appends to the existing drawing.
  DrawingSheet extra;
  extra.name = "Sheet 2";
  extra.view_ids = {"drawing-view-2"};
  const DocumentState with_sheet =
      manager.drawing_sheet_create("drawing-1", extra);
  if (!expect(with_sheet.drawing.drawings[0].sheets.size() == 2 &&
                  with_sheet.drawing.drawings[0].sheets[1].sheet_id ==
                      "drawing-sheet-2",
              "sheet_create appends and mints the sheet id")) {
    return false;
  }

  // sheet_delete cascades: the sheet, its views, and annotations on
  // those views die together.  The annotation rides drawing-view-1
  // (sheet 1), so deleting sheet 2 removes only the section view.
  const DocumentState after_delete =
      manager.drawing_sheet_delete("drawing-1", "drawing-sheet-2");
  const auto& after = after_delete.drawing.drawings[0];
  if (!expect(after.sheets.size() == 1 && after.views.size() == 1 &&
                  after.annotations.size() == 1 &&
                  after.views[0].view_id == "drawing-view-1",
              "sheet_delete cascades to views and annotations")) {
    return false;
  }

  // drawing_delete removes the drawing and clears the active id
  // (drawing_create set it).  Selected view/annotation id clearing is
  // exercised once selection mutators land (P2+).
  const DocumentState after_drawing_delete =
      manager.drawing_delete("drawing-1");
  return expect(after_drawing_delete.drawing.drawings.empty() &&
                    !after_drawing_delete.drawing.active_drawing_id.has_value(),
                "drawing_delete clears the active drawing id");
}

bool test_undo_redo_invalidate_runtime_cache() {
  DocumentManager manager;
  manager.create_document();

  // A drawing whose view ids we know: drawing_view-1 is minted.
  Drawing drawing = make_drawing();
  drawing.drawing_id.clear();
  drawing.sheets[0].sheet_id.clear();
  drawing.views[0].view_id.clear();
  drawing.views[1].view_id.clear();
  drawing.annotations[0].annotation_id.clear();
  DocumentState document = manager.drawing_create(drawing);
  const std::string doc_id = document.id;
  const int pre_undo_revision = document.revision;

  // Seed the runtime cache as the projection engine does: one entry
  // per view, stamped with the current revision.
  ProjectionResult result;
  result.source_revision = document.revision;
  polysmith::core::drawing_runtime::store_projection(
      document, "drawing-view-1", result);
  if (!expect(polysmith::core::drawing_runtime::cached_projection(
                  document, "drawing-view-1") != nullptr,
              "runtime cache stores and reads back")) {
    return false;
  }

  // A mutation pushes an undo snapshot, then undo() invalidates the
  // cache BEFORE the restore's refresh pass re-runs (branch switch —
  // a revision stamp alone cannot distinguish abandoned-branch
  // results).  The refresh itself then legitimately re-stores fresh
  // entries stamped with the restored revision (the synthetic
  // document's view degrades stale — its body does not exist), so
  // the assertions pin WHAT invalidate guarantees: no entry stamped
  // with the abandoned branch's revision, and no last-known residue
  // (drop_stale moves pruned entries into last_known; only
  // invalidate() clears it).
  manager.drawing_set_active("drawing-1");
  DocumentState undone = manager.undo();
  auto& per_doc =
      polysmith::core::drawing_runtime::document_state(doc_id);
  if (!expect(polysmith::core::drawing_runtime::cached_projection_at(
                  undone, "drawing-view-1", pre_undo_revision) == nullptr,
              "undo leaves no entry stamped with the abandoned "
              "revision")) {
    return false;
  }
  if (!expect(per_doc.last_known.empty(),
              "undo clears last-known state")) {
    return false;
  }
  const auto entry = per_doc.projections.find("drawing-view-1");
  if (!expect(entry == per_doc.projections.end() ||
                  entry->second.revision == undone.revision,
              "post-undo entries carry the restored revision only")) {
    return false;
  }

  // Re-seed and redo: the same contract in the other direction.
  polysmith::core::drawing_runtime::store_projection(
      undone, "drawing-view-1", result);
  if (!expect(polysmith::core::drawing_runtime::cached_projection(
                  undone, "drawing-view-1") != nullptr,
              "runtime cache re-seeds after undo")) {
    return false;
  }
  const int pre_redo_revision = undone.revision;
  DocumentState redone = manager.redo();
  auto& per_doc_redo =
      polysmith::core::drawing_runtime::document_state(doc_id);
  return expect(polysmith::core::drawing_runtime::cached_projection_at(
                    redone, "drawing-view-1", pre_redo_revision) ==
                        nullptr &&
                    per_doc_redo.last_known.empty(),
                "redo clears the projection runtime cache");
}

bool test_missing_drawing_key_defaults_empty() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();

  auto payload = polysmith::protocol::to_payload(document, true);
  payload.erase("drawing");
  const auto restored =
      polysmith::protocol::document_from_payload(payload);

  return expect(restored.drawing.drawings.empty() &&
                    !restored.drawing.active_drawing_id.has_value() &&
                    restored.drawing.decimal_separator == ".",
                "missing drawing key must default to empty "
                "DrawingDocumentData");
}

bool test_legacy_comma_separator_migrates() {
  // The separator was never user-settable: a stored "," is the old
  // default, so the load path maps it to ".".
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.get_document().value();
  document.drawing.decimal_separator = ",";

  auto payload = polysmith::protocol::to_payload(document, true);
  const auto restored = polysmith::protocol::document_from_payload(payload);

  return expect(restored.drawing.decimal_separator == ".",
                "a stored ',' separator must load as '.'");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_save_load_test\n";
  std::cout << "  Test 1: document serialize/deserialize round trip... ";
  if (test_document_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: file round trip + id counter restore... ";
  if (test_file_round_trip_and_counter_restore()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: mutator shape (mint/cascade/validate)... ";
  if (test_mutator_shape()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: undo/redo invalidate the runtime cache... ";
  if (test_undo_redo_invalidate_runtime_cache()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: missing drawing key defaults empty... ";
  if (test_missing_drawing_key_defaults_empty()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: legacy ',' separator migrates to '.'... ";
  if (test_legacy_comma_separator_migrates()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cad_core_drawing_save_load_test passed\n";
    return 0;
  }
  return 1;
}
