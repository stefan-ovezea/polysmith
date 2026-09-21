// Drawing sheet note test (ANNOTATE → Text).
//
// Pins the free-text note pipeline end-to-end:
//   - drawing_note_create mints "drawing-note-N", rounds the position
//     to 0.01, and validates the sheet BEFORE the undo push
//   - drawing_note_update is cosmetic-only (text/position/height/
//     angle/align) and never re-projects
//   - flatten emits one SheetText per note (purpose "note", the
//     annotation_id field carries the note id) + glyph primitives
//   - save/load round-trip + id-counter restore (no colliding ids
//     after reload)
//   - deleting a sheet cascades its notes; undo/redo works
//   - golden pins the flattened emission

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "core/document/document.h"
#include "core/drawing/drawing_sheet.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::SheetNote;
using polysmith::core::SheetPrimitiveStream;
using polysmith::core::SheetText;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

bool throws(const std::function<void()>& fn) {
  try {
    fn();
  } catch (const std::runtime_error&) {
    return true;
  }
  return false;
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

std::filesystem::path golden_dir() {
  std::filesystem::path source(__FILE__);
  return source.parent_path() / "golden";
}

bool check_golden(const std::string& name, const std::string& text) {
  const bool update = std::getenv("POLYSMITH_UPDATE_GOLDEN") != nullptr;
  const auto path = golden_dir() / (name + ".txt");
  if (update) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path.string());
    out << text;
    return expect(out.good(), "golden file written");
  }
  std::ifstream in(path.string());
  if (!in.good()) {
    std::cerr << "FAIL: golden file missing: " << path.string()
              << " (run with POLYSMITH_UPDATE_GOLDEN=1)\n";
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return expect(buffer.str() == text,
                ("golden mismatch: " + path.string()).c_str());
}

// ── Fixture: document with a 20x20x10 box, a drawing (A4 portrait),
//    and a front view at sheet [30, 40], scale 1.
struct NoteFixture {
  DocumentManager manager;
  DocumentState document;
  std::string body_id;
  std::string drawing_id;
  std::string sheet_id;
};

NoteFixture make_fixture() {
  NoteFixture fixture;
  fixture.manager.create_document();
  fixture.manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      fixture.manager.get_document().value(), /*include_meshes=*/false);
  fixture.body_id = bodies.bodies[0].id;

  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  sheet.orientation = "portrait";
  sheet.paper_size = "A4";
  drawing.sheets.push_back(sheet);
  fixture.document = fixture.manager.drawing_create(drawing);
  fixture.drawing_id = fixture.document.drawing.drawings[0].drawing_id;
  fixture.sheet_id = fixture.document.drawing.drawings[0].sheets[0].sheet_id;
  return fixture;
}

const SheetNote* find_note(const DocumentState& document,
                           const std::string& id) {
  for (const auto& drawing : document.drawing.drawings) {
    for (const auto& note : drawing.notes) {
      if (note.note_id == id) {
        return &note;
      }
    }
  }
  return nullptr;
}

const SheetText* find_text(const SheetPrimitiveStream& stream,
                           const std::string& purpose) {
  for (const auto& t : stream.texts) {
    if (t.purpose == purpose) {
      return &t;
    }
  }
  return nullptr;
}

// ── Test 1: create a note ──────────────────────────────────────────

bool test_note_create() {
  NoteFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "M6 THREAD", {100.0, 150.0},
      5.0, std::nullopt, std::nullopt);
  const auto& notes = document.drawing.drawings[0].notes;
  if (!expect(notes.size() == 1, "one note created")) {
    return false;
  }
  const SheetNote& note = notes[0];
  if (!expect(note.note_id == "drawing-note-1" &&
                  note.sheet_id == fixture.sheet_id &&
                  note.text == "M6 THREAD" &&
                  near(note.position[0], 100.0) &&
                  near(note.position[1], 150.0) &&
                  near(note.height_mm, 5.0) &&
                  near(note.angle_deg, 0.0) &&
                  note.h_align == "center",
              "note fields minted with defaults")) {
    return false;
  }
  // The position rounds to 0.01.
  DocumentState rounded = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "rounded", {10.004, 20.006},
      std::nullopt, std::nullopt, std::nullopt);
  const SheetNote& second = rounded.drawing.drawings[0].notes[1];
  return expect(near(second.position[0], 10.0) &&
                    near(second.position[1], 20.01),
                "position rounds to 0.01");
}

// ── Test 2: unknown sheet throws before mutating ───────────────────

bool test_note_create_unknown_sheet_throws() {
  NoteFixture fixture = make_fixture();
  if (!expect(throws([&]() {
        fixture.manager.drawing_note_create(
            fixture.drawing_id, "no-such-sheet", "x", {1.0, 2.0},
            std::nullopt, std::nullopt, std::nullopt);
      }),
              "unknown sheet throws")) {
    return false;
  }
  if (!expect(throws([&]() {
        fixture.manager.drawing_note_create(
            fixture.drawing_id, fixture.sheet_id, "x", {1.0, 2.0}, -1.0,
            std::nullopt, std::nullopt);
      }),
              "non-positive height throws")) {
    return false;
  }
  return expect(fixture.manager.get_document().value()
                        .drawing.drawings[0]
                        .notes.empty(),
                "nothing was mutated");
}

// ── Test 3: cosmetic update ────────────────────────────────────────

bool test_note_update_text_position() {
  NoteFixture fixture = make_fixture();
  fixture.manager.drawing_note_create(fixture.drawing_id, fixture.sheet_id,
                                      "first", {10.0, 20.0}, std::nullopt,
                                      std::nullopt, std::nullopt);
  DocumentState document = fixture.manager.drawing_note_update(
      fixture.drawing_id, "drawing-note-1", "edited",
      std::array<double, 2>{30.0, 40.0}, 7.0, std::nullopt, std::nullopt);
  const SheetNote* note = find_note(document, "drawing-note-1");
  if (!expect(note != nullptr && note->text == "edited" &&
                  near(note->position[0], 30.0) &&
                  near(note->position[1], 40.0) &&
                  near(note->height_mm, 7.0),
              "update applies text/position/height")) {
    return false;
  }
  if (!expect(throws([&]() {
        fixture.manager.drawing_note_update(
            fixture.drawing_id, "no-such-note", "x", std::nullopt,
            std::nullopt, std::nullopt, std::nullopt);
      }),
              "unknown note throws")) {
    return false;
  }
  return true;
}

// ── Test 4: delete ─────────────────────────────────────────────────

bool test_note_delete() {
  NoteFixture fixture = make_fixture();
  fixture.manager.drawing_note_create(fixture.drawing_id, fixture.sheet_id,
                                      "x", {10.0, 20.0}, std::nullopt,
                                      std::nullopt, std::nullopt);
  DocumentState document =
      fixture.manager.drawing_note_delete(fixture.drawing_id, "drawing-note-1");
  return expect(document.drawing.drawings[0].notes.empty(),
                "note deleted");
}

// ── Test 5: undo/redo ──────────────────────────────────────────────

bool test_note_undo_redo() {
  NoteFixture fixture = make_fixture();
  fixture.manager.drawing_note_create(fixture.drawing_id, fixture.sheet_id,
                                      "x", {10.0, 20.0}, std::nullopt,
                                      std::nullopt, std::nullopt);
  DocumentState after_update = fixture.manager.drawing_note_update(
      fixture.drawing_id, "drawing-note-1", "y", std::nullopt, std::nullopt,
      std::nullopt, std::nullopt);
  if (!expect(find_note(after_update, "drawing-note-1")->text == "y",
              "update applied")) {
    return false;
  }
  const DocumentState undone = fixture.manager.undo();
  if (!expect(find_note(undone, "drawing-note-1") != nullptr &&
                  find_note(undone, "drawing-note-1")->text == "x",
              "undo restores the note text")) {
    return false;
  }
  const DocumentState redone = fixture.manager.redo();
  if (!expect(find_note(redone, "drawing-note-1")->text == "y",
              "redo reapplies")) {
    return false;
  }
  // Back to the create state (the note exists with its original
  // text), then one step further removes it entirely.
  fixture.manager.undo();
  const DocumentState undone_create = fixture.manager.undo();
  return expect(find_note(undone_create, "drawing-note-1") == nullptr,
                "undo past the create removes the note");
}

// ── Test 6: flatten emits the note as a text record ────────────────

bool test_note_flatten_position_purpose() {
  NoteFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "NOTE TEXT", {120.0, 90.0},
      5.0, std::nullopt, std::nullopt);
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet flattens")) {
    return false;
  }
  const SheetText* note_text = find_text(flat.value(), "note");
  if (!expect(note_text != nullptr, "note text record present")) {
    return false;
  }
  if (!expect(note_text->text == "NOTE TEXT" &&
                  near(note_text->position[0], 120.0) &&
                  near(note_text->position[1], 90.0) &&
                  near(note_text->height_mm, 5.0) &&
                  note_text->annotation_id.has_value() &&
                  note_text->annotation_id.value() == "drawing-note-1",
              "note text carries position/height/note id")) {
    return false;
  }
  // The glyph pass turns the note into vector primitives (the
  // viewport/SVG/PDF backends render those).
  const int glyphs = static_cast<int>(std::count_if(
      flat->primitives.begin(), flat->primitives.end(),
      [](const auto& p) { return p.purpose == "text_glyph"; }));
  return expect(glyphs > 0, "note glyphs emitted");
}

// ── Test 7: save/load round-trip ───────────────────────────────────

bool test_note_save_load_roundtrip() {
  NoteFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "persisted", {33.5, 77.25},
      5.0, std::nullopt, std::nullopt);
  const auto payload = polysmith::protocol::to_payload(document, true);
  const DocumentState restored =
      polysmith::protocol::document_from_payload(payload);
  const auto& notes = restored.drawing.drawings[0].notes;
  if (!expect(notes.size() == 1, "note survives the round-trip")) {
    return false;
  }
  return expect(notes[0].note_id == "drawing-note-1" &&
                    notes[0].sheet_id == fixture.sheet_id &&
                    notes[0].text == "persisted" &&
                    near(notes[0].position[0], 33.5) &&
                    near(notes[0].position[1], 77.25) &&
                    near(notes[0].height_mm, 5.0) &&
                    notes[0].h_align == "center",
                "note fields survive");
}

// ── Test 8: id counter restores after a file reload ────────────────

bool test_note_id_counter_after_reload() {
  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_drawing_note_test.json";
  NoteFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "x", {10.0, 20.0}, std::nullopt,
      std::nullopt, std::nullopt);
  {
    std::ofstream stream(path.string());
    stream << polysmith::protocol::to_payload(document, true).dump(2);
  }
  DocumentManager loaded;
  loaded.create_document();
  const DocumentState restored = loaded.load_document_from_path(path.string());
  if (!expect(restored.drawing.drawings[0].notes.size() == 1,
              "file load: note restored")) {
    return false;
  }
  // A new note must not collide with "drawing-note-1".
  const DocumentState after_create = loaded.drawing_note_create(
      restored.drawing.drawings[0].drawing_id,
      restored.drawing.drawings[0].sheets[0].sheet_id, "second", {1.0, 2.0},
      std::nullopt, std::nullopt, std::nullopt);
  const auto& notes = after_create.drawing.drawings[0].notes;
  return expect(notes.size() == 2 &&
                    notes[1].note_id == "drawing-note-2",
                "counter restore: new note id must not collide");
}

// ── Test 9: deleting a sheet cascades its notes ────────────────────

bool test_sheet_delete_cascades_notes() {
  NoteFixture fixture = make_fixture();
  fixture.manager.drawing_note_create(fixture.drawing_id, fixture.sheet_id,
                                      "x", {10.0, 20.0}, std::nullopt,
                                      std::nullopt, std::nullopt);
  DocumentState document = fixture.manager.drawing_sheet_delete(
      fixture.drawing_id, fixture.sheet_id);
  return expect(document.drawing.drawings[0].notes.empty(),
                "notes die with their sheet");
}

// ── Test 10: golden (A4 sheet with one note) ───────────────────────

bool test_note_golden() {
  NoteFixture fixture = make_fixture();
  DocumentState document = fixture.manager.drawing_note_create(
      fixture.drawing_id, fixture.sheet_id, "M6", {150.0, 100.0}, 3.5,
      std::nullopt, std::nullopt);
  const auto flat = polysmith::core::flatten_sheet(
      document, fixture.drawing_id, fixture.sheet_id);
  if (!expect(flat.has_value(), "sheet flattens")) {
    return false;
  }
  std::ostringstream out;
  out << "sheet " << flat->width_mm << "x" << flat->height_mm << "\n";
  for (const auto& p : flat->primitives) {
    // Glyph segments are pinned by the title-block glyph golden —
    // dumping ~6k of them here would drown the note record.
    if (p.purpose == "text_glyph") {
      continue;
    }
    out << p.purpose << " " << p.kind << " " << quant(p.p0[0]) << ","
        << quant(p.p0[1]) << " " << quant(p.p1[0]) << ","
        << quant(p.p1[1]);
    if (p.center.has_value()) {
      out << " c" << quant(p.center.value()[0]) << ","
          << quant(p.center.value()[1]);
    }
    if (p.radius.has_value()) {
      out << " r" << quant(p.radius.value());
    }
    if (!p.points.empty()) {
      out << " pts";
      for (const auto& point : p.points) {
        out << " " << quant(point[0]) << "," << quant(point[1]);
      }
    }
    out << "\n";
  }
  for (const auto& t : flat->texts) {
    out << "text " << t.purpose << " \"" << t.text << "\" "
        << quant(t.position[0]) << "," << quant(t.position[1]) << " h"
        << quant(t.height_mm) << (t.stale ? " stale" : "") << "\n";
  }
  return check_golden("drawing_note_text", out.str());
}

}  // namespace

int main() {
  int passed = 0;
  const int total = 10;
  struct Test {
    const char* name;
    bool (*fn)();
  };
  const Test tests[] = {
      {"note_create", test_note_create},
      {"note_create_unknown_sheet_throws", test_note_create_unknown_sheet_throws},
      {"note_update_text_position", test_note_update_text_position},
      {"note_delete", test_note_delete},
      {"note_undo_redo", test_note_undo_redo},
      {"note_flatten_position_purpose", test_note_flatten_position_purpose},
      {"note_save_load_roundtrip", test_note_save_load_roundtrip},
      {"note_id_counter_after_reload", test_note_id_counter_after_reload},
      {"note_sheet_delete_cascades", test_sheet_delete_cascades_notes},
      {"note_golden", test_note_golden},
  };
  for (const auto& test : tests) {
    if (test.fn()) {
      ++passed;
    } else {
      std::cerr << "FAILED TEST: " << test.name << "\n";
    }
  }
  std::cout << passed << "/" << total << " drawing note tests passed\n";
  return passed == total ? 0 : 1;
}
