// Drawing projection engine test (P2).
//
// Pins the production projection engine (core/drawing/
// drawing_projection.cpp) promoted from the P0 HLR spike, the
// standard-view frame convention, and the refresh pass
// (drawing_refresh.cpp) running inside bump_geometry_revision:
//   - box front projection: exact record set with witnesses
//   - cylinder off-axis: silhouette records carry FaceAttestation,
//     rim projections carry source-edge witnesses
//   - hidden-line toggle
//   - refresh-on-edit (model change re-projects the view),
//     missing body → broken_ref + warning + stale last-known
//   - view mutators (create/update/move/delete + validation)
//   - determinism + golden files (tests/golden/,
//     POLYSMITH_UPDATE_GOLDEN=1 regenerates)

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <variant>

#include "core/document/document.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "core/viewport/viewport.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::CylinderFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::DrawingViewFrame;
using polysmith::core::FaceAttestation;
using polysmith::core::ProjectedEdgeRecord;
using polysmith::core::ProjectionInput;
using polysmith::core::ProjectionResult;
using polysmith::core::SourceBody;
using polysmith::core::SourceEdgeWitness;

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

// ── Helpers ───────────────────────────────────────────────────────

std::array<double, 3> cross(const std::array<double, 3>& a,
                            const std::array<double, 3>& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

int visible_count(const ProjectionResult& result) {
  return static_cast<int>(std::count_if(
      result.edges.begin(), result.edges.end(),
      [](const ProjectedEdgeRecord& r) { return r.line_class == "visible"; }));
}

int hidden_count(const ProjectionResult& result) {
  return static_cast<int>(std::count_if(
      result.edges.begin(), result.edges.end(),
      [](const ProjectedEdgeRecord& r) { return r.line_class == "hidden"; }));
}

int outline_count(const ProjectionResult& result) {
  return static_cast<int>(std::count_if(
      result.edges.begin(), result.edges.end(),
      [](const ProjectedEdgeRecord& r) { return r.curve_class == "outline"; }));
}

std::string source_identity(const ProjectedEdgeRecord& rec) {
  const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
  if (witness == nullptr) {
    return "outline";
  }
  if (witness->body_id.empty()) {
    return "unresolved";
  }
  return witness->body_id + "#" + std::to_string(witness->src_edge_index);
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

/// Canonical single-line form of a record: the geometry key plus the
/// source identity — the golden-file foundation.
std::string record_line(const ProjectedEdgeRecord& rec) {
  std::ostringstream os;
  os << std::fixed;
  os.precision(6);
  os << rec.line_class << "|" << rec.curve_class << "|" << rec.curve_kind
     << "|";
  if (rec.curve_kind == "line") {
    double x0 = quant(rec.p_start[0]);
    double y0 = quant(rec.p_start[1]);
    double x1 = quant(rec.p_end[0]);
    double y1 = quant(rec.p_end[1]);
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    os << x0 << "," << y0 << "|" << x1 << "," << y1;
  } else {
    os << quant(rec.p_start[0]) << "," << quant(rec.p_start[1]) << "|"
       << quant(rec.p_end[0]) << "," << quant(rec.p_end[1]);
  }
  os << "|" << source_identity(rec);
  return os.str();
}

std::string canonical_text(const ProjectionResult& result) {
  std::vector<std::string> lines;
  for (const auto& rec : result.edges) {
    lines.push_back(record_line(rec));
  }
  std::sort(lines.begin(), lines.end());
  std::ostringstream os;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i) os << "\n";
    os << lines[i];
  }
  return os.str();
}

std::filesystem::path golden_dir() {
  // Resolve relative to THIS source file so the test works under any
  // cwd (the runner spawns from the repo root, but direct runs vary).
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
  return expect(buffer.str() == text, ("golden mismatch: " + path.string()).c_str());
}

/// Builds a document with one 20x20x10 box and returns its compiled
/// body id.
std::string make_box_document(DocumentManager& manager,
                              BoxFeatureParameters params) {
  manager.create_document();
  manager.add_box_feature(params);
  const auto bodies =
      polysmith::core::compile_bodies(manager.get_document().value(),
                                      /*include_meshes=*/false);
  if (bodies.bodies.empty()) {
    return "";
  }
  return bodies.bodies[0].id;
}

// ── Test 1: standard view frames ──────────────────────────────────

bool test_standard_view_frames() {
  // Side views draw the part upright (view-Y = normal × x_direction
  // = +Z); top/bottom draw it in first-angle layout (view-Y = -X —
  // the part's back faces up on the sheet).
  const std::string side_names[4] = {"front", "right", "left", "back"};
  for (const auto& name : side_names) {
    const auto frame = polysmith::core::standard_view_frame(name);
    if (!expect(frame.has_value(), "standard view resolves")) {
      return false;
    }
    const auto up = cross(frame->normal, frame->x_direction);
    if (!expect(near(up[0], 0.0) && near(up[1], 0.0) && near(up[2], 1.0),
                "side view keeps view-Y = +Z")) {
      return false;
    }
  }
  const std::string top_bottom[2] = {"top", "bottom"};
  for (const auto& name : top_bottom) {
    const auto frame = polysmith::core::standard_view_frame(name);
    if (!expect(frame.has_value(), "standard view resolves")) {
      return false;
    }
    const auto up = cross(frame->normal, frame->x_direction);
    if (!expect(near(up[0], -1.0) && near(up[1], 0.0) && near(up[2], 0.0),
                "top/bottom view keeps view-Y = -X (first-angle)")) {
      return false;
    }
  }
  // First-angle placement semantics pinned: front's sheet-X = +Y,
  // top's sheet-up = -X (part back), right view shows the front
  // pointing LEFT.
  const auto front = polysmith::core::standard_view_frame("front");
  const auto top = polysmith::core::standard_view_frame("top");
  const auto right = polysmith::core::standard_view_frame("right");
  if (!expect(front->x_direction[1] == 1.0,
              "front: sheet-X = +Y")) {
    return false;
  }
  if (!expect(near(top->x_direction[1], 1.0) && near(top->x_direction[0], 0.0),
              "top: sheet-X = +Y")) {
    return false;
  }
  if (!expect(right->x_direction[0] == -1.0,
              "right: front points LEFT (first-angle)")) {
    return false;
  }
  return expect(
      !polysmith::core::standard_view_frame("isometric").has_value(),
      "unknown standard view resolves to nullopt");
}

// ── Test 2: box front projection — exact record set ───────────────

bool test_box_front_projection() {
  DocumentManager manager;
  const std::string body_id =
      make_box_document(manager, {.width = 20.0, .height = 20.0,
                                  .depth = 10.0});
  if (!expect(!body_id.empty(), "box body compiled")) {
    return false;
  }
  const auto bodies = polysmith::core::compile_bodies(
      manager.get_document().value(), /*include_meshes=*/false);

  ProjectionInput input;
  input.sources.push_back({body_id, bodies.bodies[0].shape});
  input.frame = polysmith::core::standard_view_frame("front").value();
  input.show_hidden = true;
  input.source_revision = 1;
  const ProjectionResult result = polysmith::core::project(input);

  // Front view: 4 visible sharp (the x=20 face) + 4 hidden (the x=0
  // face).  The 4 depth edges project degenerately (parallel to the
  // view) and are filtered — the P0 spike found them landing hidden;
  // the production engine drops what cannot be drawn.
  if (!expect(visible_count(result) == 4 && hidden_count(result) == 4 &&
                  outline_count(result) == 0,
              "box front: 4 visible + 4 hidden, no outlines")) {
    std::cerr << "  got " << visible_count(result) << " visible, "
              << hidden_count(result) << " hidden, "
              << outline_count(result) << " outlines\n";
    return false;
  }
  // Full-set witness assertion: every record carries a witness for
  // the compiled body.
  for (const auto& rec : result.edges) {
    const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
    if (!expect(witness != nullptr && witness->body_id == body_id,
                "box front: every record carries a body witness")) {
      return false;
    }
  }
  // Visible records form the 20x10 rectangle in view space (view X =
  // world Y ∈ [0,20], view Y = world Z ∈ [0,10]).
  double min_x = 1e18, max_x = -1e18, min_y = 1e18, max_y = -1e18;
  for (const auto& rec : result.edges) {
    if (rec.line_class != "visible") continue;
    min_x = std::min({min_x, rec.p_start[0], rec.p_end[0]});
    max_x = std::max({max_x, rec.p_start[0], rec.p_end[0]});
    min_y = std::min({min_y, rec.p_start[1], rec.p_end[1]});
    max_y = std::max({max_y, rec.p_start[1], rec.p_end[1]});
  }
  return expect(near(min_x, 0.0, 1e-4) && near(max_x, 20.0, 1e-4) &&
                    near(min_y, 0.0, 1e-4) && near(max_y, 10.0, 1e-4),
                "box front: visible records bound the 20x10 face");
}

// ── Test 3: cylinder silhouette provenance ────────────────────────

bool test_cylinder_silhouette() {
  DocumentManager manager;
  manager.create_document();
  manager.add_cylinder_feature({.radius = 10.0, .height = 30.0});
  const auto bodies = polysmith::core::compile_bodies(
      manager.get_document().value(), /*include_meshes=*/false);
  if (!expect(!bodies.bodies.empty(), "cylinder body compiled")) {
    return false;
  }
  const std::string body_id = bodies.bodies[0].id;

  ProjectionInput input;
  input.sources.push_back({body_id, bodies.bodies[0].shape});
  // Off-axis frame (the P0 spike's): silhouettes appear, rim circles
  // project to ellipses.
  input.frame.origin = {0.0, 0.0, 0.0};
  input.frame.normal = {1.0, 0.4, 0.2};
  input.frame.x_direction = {0.0, 1.0, 0.0};
  input.show_hidden = true;
  input.source_revision = 1;
  const ProjectionResult result = polysmith::core::project(input);

  if (!expect(outline_count(result) >= 2,
              "cylinder off-axis produces silhouette outlines")) {
    return false;
  }
  bool outline_carries_face_attestation = true;
  for (const auto& rec : result.edges) {
    if (rec.curve_class == "outline") {
      if (!std::holds_alternative<FaceAttestation>(rec.source)) {
        outline_carries_face_attestation = false;
      }
    }
  }
  if (!expect(outline_carries_face_attestation,
              "silhouette records carry FaceAttestation")) {
    return false;
  }
  bool saw_ellipse_witness = false;
  for (const auto& rec : result.edges) {
    const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
    if (witness != nullptr && witness->curve_kind == "circle" &&
        rec.curve_kind == "ellipse") {
      saw_ellipse_witness = true;
    }
  }
  if (!saw_ellipse_witness) {
    std::cerr << "  cylinder records:\n";
    for (const auto& rec : result.edges) {
      const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
      std::cerr << "    " << rec.line_class << " " << rec.curve_class
                << " recKind=" << rec.curve_kind;
      if (witness != nullptr && !witness->body_id.empty()) {
        std::cerr << " body=" << witness->body_id
                  << " witnessKind=" << witness->curve_kind;
      } else {
        std::cerr << " (unresolved/face attestation)";
      }
      std::cerr << "\n";
    }
  }
  return expect(saw_ellipse_witness,
                "oblique rim projection is an ellipse with a circle "
                "witness");
}

// ── Test 4: hidden-line toggle ────────────────────────────────────

bool test_hidden_toggle() {
  DocumentManager manager;
  const std::string body_id =
      make_box_document(manager, {.width = 20.0, .height = 20.0,
                                  .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      manager.get_document().value(), /*include_meshes=*/false);

  ProjectionInput input;
  input.sources.push_back({body_id, bodies.bodies[0].shape});
  input.frame = polysmith::core::standard_view_frame("front").value();
  input.show_hidden = false;
  input.source_revision = 1;
  const ProjectionResult visible_only = polysmith::core::project(input);
  input.show_hidden = true;
  const ProjectionResult with_hidden = polysmith::core::project(input);

  return expect(visible_count(visible_only) == 4 &&
                    hidden_count(visible_only) == 0 &&
                    hidden_count(with_hidden) == 4,
                "hidden toggle: 4 visible / no hidden, then 4 hidden");
}

// ── Test 5: refresh on edit + broken body + undo ──────────────────

bool test_refresh_and_undo() {
  DocumentManager manager;
  const std::string body_id =
      make_box_document(manager, {.width = 20.0, .height = 20.0,
                                  .depth = 10.0});

  // Create a drawing + front view through the mutators; the bump
  // inside drawing_view_create runs the refresh pass, so the
  // projection must be cached at the current revision immediately.
  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  sheet.paper_size = "A4";
  drawing.sheets.push_back(sheet);
  DocumentState document = manager.drawing_create(drawing);
  const std::string drawing_id = document.drawing.drawings[0].drawing_id;
  const std::string sheet_id = document.drawing.drawings[0].sheets[0].sheet_id;

  DrawingView view;
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {body_id};
  document = manager.drawing_view_create(drawing_id, sheet_id, view);
  const std::string view_id = document.drawing.drawings[0].views[0].view_id;
  if (!expect(view_id == "drawing-view-1", "view id minted")) {
    return false;
  }

  const ProjectionResult* cached = polysmith::core::drawing_runtime::
      cached_projection(document, view_id);
  if (!expect(cached != nullptr && !cached->stale &&
                  visible_count(*cached) == 4,
              "view_create's bump re-projects and caches the view")) {
    return false;
  }
  const int revision_after_create = document.revision;

  // Model edit: resize the box HEIGHT (world Y = view X for the
  // front view) → the refresh re-projects at the new revision and
  // the view X extent grows 20 → 40.
  const std::string feature_id = document.feature_history.back().id;
  document = manager.update_box_feature(
      feature_id, {.width = 20.0, .height = 40.0, .depth = 10.0});
  if (!expect(document.revision != revision_after_create,
              "model edit bumps the revision")) {
    return false;
  }
  cached = polysmith::core::drawing_runtime::cached_projection(
      document, view_id);
  if (!expect(cached != nullptr && visible_count(*cached) == 4,
              "model edit re-projects the view")) {
    return false;
  }
  double max_x = -1e18;
  for (const auto& rec : cached->edges) {
    if (rec.line_class != "visible") continue;
    max_x = std::max({max_x, rec.p_start[0], rec.p_end[0]});
  }
  if (!expect(near(max_x, 40.0, 1e-4),
              "resized box: view X extent follows the model")) {
    return false;
  }

  // Broken body: a view referencing a body that does not exist must
  // degrade with broken_ref + warning + a stale last-known result —
  // never crash, never silently substitute.
  DrawingView ghost;
  ghost.kind = "projection";
  ghost.standard_view = "front";
  ghost.source_body_ids = {"body-that-does-not-exist"};
  document = manager.drawing_view_create(drawing_id, sheet_id, ghost);
  const std::string ghost_id = document.drawing.drawings[0].views[1].view_id;
  const auto& ghost_view = document.drawing.drawings[0].views[1];
  if (!expect(ghost_view.broken_ref.has_value() &&
                  ghost_view.broken_ref.value() ==
                      "body-that-does-not-exist" &&
                  !ghost_view.warning.empty(),
              "missing body sets broken_ref + warning")) {
    return false;
  }
  const ProjectionResult* ghost_result =
      polysmith::core::drawing_runtime::cached_projection(document,
                                                          ghost_id);
  if (!expect(ghost_result != nullptr && ghost_result->stale,
              "missing body stores a stale last-known result")) {
    return false;
  }

  // Undo removes the ghost view and its cached result; redo restores
  // both (the refresh re-runs inside the undo/redo bumps).
  document = manager.undo();
  if (!expect(document.drawing.drawings[0].views.size() == 1 &&
                  polysmith::core::drawing_runtime::cached_projection(
                      document, ghost_id) == nullptr,
              "undo removes the view and its cached projection")) {
    return false;
  }
  document = manager.redo();
  const ProjectionResult* redone =
      polysmith::core::drawing_runtime::cached_projection(document,
                                                          ghost_id);
  return expect(redone != nullptr && redone->stale &&
                    document.drawing.drawings[0].views.size() == 2,
                "redo restores the broken view and its stale result");
}

// ── Test 6: view mutator shape ────────────────────────────────────

bool test_view_mutators() {
  DocumentManager manager;
  const std::string body_id =
      make_box_document(manager, {.width = 20.0, .height = 20.0,
                                  .depth = 10.0});
  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  drawing.sheets.push_back(sheet);
  DocumentState document = manager.drawing_create(drawing);
  const std::string drawing_id = document.drawing.drawings[0].drawing_id;
  const std::string sheet_id = document.drawing.drawings[0].sheets[0].sheet_id;

  // Validation rejects BEFORE the undo push: a section view without a
  // definition, unknown standard view, empty source bodies.
  const auto rejects = [&](const DrawingView& bad) {
    bool threw = false;
    try {
      manager.drawing_view_create(drawing_id, sheet_id, bad);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    return threw;
  };
  DrawingView section_view;
  section_view.kind = "section";
  section_view.source_body_ids = {body_id};
  if (!expect(rejects(section_view),
              "section view without a definition rejected")) {
    return false;
  }
  DrawingView bad_standard;
  bad_standard.kind = "projection";
  bad_standard.standard_view = "iso";
  bad_standard.source_body_ids = {body_id};
  if (!expect(rejects(bad_standard), "unknown standard view rejected")) {
    return false;
  }
  DrawingView no_bodies;
  no_bodies.kind = "projection";
  no_bodies.standard_view = "front";
  if (!expect(rejects(no_bodies), "view without source bodies rejected")) {
    return false;
  }
  if (!expect(manager.get_document()->undo_step_names.size() == 2,
              "rejected creates leave no undo steps")) {
    return false;
  }

  // Create appends to the sheet's ordering list.
  DrawingView view;
  view.kind = "projection";
  view.standard_view = "top";
  view.source_body_ids = {body_id};
  document = manager.drawing_view_create(drawing_id, sheet_id, view);
  const std::string view_id = document.drawing.drawings[0].views[0].view_id;
  if (!expect(document.drawing.drawings[0].sheets[0].view_ids ==
                  std::vector<std::string>{view_id},
              "view_create appends to the sheet ordering")) {
    return false;
  }

  // Move changes the sheet position (cosmetic).
  document = manager.drawing_view_move(drawing_id, view_id, {15.0, 25.0});
  if (!expect(document.drawing.drawings[0].views[0].sheet_position ==
                  std::array<double, 2>{15.0, 25.0},
              "view_move updates the sheet position")) {
    return false;
  }

  // Update replaces the definition (same id, stays on the sheet).
  DrawingView updated = document.drawing.drawings[0].views[0];
  updated.standard_view = "right";
  updated.sheet_position = {0.0, 0.0};
  document = manager.drawing_view_update(drawing_id, updated);
  const auto& stored = document.drawing.drawings[0].views[0];
  if (!expect(stored.standard_view == "right" &&
                  stored.view_id == view_id,
              "view_update replaces the definition")) {
    return false;
  }

  // Delete removes the view and its sheet reference.
  document = manager.drawing_view_delete(drawing_id, view_id);
  if (!expect(document.drawing.drawings[0].views.empty() &&
                  document.drawing.drawings[0].sheets[0].view_ids.empty(),
              "view_delete removes the view and the sheet reference")) {
    return false;
  }

  // P4: a section view WITH a valid definition is accepted now (the
  // frame derives from the cutting plane — no standard view needed).
  DrawingView accepted_section;
  accepted_section.kind = "section";
  accepted_section.source_body_ids = {body_id};
  polysmith::core::SectionDefinition section;
  section.cutting_plane_point = {10.0, 0.0, 0.0};
  section.cutting_plane_normal = {1.0, 0.0, 0.0};
  accepted_section.section = section;
  document = manager.drawing_view_create(drawing_id, sheet_id,
                                         accepted_section);
  return expect(document.drawing.drawings[0].views.size() == 1 &&
                    document.drawing.drawings[0].views[0].kind == "section",
                "section views with a definition are accepted (P4)");
}

// ── Test 7b: viewport sheet emission ──────────────────────────────

bool test_viewport_sheet_emission() {
  DocumentManager manager;
  const std::string body_id =
      make_box_document(manager, {.width = 20.0, .height = 20.0,
                                  .depth = 10.0});

  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  sheet.paper_size = "A4";
  drawing.sheets.push_back(sheet);
  DocumentState document = manager.drawing_create(drawing);
  const std::string drawing_id = document.drawing.drawings[0].drawing_id;
  const std::string sheet_id = document.drawing.drawings[0].sheets[0].sheet_id;

  DrawingView view;
  view.kind = "projection";
  view.standard_view = "front";
  view.source_body_ids = {body_id};
  view.scale = 0.5;
  view.sheet_position = {30.0, 40.0};
  document = manager.drawing_view_create(drawing_id, sheet_id, view);

  const auto state = polysmith::core::build_viewport_state(document);
  if (!expect(state.drawing_sheets.size() == 1,
              "viewport emits one sheet for the active drawing")) {
    return false;
  }
  const auto& emitted = state.drawing_sheets[0];
  if (!expect(near(emitted.width_mm, 210.0) &&
                  near(emitted.height_mm, 297.0) &&
                  emitted.sheet_id == sheet_id,
              "viewport emits the A4 sheet dimensions")) {
    return false;
  }
  // Front view at scale 0.5 from (30, 40): the visible face bounds
  // are [30, 40] x [40, 45] in sheet-mm (20x10 at half scale).  The
  // P5 payload also carries the ISO 5457 furniture — count the VIEW
  // geometry only.
  int visible = 0;
  for (const auto& curve : emitted.curves) {
    if (curve.purpose != "view_geometry" ||
        curve.line_class != "visible") {
      continue;
    }
    ++visible;
  }
  if (!expect(visible == 4,
              "viewport emits the 4 visible face curves")) {
    return false;
  }
  if (!expect(emitted.views.size() == 1 &&
                  emitted.views[0].label == "front" &&
                  near(emitted.views[0].scale, 0.5) &&
                  near(emitted.views[0].min[0], 30.0, 1e-4) &&
                  near(emitted.views[0].max[0], 40.0, 1e-4) &&
                  near(emitted.views[0].min[1], 40.0, 1e-4) &&
                  near(emitted.views[0].max[1], 45.0, 1e-4) &&
                  !emitted.views[0].stale,
              "viewport emits the view bounds in sheet-mm")) {
    return false;
  }
  // Hidden edges off: no hidden curves.
  for (const auto& curve : emitted.curves) {
    if (!expect(curve.line_class == "visible",
                "show_hidden=false emits no hidden curves")) {
      return false;
    }
  }
  // A view curve's sheet-space coordinates: view (0,0) maps to
  // (30, 40).
  const auto first = std::find_if(
      emitted.curves.begin(), emitted.curves.end(),
      [](const auto& curve) { return curve.purpose == "view_geometry"; });
  if (!expect(first != emitted.curves.end(),
              "the payload carries view geometry")) {
    return false;
  }
  const bool at_origin = (near(first->p0[0], 30.0, 1e-4) &&
                          near(first->p0[1], 40.0, 1e-4)) ||
                         (near(first->p1[0], 30.0, 1e-4) &&
                          near(first->p1[1], 40.0, 1e-4));
  return expect(at_origin,
                "curves are transformed to sheet-mm by scale + position");
}

// ── Test 7: determinism + golden files ────────────────────────────

bool test_determinism_and_golden() {
  {
    DocumentManager manager;
    const std::string body_id =
        make_box_document(manager, {.width = 20.0, .height = 20.0,
                                    .depth = 10.0});
    const auto bodies = polysmith::core::compile_bodies(
        manager.get_document().value(), /*include_meshes=*/false);
    ProjectionInput input;
    input.sources.push_back({body_id, bodies.bodies[0].shape});
    input.frame = polysmith::core::standard_view_frame("front").value();
    input.show_hidden = true;
    input.source_revision = 1;
    const ProjectionResult first = polysmith::core::project(input);
    const ProjectionResult second = polysmith::core::project(input);
    if (!expect(canonical_text(first) == canonical_text(second),
                "determinism: identical input gives identical output")) {
      return false;
    }
    if (!check_golden("drawing_projection_box_front",
                      canonical_text(first))) {
      return false;
    }
  }
  {
    DocumentManager manager;
    manager.create_document();
    manager.add_cylinder_feature({.radius = 10.0, .height = 30.0});
    const auto bodies = polysmith::core::compile_bodies(
        manager.get_document().value(), /*include_meshes=*/false);
    ProjectionInput input;
    input.sources.push_back({bodies.bodies[0].id, bodies.bodies[0].shape});
    input.frame.origin = {0.0, 0.0, 0.0};
    input.frame.normal = {1.0, 0.4, 0.2};
    input.frame.x_direction = {0.0, 1.0, 0.0};
    input.show_hidden = true;
    input.source_revision = 1;
    const ProjectionResult result = polysmith::core::project(input);
    if (!check_golden("drawing_projection_cylinder_offaxis",
                      canonical_text(result))) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_projection_test\n";
  std::cout << "  Test 1: standard view frames... ";
  if (test_standard_view_frames()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: box front projection... ";
  if (test_box_front_projection()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: cylinder silhouette provenance... ";
  if (test_cylinder_silhouette()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: hidden-line toggle... ";
  if (test_hidden_toggle()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: refresh on edit + broken body + undo... ";
  if (test_refresh_and_undo()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: view mutator shape... ";
  if (test_view_mutators()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: determinism + golden files... ";
  if (test_determinism_and_golden()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 8: viewport sheet emission... ";
  if (test_viewport_sheet_emission()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cad_core_drawing_projection_test passed\n";
    return 0;
  }
  return 1;
}
