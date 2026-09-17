// Undo/redo redesign — stage-isolated verification program.
//
// Each building stage of the redesign (spec: Undo-Redo-Redesign-
// Requirements.md) has a dedicated test group. Run the whole program
// (joins the automatic gate run) or isolate one stage:
//
//   cad_core_undo_stages_test            -> all stages
//   cad_core_undo_stages_test --stage 5  -> projection remove only
//   cad_core_undo_stages_test --list     -> stage names
//
// Stage map:
//   1  Empty-stack contract + can_undo/can_redo flags in the payload
//   2  Undo/redo refresh pipeline (revision monotonic, selection
//      prune after a session undo)
//   3  Extrude panel session (create + N updates = ONE undo; redo
//      survives the updates)
//   4  Construction-plane preview never clears the redo branch
//   5  remove_sketch_projections Remove mode always ONE entry
//   6  Timeline cursor scrub (no history pollution, proper bump)
//   7  2D session semantics (per-action undo inside, one step after
//      finish, abort leaves no trace, nested dimension group)
//   8  Named steps + depth limit + undo_many
//   9  CAM generate is undoable and invalidates the runtime cache

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/cam/cam_runtime.h"
#include "core/cam/toolpath.h"
#include "core/document/document.h"
#include "core/document/feature.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_feature_parameters.h"
#include "core/sketch/sketch_projection_types.h"
#include "protocol/serialization.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchProjectedPoint;
using polysmith::core::SketchProjection;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

// The sketch that is (or most recently was) active — the last sketch
// feature in history.
const SketchFeatureParameters& last_sketch_params(const DocumentState& doc) {
  for (auto it = doc.feature_history.rbegin(); it != doc.feature_history.rend();
       ++it) {
    if (it->kind == "sketch" && it->sketch_parameters.has_value()) {
      return it->sketch_parameters.value();
    }
  }
  throw std::runtime_error("no sketch feature in history");
}

int sketch_line_count(const DocumentState& doc) {
  return static_cast<int>(last_sketch_params(doc).lines.size());
}

// ── Stage 1: empty-stack contract + payload flags ─────────────────

bool stage1_empty_stack_contract() {
  {
    DocumentManager manager;
    manager.create_document();

    // Fresh document: nothing to undo/redo, and the flags must travel
    // in the document payload (D7), not only in the request-only
    // session_state.
    if (!expect(!manager.get_session_state().can_undo &&
                    !manager.get_session_state().can_redo,
                "stage1: fresh doc reports can_undo=false/can_redo=false")) {
      return false;
    }
    const auto payload =
        polysmith::protocol::to_payload(manager.get_document().value());
    if (!expect(payload.contains("can_undo") && payload["can_undo"] == false &&
                    payload.contains("can_redo") && payload["can_redo"] == false,
                "stage1: payload carries can_undo/can_redo=false")) {
      return false;
    }

    // The manager-level backstop contract stays a throw (the app
    // handler pre-checks and reports EMPTY_UNDO_STACK instead).
    bool threw = false;
    try {
      manager.undo();
    } catch (const std::runtime_error& error) {
      threw = std::string(error.what()).find("Nothing to undo") !=
              std::string::npos;
    }
    if (!expect(threw, "stage1: empty-stack undo throws 'Nothing to undo'")) {
      return false;
    }
  }

  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
    manager.finish_sketch();

    if (!expect(manager.get_session_state().can_undo,
                "stage1: a command makes can_undo true")) {
      return false;
    }
    const auto payload =
        polysmith::protocol::to_payload(manager.get_document().value());
    if (!expect(payload.contains("can_undo") && payload["can_undo"] == true,
                "stage1: payload can_undo=true after a command")) {
      return false;
    }

    // Undo the sketch session step -> the line is gone; redo brings it
    // back.
    const DocumentState undone = manager.undo();
    if (!expect(sketch_line_count(undone) == 0,
                "stage1: undo removes the sketch session edits")) {
      return false;
    }
    if (!expect(manager.get_session_state().can_redo,
                "stage1: undo makes can_redo true")) {
      return false;
    }
    const DocumentState redone = manager.redo();
    if (!expect(sketch_line_count(redone) == 1,
                "stage1: redo restores the line")) {
      return false;
    }
    if (!expect(!manager.get_session_state().can_redo,
                "stage1: redo empties the redo stack")) {
      return false;
    }
  }

  return true;
}

// ── Stage 2: undo/redo refresh pipeline ───────────────────────────

bool stage2_refresh_pipeline() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  const int revision_after_add =
      manager.get_document().value().revision;
  manager.finish_sketch();

  const DocumentState undone = manager.undo();
  // D1: the restore must run the full refresh — the revision must
  // advance past every value seen before (monotonic clock), never
  // regress to the snapshot's stored value.
  if (!expect(undone.revision > revision_after_add,
              "stage2: undo bumps the revision (monotonic, not regressed)")) {
    return false;
  }

  // Selection prune: select an entity that only exists AFTER the
  // session snapshot, undo, and the dangling selection must be
  // cleared by the prune inside the restore's bump.
  DocumentManager manager2;
  manager2.create_document();
  manager2.start_sketch_on_plane("ref-plane-xy");
  manager2.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  manager2.select_sketch_entity("line-1");
  manager2.finish_sketch();
  const DocumentState pruned = manager2.undo();
  if (!expect(!pruned.selected_sketch_entity_id.has_value(),
              "stage2: undo prunes selection ids the restored doc lacks")) {
    return false;
  }
  return true;
}

// ── Stage 3: extrude panel session is ONE step ────────────────────

bool stage3_extrude_panel_session() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 20.0, 20.0);
  DocumentState document = manager.finish_sketch();

  const auto& sketch = last_sketch_params(document);
  if (sketch.profiles.empty()) {
    return expect(false, "stage3: rectangle produced no profile");
  }
  const std::string profile_id = sketch.profiles[0].id;
  document = manager.extrude_profiles({profile_id}, 10.0, "new_body");

  const auto find_extrude = [](const DocumentState& doc) {
    for (const auto& feature : doc.feature_history) {
      if (feature.kind == "extrude") {
        return feature.id;
      }
    }
    return std::string();
  };
  const std::string extrude_id = find_extrude(document);
  if (!expect(!extrude_id.empty(), "stage3: extrude feature exists")) {
    return false;
  }

  // The panel session: several live updates. Each must mutate without
  // creating an undo step (D5 — the create already pushed ONE).
  manager.update_extrude_depth(extrude_id, 15.0);
  manager.update_extrude_depth(extrude_id, 20.0);
  manager.update_extrude_mode(extrude_id, "cut");

  // One undo must remove the ENTIRE extrude (create + every update
  // collapse to the create's entry).
  const DocumentState undone = manager.undo();
  if (!expect(find_extrude(undone).empty(),
              "stage3: ONE undo removes the whole extrude panel session")) {
    return false;
  }

  // The redo branch survived the live updates (previews must never
  // clear redo) and re-applies the FINAL parameters.
  const DocumentState redone = manager.redo();
  if (!expect(!find_extrude(redone).empty(),
              "stage3: redo re-applies the extrude")) {
    return false;
  }
  for (const auto& feature : redone.feature_history) {
    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      if (!expect(feature.extrude_parameters->side1.distance == 20.0,
                  "stage3: redo restores the final depth, not an intermediate")) {
        return false;
      }
      if (!expect(feature.extrude_parameters->mode == "cut",
                  "stage3: redo restores the final mode")) {
        return false;
      }
    }
  }
  return true;
}

// ── Stage 4: construction-plane preview keeps the redo branch ─────

bool stage4_plane_preview_keeps_redo() {
  DocumentManager manager;
  manager.create_document();
  const DocumentState created =
      manager.create_offset_plane("ref-plane-xy", 10.0);
  const std::string plane_id =
      created.feature_history.back().id;

  // Live preview scrubs (previously these cleared the redo stack —
  // D3 forbids that).
  manager.update_offset_plane(plane_id, 25.0);
  manager.update_offset_plane(plane_id, 30.0);

  const DocumentState undone = manager.undo();
  if (!expect(manager.get_session_state().can_redo,
              "stage4: preview did not destroy the redo branch")) {
    return false;
  }
  const DocumentState redone = manager.redo();
  bool found = false;
  for (const auto& feature : redone.feature_history) {
    if (feature.kind == "construction_plane" &&
        feature.construction_plane_parameters.has_value()) {
      found = true;
      if (!expect(feature.construction_plane_parameters->offset == 30.0,
                  "stage4: redo restores the final preview value")) {
        return false;
      }
    }
  }
  return expect(found, "stage4: construction plane exists after redo");
}

// ── Stage 5: remove_sketch_projections always ONE entry ───────────

// Hand-builds a sketch feature with a projection record, a projected
// point and a source link — the shape remove_sketch_projections
// mutates — without needing a body to project from.  Installed via
// the save/load seam (the established synthetic-document pattern).
FeatureEntry make_projected_sketch(const std::string& id,
                                   const std::string& plane_ref,
                                   int next_vertex_index,
                                   bool with_record) {
  SketchFeatureParameters params;
  params.plane_id = plane_ref;
  params.next_vertex_index = next_vertex_index;
  SketchLine line;
  line.id = "line-1";
  line.start_vertex_id = "vertex-1";
  line.end_vertex_id = "vertex-2";
  line.start_x = 0.0;
  line.start_y = 0.0;
  line.end_x = 10.0;
  line.end_y = 0.0;
  params.lines.push_back(line);
  if (with_record) {
    SketchProjection projection;
    projection.id = "projection-1";
    projection.source_id = "body-1:face:0";
    projection.source_kind = "face";
    projection.generated_line_ids.push_back("line-1");
    params.projections.push_back(projection);
  }
  params.projected_sources.push_back("body-1:face:0");
  SketchProjectedPoint point;
  point.id = "projected-point-1";
  point.x = 5.0;
  point.y = 0.0;
  point.source_id = "body-1:face:0";
  params.projected_points.push_back(point);
  FeatureEntry feature;
  feature.id = id;
  feature.kind = "sketch";
  feature.name = "Projected Sketch";
  feature.status = "healthy";
  feature.sketch_parameters = params;
  return feature;
}

DocumentManager load_synthetic(const DocumentState& document,
                               const char* suffix) {
  const auto path = std::filesystem::temp_directory_path() /
                    (std::string("polysmith_undo_stages_") + suffix +
                     ".json");
  {
    std::ofstream stream(path.string());
    stream << polysmith::protocol::to_payload(document, true).dump(2);
  }
  DocumentManager manager;
  manager.create_document();
  manager.load_document_from_path(path.string());
  return manager;
}

bool stage5_projection_remove_single_entry() {
  const auto find_projection = [](const DocumentState& doc) {
    for (const auto& feature : doc.feature_history) {
      if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
        return feature.sketch_parameters.value();
      }
    }
    throw std::runtime_error("no sketch feature in history");
  };

  // Case 1: records + entities exist — the nested delete pushes, and
  // the group collapses everything (delete + sources + point sweep)
  // into ONE entry.
  {
    DocumentManager seed;
    seed.create_document();
    DocumentState synthetic = seed.get_document().value();
    synthetic.feature_history.push_back(
        make_projected_sketch("feature-10", "ref-plane-xy", 100, true));

    DocumentManager manager = load_synthetic(synthetic, "proj_case1");
    const DocumentState removed =
        manager.remove_sketch_projections("feature-10", false);
    if (!expect(manager.get_session_state().can_undo,
                "stage5: remove leaves exactly one undo entry")) {
      return false;
    }
    // ONE undo must restore EVERYTHING the remove touched: the
    // deleted line, the record, the sources and the swept point.
    const DocumentState undone = manager.undo();
    const auto& params = find_projection(undone);
    if (!expect(params.lines.size() == 1,
                "stage5: one undo restores the deleted projected line")) {
      return false;
    }
    if (!expect(params.projections.size() == 1 &&
                    params.projected_sources.size() == 1 &&
                    params.projected_points.size() == 1,
                "stage5: one undo restores record, sources and swept point")) {
      return false;
    }
  }

  // Case 2: the records are already gone (entities deleted
  // individually) — only sources + a stray projected point remain.
  // The nested delete is a no-op, yet the bookkeeping sweep must
  // still commit as ONE undoable step.
  {
    DocumentManager seed;
    seed.create_document();
    DocumentState synthetic = seed.get_document().value();
    synthetic.feature_history.push_back(
        make_projected_sketch("feature-10", "ref-plane-xy", 100, false));

    DocumentManager manager = load_synthetic(synthetic, "proj_case2");
    manager.remove_sketch_projections("feature-10", false);
    if (!expect(manager.get_session_state().can_undo,
                "stage5 case2: sweep-only remove leaves one undo entry")) {
      return false;
    }
    const DocumentState undone = manager.undo();
    const auto& params = find_projection(undone);
    if (!expect(params.projected_sources.size() == 1 &&
                    params.projected_points.size() == 1,
                "stage5 case2: one undo restores the swept bookkeeping")) {
      return false;
    }
  }

  return true;
}

// ── Stage 6: timeline cursor scrub ────────────────────────────────

bool stage6_timeline_scrub() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  manager.finish_sketch();

  const int before = manager.get_document().value().revision;
  const DocumentState scrubbed = manager.set_timeline_cursor(1);
  if (!expect(scrubbed.revision > before,
              "stage6: scrub bumps the revision through the refresh pass")) {
    return false;
  }
  // The scrub must not pollute the history: one undo still removes
  // the sketch session edits (not the scrub).
  const DocumentState undone = manager.undo();
  if (!expect(sketch_line_count(undone) == 0,
              "stage6: undo after a scrub undoes the real edit")) {
    return false;
  }
  return true;
}

// ── Stage 7: 2D session semantics ─────────────────────────────────

bool stage7_session_semantics() {
  // 7a. Per-action undo INSIDE the open session; the boundary protects
  // the session's own creation entry.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);   // line-1
    manager.add_sketch_line(0.0, 10.0, 10.0, 10.0); // line-2
    if (!expect(sketch_line_count(manager.get_document().value()) == 2,
                "stage7a: two lines in the open session")) {
      return false;
    }
    DocumentState doc = manager.undo();  // pops line-2 (per-action)
    if (!expect(sketch_line_count(doc) == 1,
                "stage7a: undo inside the session removes ONE line")) {
      return false;
    }
    doc = manager.undo();  // pops line-1
    if (!expect(sketch_line_count(doc) == 0,
                "stage7a: second in-session undo removes the other line")) {
      return false;
    }
    // Third undo must stop at the session boundary (no-op), never
    // consuming the sketch-creation entry.
    doc = manager.undo();
    if (!expect(!doc.feature_history.empty() &&
                    doc.feature_history.back().kind == "sketch",
                "stage7a: in-session undo stops at the session boundary")) {
      return false;
    }
    // Redo inside the session brings the lines back.
    doc = manager.redo();
    if (!expect(sketch_line_count(doc) == 1,
                "stage7a: redo inside the session restores a line")) {
      return false;
    }
  }

  // 7b. Closing the session collapses everything into ONE step.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
    manager.add_sketch_line(0.0, 10.0, 10.0, 10.0);
    manager.add_sketch_line(0.0, 20.0, 10.0, 20.0);
    manager.finish_sketch();
    const DocumentState undone = manager.undo();
    if (!expect(sketch_line_count(undone) == 0,
                "stage7b: ONE undo removes the whole session (3 lines)")) {
      return false;
    }
    const DocumentState redone = manager.redo();
    if (!expect(sketch_line_count(redone) == 3,
                "stage7b: ONE redo restores the whole session")) {
      return false;
    }
  }

  // 7c. Abort leaves no trace: the session's steps vanish, one undo
  // still reaches the pre-session state.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
    manager.add_sketch_line(0.0, 10.0, 10.0, 10.0);
    const DocumentState aborted = manager.abort_undo_group();
    if (!expect(sketch_line_count(aborted) == 0,
                "stage7c: abort rolls the session back")) {
      return false;
    }
    // The session left no step behind: the next undo removes the
    // sketch creation itself.
    const DocumentState undone = manager.undo();
    bool has_sketch = false;
    for (const auto& feature : undone.feature_history) {
      if (feature.kind == "sketch") {
        has_sketch = true;
      }
    }
    if (!expect(!has_sketch,
                "stage7c: abort left no trace — undo reaches the creation")) {
      return false;
    }
  }

  // 7d. Nested group (dimension draft) collapses to one LOCAL step
  // inside the session.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.begin_undo_group("Dimension");
    manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
    manager.add_sketch_line(0.0, 10.0, 10.0, 10.0);
    manager.end_undo_group();
    // Inside the session the nested group is ONE step: a single undo
    // removes BOTH lines of the draft.
    DocumentState doc = manager.undo();
    if (!expect(sketch_line_count(doc) == 0,
                "stage7d: the nested draft group is one in-session step")) {
      return false;
    }
    doc = manager.redo();
    if (!expect(sketch_line_count(doc) == 2,
                "stage7d: redo restores the whole draft")) {
      return false;
    }
    // And the whole session still collapses to one step on finish.
    manager.finish_sketch();
    doc = manager.undo();
    if (!expect(sketch_line_count(doc) == 0,
                "stage7d: session with a nested group is still ONE step")) {
      return false;
    }
  }

  return true;
}

// ── Stage 8: named steps + depth limit + undo_many ────────────────

bool stage8_names_limit_multi() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  manager.finish_sketch();

  // The document payload carries the step names (most recent first)
  // for tooltips and the history dropdown.
  const auto payload =
      polysmith::protocol::to_payload(manager.get_document().value());
  if (!expect(payload.contains("undo_step_names"),
              "stage8: payload carries undo_step_names")) {
    return false;
  }
  if (!expect(!payload["undo_step_names"].empty() &&
                  payload["undo_step_names"][0] == "Sketch Edit",
              "stage8: the session step is named 'Sketch Edit'")) {
    return false;
  }
  if (!expect(payload["undo_step_names"].size() == 2 &&
                  payload["undo_step_names"][1] == "Start Sketch On Plane",
              "stage8: the creation step is named 'Start Sketch On Plane'")) {
    return false;
  }

  // Depth limit: cap at 2; three parameter adds leave only the two
  // newest steps.
  manager.set_undo_limit(2);
  manager.add_parameter("a", "1", "length");
  manager.add_parameter("b", "2", "length");
  manager.add_parameter("c", "3", "length");
  {
    const auto limited =
        polysmith::protocol::to_payload(manager.get_document().value());
    if (!expect(limited["undo_step_names"].size() == 2,
                "stage8: depth limit bounds the named stack")) {
      return false;
    }
  }
  manager.undo();
  manager.undo();
  if (!expect(!manager.get_session_state().can_undo,
              "stage8: history is empty after undoing the bounded stack")) {
    return false;
  }

  // undo_many: several steps in ONE call (the history-dropdown
  // semantics), and negative count = redo direction.
  DocumentManager batch;
  batch.create_document();
  batch.add_parameter("x", "1", "length");
  batch.add_parameter("y", "2", "length");
  batch.add_parameter("z", "3", "length");
  const DocumentState undone = batch.undo_many(2);
  if (!expect(undone.parameters.size() == 1 &&
                  undone.parameters[0].name == "x",
              "stage8: undo_many(2) removes exactly two steps")) {
    return false;
  }
  const DocumentState redone = batch.undo_many(-2);
  if (!expect(redone.parameters.size() == 3,
              "stage8: undo_many(-2) redoes both steps")) {
    return false;
  }
  return true;
}

// ── Stage 9: CAM generate is undoable + cache invalidation ────────

bool stage9_cam_generate() {
  DocumentManager manager;
  manager.create_document();

  CamSetup setup;
  setup.name = "Setup 1";
  setup.machine_type = "3_axis_mill";
  manager.cam_setup_create(setup);
  ToolEntry tool;
  tool.name = "6mm endmill";
  tool.type = "endmill_flat";
  DocumentState document = manager.cam_tool_add(tool);
  const std::string tool_id = document.cam.tool_library.back().tool_id;
  CamOperation op;
  op.name = "Face";
  op.type = "face_milling";
  op.tool_id = tool_id;
  document = manager.cam_operation_add(op);
  const std::string op_id = document.cam.operations.back().op_id;

  // Generate: an undoable commit that also stores the runtime path.
  Toolpath path;
  path.op_id = op_id;
  path.total_length_mm = 42.0;
  path.moves.push_back(
      ToolpathMove{/*kind=*/ToolpathMoveKind::Rapid, /*x=*/0.0, /*y=*/0.0,
                   /*z=*/5.0});
  const CamOperation* stored = nullptr;
  for (const auto& entry : document.cam.operations) {
    if (entry.op_id == op_id) {
      stored = &entry;
    }
  }
  if (!expect(stored != nullptr, "stage9: operation exists before generate")) {
    return false;
  }
  document = manager.cam_operation_set_generated(op_id, *stored, path);
  auto find_status = [&](const DocumentState& doc) {
    for (const auto& entry : doc.cam.operations) {
      if (entry.op_id == op_id) {
        return entry.status;
      }
    }
    return std::string("missing");
  };
  if (!expect(find_status(document) == "generated",
              "stage9: generate marks the op generated")) {
    return false;
  }
  if (!expect(polysmith::core::cam_runtime::cached_toolpath(document, op_id) !=
                  nullptr,
              "stage9: the generated path is cached")) {
    return false;
  }

  // Undo reverts the generated badge AND invalidates the runtime
  // path (D9/D1).
  document = manager.undo();
  if (!expect(find_status(document) != "generated",
              "stage9: undo reverts the generated status")) {
    return false;
  }
  if (!expect(polysmith::core::cam_runtime::cached_toolpath(document, op_id) ==
                  nullptr,
              "stage9: undo invalidates the cached toolpath")) {
    return false;
  }

  // Redo re-applies the status; the derived path is gone (cache is
  // memory-only) so the dependency pass correctly demands a
  // regenerate — the honest parametric behavior.
  document = manager.redo();
  if (!expect(find_status(document) == "needs_regenerate",
              "stage9: redo restores generated state, pass asks to regenerate")) {
    return false;
  }
  return true;
}

struct Stage {
  int number;
  const char* name;
  bool (*run)();
};

const Stage kStages[] = {
    {1, "empty-stack contract + payload flags", stage1_empty_stack_contract},
    {2, "undo/redo refresh pipeline", stage2_refresh_pipeline},
    {3, "extrude panel session is one step", stage3_extrude_panel_session},
    {4, "plane preview keeps the redo branch", stage4_plane_preview_keeps_redo},
    {5, "projection remove is one entry", stage5_projection_remove_single_entry},
    {6, "timeline cursor scrub", stage6_timeline_scrub},
    {7, "2D session semantics", stage7_session_semantics},
    {8, "named steps + depth limit + multi-undo", stage8_names_limit_multi},
    {9, "CAM generate undoable + invalidation", stage9_cam_generate},
};

}  // namespace

int main(int argc, char** argv) {
  int only_stage = -1;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--list") {
      for (const auto& s : kStages) {
        std::cout << s.number << ": " << s.name << "\n";
      }
      return 0;
    }
    if (arg == "--stage" && i + 1 < argc) {
      only_stage = std::atoi(argv[++i]);
    }
  }

  try {
    int failed = 0;
    for (const auto& s : kStages) {
      if (only_stage != -1 && s.number != only_stage) continue;
      std::cout << "stage " << s.number << " — " << s.name << "\n";
      if (!s.run()) {
        std::cerr << "stage " << s.number << " FAILED\n";
        failed++;
      } else {
        std::cout << "stage " << s.number << " passed\n";
      }
    }
    if (failed != 0) {
      std::cerr << failed << " stage(s) failed\n";
      return 1;
    }
    std::cout << "undo_stages_test passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
