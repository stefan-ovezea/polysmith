// Regression tests for the DXF import/export feature:
//   - import_dxf (ASCII DXF - new sketch on a reference plane)
//   - entity mapping: LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE (bulge-arc),
//     POINT, SPLINE (de Boor tessellation), ELLIPSE
//   - $INSUNITS inches - mm scaling
//   - unsupported/degenerate entities skipped + counted
//   - export_document_as_dxf (active sketch - ASCII DXF) + round-trip
//   - error paths: missing/garbage files, no active sketch
//   - save/load round-trip of an imported sketch.
//
// Fixtures are hand-assembled ASCII DXF strings written into the temp
// directory (R12-style group-code/value pairs), so every path here
// exercises the same libdxfrw reader the core uses.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/cam/cam_profile_reference.h"
#include "core/sketch/sketch_profile.h"
#include "core/viewport/viewport.h"
#include "dxf/dxf_export.h"
#include "dxf/dxf_import.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::SketchProfilePoint;
using polysmith::core::SketchProfileRegion;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) <= tolerance;
}

// ---------------------------------------------------------------------------
// DXF fixture assembly.
// ---------------------------------------------------------------------------

std::string dxf_header(int insunits = 4) {
  std::ostringstream s;
  s << "0\nSECTION\n2\nHEADER\n";
  if (insunits >= 0) {
    s << "9\n$INSUNITS\n70\n" << insunits << "\n";
  }
  s << "9\n$ACADVER\n1\nAC1015\n0\nENDSEC\n";
  s << "0\nSECTION\n2\nENTITIES\n";
  return s.str();
}

std::string dxf_footer() { return "0\nENDSEC\n0\nEOF\n"; }

std::string dxf_line(double x1, double y1, double x2, double y2) {
  std::ostringstream s;
  s << "0\nLINE\n8\n0\n10\n" << x1 << "\n20\n" << y1
    << "\n30\n0.0\n11\n" << x2 << "\n21\n" << y2 << "\n31\n0.0\n";
  return s.str();
}

std::string dxf_circle(double cx, double cy, double r) {
  std::ostringstream s;
  s << "0\nCIRCLE\n8\n0\n10\n" << cx << "\n20\n" << cy
    << "\n30\n0.0\n40\n" << r << "\n";
  return s.str();
}

// Angles in DEGREES (DXF group codes 50/51 are degrees on disk;
// libdxfrw converts to radians when parsing).
std::string dxf_arc(double cx, double cy, double r, double start_deg,
                    double end_deg) {
  std::ostringstream s;
  s << "0\nARC\n8\n0\n10\n" << cx << "\n20\n" << cy
    << "\n30\n0.0\n40\n" << r << "\n50\n" << start_deg
    << "\n51\n" << end_deg << "\n";
  return s.str();
}

std::string dxf_lwpolyline(const std::vector<std::pair<double, double>>& pts,
                           const std::vector<double>& bulges, int flags) {
  std::ostringstream s;
  s << "0\nLWPOLYLINE\n8\n0\n90\n" << pts.size() << "\n70\n" << flags
    << "\n38\n0.0\n";
  for (size_t i = 0; i < pts.size(); ++i) {
    s << "10\n" << pts[i].first << "\n20\n" << pts[i].second << "\n42\n"
      << (i < bulges.size() ? bulges[i] : 0.0) << "\n";
  }
  return s.str();
}

std::string dxf_polyline(const std::vector<std::pair<double, double>>& pts,
                         int flags) {
  std::ostringstream s;
  s << "0\nPOLYLINE\n8\n0\n66\n1\n70\n" << flags << "\n";
  for (const auto& pt : pts) {
    s << "0\nVERTEX\n8\n0\n10\n" << pt.first << "\n20\n" << pt.second
      << "\n30\n0.0\n";
  }
  s << "0\nSEQEND\n";
  return s.str();
}

std::string dxf_point(double x, double y) {
  std::ostringstream s;
  s << "0\nPOINT\n8\n0\n10\n" << x << "\n20\n" << y << "\n30\n0.0\n";
  return s.str();
}

// Cubic Bezier spline over the control polygon (0,0)-(10,0)-(10,10)-(0,10).
std::string dxf_spline() {
  std::ostringstream s;
  s << "0\nSPLINE\n8\n0\n71\n3\n72\n8\n73\n4\n74\n0\n";
  for (int i = 0; i < 4; ++i) s << "40\n0.0\n";
  for (int i = 0; i < 4; ++i) s << "40\n1.0\n";
  const double controls[4][3] = {{0.0, 0.0, 0.0},
                                 {10.0, 0.0, 0.0},
                                 {10.0, 10.0, 0.0},
                                 {0.0, 10.0, 0.0}};
  for (const auto& c : controls) {
    s << "10\n" << c[0] << "\n20\n" << c[1] << "\n30\n" << c[2] << "\n";
  }
  return s.str();
}

// Four control points spanning 0.4 mm: at 64 tessellation segments each step is
// about 0.006 mm, i.e. SMALLER than the sketch's 0.01 mm coincidence tolerance.
// That is the shape of a Fusion sketch export, and it is the case that used to
// abort the whole import.
std::string dxf_tiny_spline() {
  std::ostringstream s;
  s << "0\nSPLINE\n8\n0\n71\n3\n72\n8\n73\n4\n74\n0\n";
  for (int i = 0; i < 4; ++i) s << "40\n0.0\n";
  for (int i = 0; i < 4; ++i) s << "40\n1.0\n";
  const double controls[4][3] = {{0.0, 0.0, 0.0},
                                 {0.13, 0.0, 0.0},
                                 {0.13, 0.13, 0.0},
                                 {0.0, 0.26, 0.0}};
  for (const auto& c : controls) {
    s << "10\n" << c[0] << "\n20\n" << c[1] << "\n30\n" << c[2] << "\n";
  }
  return s.str();
}

std::string dxf_ellipse(double mx, double my, double ratio) {
  std::ostringstream s;
  s << "0\nELLIPSE\n8\n0\n10\n0.0\n20\n0.0\n30\n0.0\n11\n" << mx << "\n21\n"
    << my << "\n31\n0.0\n40\n" << ratio << "\n41\n0.0\n42\n"
    << 2.0 * M_PI << "\n";
  return s.str();
}

std::string dxf_text() {
  return "0\nTEXT\n8\n0\n10\n1.0\n20\n2.0\n30\n0.0\n40\n1.0\n1\nHi\n";
}

std::string dxf_insert() {
  return "0\nINSERT\n8\n0\n2\nblockname\n10\n0.0\n20\n0.0\n30\n0.0\n";
}

std::string write_fixture(const std::string& name, const std::string& content) {
  const auto path =
      (std::filesystem::temp_directory_path() / name).string();
  std::ofstream file(path, std::ios::binary);
  file << content;
  return path;
}

// Finds the active sketch feature's parameters on a DocumentState.
const polysmith::core::SketchFeatureParameters* active_sketch_params(
    const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.id == document.active_sketch_feature_id.value_or("") &&
        feature.kind == "sketch") {
      return &feature.sketch_parameters.value();
    }
  }
  return nullptr;
}

// Imported geometry must stay editable: every vertex it created should
// be UNfixed (projection locks derived points, import must not).
bool vertex_unfixed(const polysmith::core::SketchFeatureParameters& sketch,
                    const std::string& vertex_id) {
  for (const auto& vertex : sketch.vertices) {
    if (vertex.id == vertex_id) {
      return !vertex.is_fixed;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Tests.
// ---------------------------------------------------------------------------

// Regression fixture: a Fusion 360 sketch export (splines, polylines, lines,
// arcs).  Gated on the local project file, like the PCB real-board fixtures:
// the real export is what found the failure, and it may not exist on every
// machine.  Reported regression: importing it threw
// "Sketch lines must have non-zero length" and produced no sketch at all.
bool test_fusion_sketch_when_present() {
  const std::string path = "projects/Sketch3.dxf";
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    std::cout << "  (skipped: no fixture at " << path << ")\n";
    return true;
  }

  // Parse-only first: this isolates the reader from the document layer.
  const polysmith::core::DxfImportResult parsed =
      polysmith::core::import_dxf_geometry(path);
  std::cout << "  parsed lines=" << parsed.lines.size()
            << " arcs=" << parsed.arcs.size()
            << " circles=" << parsed.circles.size()
            << " points=" << parsed.points.size()
            << " skipped=" << parsed.skipped_count << "\n";

  double shortest = 1e9;
  for (const auto& line : parsed.lines) {
    const double length = std::hypot(line.x2 - line.x1, line.y2 - line.y1);
    shortest = std::min(shortest, length);
  }
  std::cout << "  shortest parsed line: " << shortest << " mm\n";

  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  try {
    document = manager.import_dxf(path);
  } catch (const std::exception& failure) {
    std::cerr << "  import threw: " << failure.what() << "\n";
    return expect(false, "a Fusion sketch export imports without throwing");
  }

  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "import created an active sketch")) {
    return false;
  }
  std::cout << "  sketch lines=" << sketch->lines.size()
            << " arcs=" << sketch->arcs.size()
            << " circles=" << sketch->circles.size() << "\n";
  bool ok = expect(!sketch->lines.empty(), "the sketch has geometry");
  // The sketch model merges points within 0.01 mm, so geometry finer than that
  // cannot all survive; the reader decimates tessellations to that spacing to
  // keep the loss small, and the import reports what was merged.  Assert the
  // honest contract: nothing throws, the curves are there, and the overwhelming
  // majority of segments arrive.
  const double arrived =
      parsed.lines.empty()
          ? 1.0
          : static_cast<double>(sketch->lines.size()) /
                static_cast<double>(parsed.lines.size());
  std::cout << "  " << sketch->lines.size() << " of " << parsed.lines.size()
            << " segments arrived (" << (arrived * 100.0) << "%)\n";
  ok &= expect(arrived > 0.95, "at least 95% of the parsed segments arrive");
  // Deterministic guard for the thing that made this sketch unusable: a fixed
  // 64-segment tessellation produced 12.5k entities for 191 short splines, and
  // the sketch refresh is superlinear in entity count (1k lines 0.2 s, 10.7k
  // 14 s).  Douglas-Peucker at 0.02 mm turns that into ~850.  Bounding the
  // count is what keeps the import editable; wall-clock assertions would be
  // flaky on other machines.
  ok &= expect(parsed.lines.size() < 2000,
               "the tessellation is simplified, not emitted point for point");
  // The app calls build_viewport_state on every refresh, so anything it throws
  // becomes one logged error PER REQUEST - which is exactly what the user saw
  // as a flood of "Sketch lines must have non-zero length" after the import.
  bool viewport_threw = false;
  std::string viewport_error;
  try {
    (void)polysmith::core::build_viewport_state(manager.get_document());
  } catch (const std::exception& failure) {
    viewport_threw = true;
    viewport_error = failure.what();
  }
  if (viewport_threw) {
    std::cerr << "  viewport threw: " << viewport_error << "\n";
  }
  ok &= expect(!viewport_threw,
               "the app's refresh path survives the imported sketch");

  // Where does the time go?  The user reports a 10.7k-line import that takes
  // ~15 s to delete a single line, so the cost has to be measured, not guessed.
  {
    using clock = std::chrono::steady_clock;
    const auto ms = [](clock::duration span) {
      return std::chrono::duration<double, std::milli>(span).count();
    };

    DocumentManager timing;
    timing.create_document();
    const auto parse_start = clock::now();
    const auto geometry = polysmith::core::import_dxf_geometry(path);
    const auto parse_done = clock::now();
    const auto import_start = clock::now();
    const DocumentState loaded = timing.import_dxf(path);
    const auto import_done = clock::now();

    const auto viewport_start = clock::now();
    (void)polysmith::core::build_viewport_state(timing.get_document());
    const auto viewport_done = clock::now();

    const std::string first_line_id = loaded.feature_history.back()
                                          .sketch_parameters->lines.front()
                                          .id;
    const auto delete_start = clock::now();
    (void)timing.delete_sketch_selection({first_line_id}, {}, {});
    const auto delete_done = clock::now();

    std::cout << "  timing: parse " << ms(parse_done - parse_start)
              << " ms (" << geometry.lines.size() << " lines parsed)\n"
              << "  timing: import " << ms(import_done - import_start)
              << " ms, viewport " << ms(viewport_done - viewport_start)
              << " ms, delete one line " << ms(delete_done - delete_start)
              << " ms\n";

    // Split the refresh: the vertex rebuild / solver pass versus profile
    // detection, which is the part that looks superlinear on a big sketch.
    DocumentState copy = timing.get_document().value();
    auto& feature = copy.feature_history.back();
    const auto refresh_start = clock::now();
    polysmith::core::refresh_sketch_derived_state(feature);
    const auto refresh_done = clock::now();
    const auto profiles_start = clock::now();
    const auto profiles = polysmith::core::detect_sketch_profiles(feature);
    const auto profiles_done = clock::now();
    std::cout << "  timing: refresh " << ms(refresh_done - refresh_start)
              << " ms, detect profiles " << ms(profiles_done - profiles_start)
              << " ms (" << profiles.polygons.size() << " polygons, "
              << profiles.circles.size() << " circles)\n";

    // detect_sketch_profiles returns the CACHED list; the refresh pays for the
    // REBUILD, and that is where exact splines get expensive (OCCT
    // intersections per curve pair).  Measure it on its own.
    const auto rebuild_start = clock::now();
    (void)polysmith::core::build_sketch_profile_regions(
        *feature.sketch_parameters);
    const auto rebuild_done = clock::now();
    std::cout << "  timing: rebuild profiles " << ms(rebuild_done - rebuild_start)
              << " ms (sketch: " << feature.sketch_parameters->lines.size()
              << " lines, " << feature.sketch_parameters->arcs.size()
              << " arcs, " << feature.sketch_parameters->splines.size()
              << " splines, " << feature.sketch_parameters->ellipses.size()
              << " ellipses)\n";

    // What a single selection costs the UI: the whole document state goes over
    // IPC on every selection change, so its size is the interaction budget.
    const auto payload_start = clock::now();
    const auto payload =
        polysmith::protocol::to_payload(timing.get_document().value());
    const auto payload_done = clock::now();
    std::cout << "  timing: document payload "
              << (payload.dump().size() / 1024) << " KB in "
              << ms(payload_done - payload_start) << " ms (sketch active)\n";

    // The interaction case that made the UI feel broken: selecting surfaces in
    // the model view, with NO sketch open.  Only the active sketch ships its
    // derived caches, so this is the payload a click costs.
    DocumentState idle = timing.get_document().value();
    idle.active_sketch_feature_id.reset();
    const auto idle_start = clock::now();
    const auto idle_payload = polysmith::protocol::to_payload(idle);
    const auto idle_done = clock::now();
    const size_t idle_kb = idle_payload.dump().size() / 1024;
    std::cout << "  timing: document payload with no sketch open " << idle_kb
              << " KB in " << ms(idle_done - idle_start) << " ms\n";
    ok &= expect(idle_kb < 400,
                 "the document payload stays small when no sketch is open");

    // Phase B: a select_* command replies with the selection fields only, so a
    // click costs bytes rather than a document.  Assert the shape is tiny and
    // actually carries the selection (a wrong payload here breaks every click).
    DocumentState selected = timing.get_document().value();
    selected.selected_sketch_profile_ids = {"polygon-1", "polygon-2"};
    const auto selection = polysmith::protocol::to_selection_payload(selected);
    const size_t selection_kb = selection.dump().size() / 1024;
    std::cout << "  selection payload: " << selection.dump().size()
              << " bytes\n";
    ok &= expect(selection_kb < 2, "a selection reply is not a document");
    ok &= expect(selection["selected_sketch_profile_ids"].size() == 2,
                 "the selection reply carries the selected profiles");

    // Every selection is followed by a viewport refetch in the UI, so that is
    // the other half of a click's cost.
    const auto viewport_payload_start = clock::now();
    const auto viewport_state =
        polysmith::core::build_viewport_state(timing.get_document());
    const auto viewport_payload =
        polysmith::protocol::to_payload(viewport_state);
    const auto viewport_payload_done = clock::now();
    std::cout << "  viewport payload: " << (viewport_payload.dump().size() / 1024)
              << " KB in " << ms(viewport_payload_done - viewport_payload_start)
              << " ms\n";
    // What is in it?  The payload is refetched after most edits, so its size is
    // the interaction budget for everything except a bare selection.
    for (auto it = viewport_payload.begin(); it != viewport_payload.end(); ++it) {
      const size_t kb = it.value().dump().size() / 1024;
      if (kb == 0) {
        continue;
      }
      std::cout << "  viewport[" << it.key() << "] = " << kb << " KB\n";
    }

    // REGRESSION (reported): with exact spline edges the extrude refused every
    // spline-bounded region with "10 pairs of holes touch (gap 0.000 mm)" — the
    // hole-separation check sampled spline edges as circular arcs with centre
    // and radius 0, collapsing them to the origin.  Extruding the largest
    // region must now succeed.
    {
      size_t biggest = 0;
      size_t biggest_points = 0;
      const auto& profiles = loaded.feature_history.back()
                                 .sketch_parameters->profiles;
      for (size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].points.size() > biggest_points) {
          biggest_points = profiles[i].points.size();
          biggest = i;
        }
      }
      if (biggest < profiles.size()) {
        DocumentManager extruder;
        extruder.create_document();
        (void)extruder.import_dxf(path);
        const std::string profile_id = profiles[biggest].id;
        const auto extrude_start = clock::now();
        bool refused = false;
        std::string message;
        try {
          (void)extruder.extrude_profiles({profile_id}, 1.0, "", std::nullopt,
                                          std::nullopt);
        } catch (const std::exception& error) {
          refused = true;
          message = error.what();
        }
        const auto extrude_done = clock::now();
        std::cout << "  extrude largest region: "
                  << (refused ? "refused" : "ok") << " in "
                  << ms(extrude_done - extrude_start) << " ms\n";
        if (refused) {
          std::cout << "    reason: " << message << "\n";
        }
        ok &= expect(!refused,
                     "a spline-bounded region extrudes (hole separation is "
                     "measured on real geometry, not at the origin)");
      }
    }

    // Reported: "each surface selection increases the reply from selection to
    // extrusion by one second".  Measure the two candidates at N profiles.
    for (const size_t count : {size_t{1}, size_t{4}, size_t{8}}) {
      if (feature.sketch_parameters->profiles.size() < count) {
        continue;
      }
      std::vector<std::string> ids;
      for (size_t i = 0; i < count; ++i) {
        ids.push_back(feature.sketch_parameters->profiles[i].id);
      }
      DocumentState doc_copy = timing.get_document().value();
      polysmith::core::CamOperation op;
      const auto capture_start = clock::now();
      const bool captured =
          polysmith::core::capture_profile_references_from_profile_ids(
              doc_copy, ids, op);
      const auto capture_done = clock::now();
      std::cout << "  capture " << count << " profile(s): "
                << ms(capture_done - capture_start) << " ms (ok=" << captured
                << ")\n";
    }

    // Per-key breakdown: WHICH part of the document is the payload?  Any
    // interaction fix has to target the big one, not the whole.
    for (auto it = payload.begin(); it != payload.end(); ++it) {
      std::cout << "  payload[" << it.key() << "] = "
                << (it.value().dump().size() / 1024) << " KB\n";
    }
    for (const auto& serialized :
         payload.value("feature_history", nlohmann::json::array())) {
      const auto params = serialized.value("sketch_parameters", nlohmann::json());
      if (params.is_null()) {
        continue;
      }
      std::cout << "  sketch payload keys:";
      for (auto it = params.begin(); it != params.end(); ++it) {
        std::cout << " " << it.key() << "=" << (it.value().dump().size() / 1024)
                  << "KB";
      }
      std::cout << "\n";
    }

    // Largest regions by vertex count: is profile detection producing sensible
    // regions, or 48 slivers between crossing curves?
    std::vector<size_t> region_sizes;
    for (const auto& polygon : profiles.polygons) {
      region_sizes.push_back(polygon.points.size());
    }
    std::sort(region_sizes.rbegin(), region_sizes.rend());
    std::cout << "  regions: " << region_sizes.size() << ", biggest vertex counts:";
    for (size_t i = 0; i < std::min<size_t>(region_sizes.size(), 6); ++i) {
      std::cout << " " << region_sizes[i];
    }
    std::cout << "\n";

    // Scaling probe: is the refresh linear or quadratic in the entity count?
    // That decides whether a cheaper import (fewer entities) is a real fix.
    for (const size_t keep : {static_cast<size_t>(3000)}) {
      DocumentState trimmed = timing.get_document().value();
      auto& trim_feature = trimmed.feature_history.back();
      trim_feature.sketch_parameters->lines.resize(keep);
      const auto start = clock::now();
      polysmith::core::refresh_sketch_derived_state(trim_feature);
      const auto done = clock::now();
      std::cout << "  timing: refresh with " << keep << " lines = "
                << ms(done - start) << " ms\n";
    }
  }
  return ok;
}

// A segment can be longer than the reader's minimum yet shorter than the
// sketch's coincidence tolerance (0.01 mm).  Both endpoints then snap onto the
// same existing vertex and the stored line would have zero length - which every
// later validation rejects.  It used to THROW, aborting the whole import; it
// must be dropped instead, with the rest of the sketch intact.
bool test_collapsed_segment_is_dropped_not_thrown() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);

  // 0.002 mm long, starting 0.004 mm from the first line's end: both endpoints
  // are inside the 0.01 mm coincidence window of (10, 0), so both snap onto
  // that vertex and the segment has zero length in the document.
  bool threw = false;
  try {
    manager.add_sketch_line(10.004, 0.002, 10.004, 0.004);
  } catch (const std::exception&) {
    threw = true;
  }
  bool ok = expect(!threw, "a collapsing segment does not throw");
  // get_document() returns BY VALUE: hold the copy, never a reference into it.
  const DocumentState snapshot = manager.get_document().value();
  const auto* sketch = active_sketch_params(snapshot);
  if (!expect(sketch != nullptr, "the sketch survived")) {
    return false;
  }
  ok &= expect(sketch->lines.size() == 1,
               "the collapsed segment was not added, the original line is");
  return ok;
}

// The reported failure, in miniature and with no fixture file: a spline whose
// tessellation steps are finer than the sketch's coincidence tolerance used to
// collapse on the way in and throw, aborting the whole import.  It must import,
// and the curve must survive at the sketch model's resolution.
bool test_sub_tolerance_tessellation_does_not_abort_import() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_tiny_spline() << dxf_footer();
  const std::string path =
      write_fixture("dxf_tiny_spline.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  bool threw = false;
  DocumentState document;
  try {
    document = manager.import_dxf(path);
  } catch (const std::exception& failure) {
    threw = true;
    std::cout << "  threw: " << failure.what() << "\n";
  }
  bool ok = expect(!threw, "a sub-tolerance spline does not abort the import");
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "a sketch was created")) {
    return false;
  }
  std::cout << "  tiny spline: " << sketch->splines.size() << " spline(s), "
            << sketch->lines.size() << " line(s)\n";
  // The curve must survive in SOME form: as an exact spline entity (the normal
  // path now) or as segments (the fallback for an unrepresentable curve).  The
  // regression this guards is the import ABORTING, not the entity kind.
  ok &= expect(!sketch->splines.empty() || sketch->lines.size() >= 2,
               "the curve survives at the sketch model's resolution");
  return ok;
}

bool test_import_lines_circles_arcs() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_line(0.0, 0.0, 10.0, 0.0)
      << dxf_circle(5.0, 5.0, 2.5) << dxf_arc(0.0, 10.0, 5.0, 0.0, 90.0)
      << dxf_footer();
  const std::string path = write_fixture("dxf_basic.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);

  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "import created no active sketch")) {
    return false;
  }
  if (!expect(sketch->lines.size() == 1, "expected 1 line")) return false;
  if (!expect(sketch->circles.size() == 1, "expected 1 circle")) return false;
  if (!expect(sketch->arcs.size() == 1, "expected 1 arc")) return false;
  if (!expect(near(sketch->lines[0].start_x, 0.0) &&
                  near(sketch->lines[0].end_x, 10.0) &&
                  near(sketch->lines[0].start_y, 0.0),
              "line coords wrong")) {
    return false;
  }
  if (!expect(near(sketch->circles[0].center_x, 5.0) &&
                  near(sketch->circles[0].center_y, 5.0) &&
                  near(sketch->circles[0].radius, 2.5),
              "circle coords wrong")) {
    return false;
  }
  const auto& arc = sketch->arcs[0];
  if (!expect(near(arc.center_x, 0.0) && near(arc.center_y, 10.0) && near(arc.radius, 5.0),
              "arc center/radius wrong")) {
    return false;
  }
  // DXF arcs sweep CCW from 0- to 90-: start on +x, end on +y.
  if (!expect(near(arc.start_x, 5.0) && near(arc.start_y, 10.0), "arc start wrong")) {
    return false;
  }
  if (!expect(near(arc.end_x, 0.0) && near(arc.end_y, 15.0), "arc end wrong")) {
    return false;
  }
  if (!expect(arc.ccw, "arc should be ccw")) return false;
  if (!expect(document.active_sketch_tool == "select",
              "active tool not select")) {
    return false;
  }
  // Imported geometry is editable: no fixed flags anywhere.
  if (!expect(vertex_unfixed(*sketch, sketch->lines[0].start_vertex_id) &&
                  vertex_unfixed(*sketch, sketch->lines[0].end_vertex_id) &&
                  vertex_unfixed(*sketch,
                                 sketch->circles[0].center_vertex_id) &&
                  vertex_unfixed(*sketch, arc.start_vertex_id) &&
                  vertex_unfixed(*sketch, arc.end_vertex_id),
              "imported vertices should not be fixed")) {
    return false;
  }
  for (const auto& feature : document.feature_history) {
    if (feature.id == document.active_sketch_feature_id.value_or("")) {
      if (!expect(feature.name == "DXF Import", "feature name wrong")) {
        return false;
      }
    }
  }
  return true;
}

// must still import, as tessellated lines - the fallback has to keep working,

bool test_import_insunits_inches() {
  std::ostringstream dxf;
  dxf << dxf_header(/*insunits=*/1) << dxf_line(0.0, 0.0, 1.0, 0.0)
      << dxf_footer();
  const std::string path = write_fixture("dxf_inches.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr && sketch->lines.size() == 1,
              "inches import missing line")) {
    return false;
  }
  // 1 inch - 25.4 mm.
  return expect(near(sketch->lines[0].end_x, 25.4) &&
                    near(sketch->lines[0].start_x, 0.0),
                "inches scaling wrong");
}

bool test_lwpolyline_bulge_arc() {
  // Two vertices 10 apart with bulge 1 = semicircle: radius 5, center
  // at the chord midpoint.
  std::ostringstream dxf;
  dxf << dxf_header()
      << dxf_lwpolyline({{0.0, 0.0}, {10.0, 0.0}}, {1.0}, /*flags=*/0)
      << dxf_footer();
  const std::string path = write_fixture("dxf_bulge_pos.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "bulge import no sketch")) return false;
  if (!expect(sketch->lines.empty(), "bulge should not make lines")) {
    return false;
  }
  if (!expect(sketch->arcs.size() == 1, "bulge should make 1 arc")) {
    return false;
  }
  const auto& arc = sketch->arcs[0];
  if (!expect(near(arc.radius, 5.0) && near(arc.center_x, 5.0) && near(arc.center_y, 0.0),
              "positive bulge arc geometry wrong")) {
    return false;
  }
  if (!expect(arc.ccw, "positive bulge should be ccw")) return false;

  // Negative bulge: same semicircle, clockwise.
  std::ostringstream dxf2;
  dxf2 << dxf_header()
       << dxf_lwpolyline({{0.0, 0.0}, {10.0, 0.0}}, {-1.0}, /*flags=*/0)
       << dxf_footer();
  const std::string path2 = write_fixture("dxf_bulge_neg.dxf", dxf2.str());
  DocumentManager manager2;
  manager2.create_document();
  const DocumentState document2 = manager2.import_dxf(path2);
  const auto* sketch2 = active_sketch_params(document2);
  if (!expect(sketch2 != nullptr && sketch2->arcs.size() == 1,
              "negative bulge import failed")) {
    return false;
  }
  if (!expect(near(sketch2->arcs[0].radius, 5.0) &&
                  near(sketch2->arcs[0].center_x, 5.0) &&
                  near(sketch2->arcs[0].center_y, 0.0),
              "negative bulge arc geometry wrong")) {
    return false;
  }
  return expect(!sketch2->arcs[0].ccw, "negative bulge should be cw");
}

bool test_closed_lwpolyline() {
  std::ostringstream dxf;
  dxf << dxf_header()
      << dxf_lwpolyline({{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}}, {},
                        /*flags=*/1)
      << dxf_footer();
  const std::string path = write_fixture("dxf_closed_lw.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "closed lwpolyline no sketch")) return false;
  // 3 segments: (0,0)-(10,0), (10,0)-(0,10), closing (0,10)-(0,0).
  return expect(sketch->lines.size() == 3, "closed lwpolyline segment count");
}

bool test_polyline_legacy_closed() {
  std::ostringstream dxf;
  dxf << dxf_header()
      << dxf_polyline({{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}},
                      /*flags=*/1)
      << dxf_footer();
  const std::string path = write_fixture("dxf_legacy_poly.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "legacy polyline no sketch")) return false;
  return expect(sketch->lines.size() == 3, "legacy polyline segment count");
}

bool test_spline_imports_as_exact_spline() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_spline() << dxf_footer();
  const std::string path = write_fixture("dxf_spline.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "spline import no sketch")) return false;
  // NO FACETING: a clamped open-uniform spline is representable by the sketch's
  // own spline entity, so it must arrive as ONE curve - not as tessellated
  // lines.  Tessellating is what produced faceted extrusions.
  if (!expect(sketch->lines.empty(),
              "an exactly representable spline produces no tessellated lines")) {
    return false;
  }
  if (!expect(sketch->splines.size() == 1, "the spline imports as one entity")) {
    return false;
  }
  const auto& spline = sketch->splines.front();
  if (!expect(spline.degree == 3, "imported spline keeps its degree")) {
    return false;
  }
  if (!expect(spline.pole_xs.size() == 4,
              "imported spline keeps all control points")) {
    return false;
  }
  // End points of a clamped B-spline are its end poles: (0,0) - (0,10).
  if (!expect(near(spline.pole_xs.front(), 0.0) &&
                  near(spline.pole_ys.front(), 0.0),
              "spline start wrong")) {
    return false;
  }
  return expect(near(spline.pole_xs.back(), 0.0) &&
                    near(spline.pole_ys.back(), 10.0),
                "spline end wrong");
}

// NO-FACETING CONTRACT.  A closed region bounded by an imported spline must
// keep a SPLINE boundary edge in profile detection - that edge is what the
// sketch wire builder turns into a Geom_BSplineCurve, i.e. what makes the
// extruded surface smooth.  If this ever reports "line" edges instead, the
// importer started tessellating and the user gets faceted surfaces.
bool test_spline_profile_keeps_curve() {
  std::ostringstream dxf;
  // The cubic from (0,0) to (0,10) bulging right, closed by a straight line.
  dxf << dxf_header() << dxf_spline() << dxf_line(0.0, 10.0, 0.0, 0.0)
      << dxf_footer();
  const std::string path = write_fixture("dxf_spline_profile.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "spline profile no sketch")) return false;
  if (!expect(sketch->splines.size() == 1,
              "the spline is an exact entity before profiling")) {
    return false;
  }
  if (!expect(!sketch->profiles.empty(), "a region is detected")) return false;
  bool found_spline_edge = false;
  for (const auto& region : sketch->profiles) {
    for (const auto& edge : region.boundary_edges) {
      if (edge.entity_kind == "spline" && !edge.spline_pole_xs.empty()) {
        found_spline_edge = true;
      }
    }
  }
  return expect(found_spline_edge,
                "the region keeps a spline edge with its poles, not a "
                "tessellated run");
}

// Diagnostics on a SAVED sketch the user is trying to select surfaces in.
//
// Reported symptom: individual surfaces inside the big circle select and extrude
// fine, but clicking the region BETWEEN them ("the circle surface") also selects
// the areas inside surfaces that look closed.  That is what a region looks like
// when its inner holes were not attached — the outer disc swallows them.  So:
// find open chains, sub-tolerance gaps, degenerate entities and nested regions.
bool test_saved_sketch_diagnostics() {
  const std::string path = "projects/Sketch4.polysmith";
  if (!std::filesystem::exists(path)) {
    std::cout << "  (no " << path << ", skipping)\n";
    return true;
  }
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.load_document_from_path(path);
  // After a load no sketch is "active", so find the sketch feature directly.
  const polysmith::core::SketchFeatureParameters* sketch = nullptr;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      sketch = &feature.sketch_parameters.value();
    }
  }
  if (!expect(sketch != nullptr, "the saved document has a sketch")) return false;

  std::cout << "  entities: " << sketch->lines.size() << " lines, "
            << sketch->arcs.size() << " arcs, " << sketch->circles.size()
            << " circles, " << sketch->splines.size() << " splines, "
            << sketch->ellipses.size() << " ellipses\n";
  std::cout << "  regions: " << sketch->profiles.size() << ", inner loops: ";
  size_t total_inner = 0;
  for (const auto& region : sketch->profiles) {
    total_inner += region.inner_loops.size();
  }
  std::cout << total_inner << "\n";

  // Endpoint inventory: how many endpoints are NOT shared by another entity?
  const auto key = [](double x, double y) {
    // 1e-3 mm grid — coarse enough to treat "the same point" as the same key.
    return std::make_pair(std::llround(x * 1000.0), std::llround(y * 1000.0));
  };
  std::map<std::pair<long long, long long>, int> endpoint_uses;
  // Which entity owns each end: an open end must be NAMED, not guessed at.
  std::map<std::pair<long long, long long>, std::vector<std::string>>
      endpoint_owners;
  const auto add_endpoint = [&](double x, double y, const std::string& owner) {
    const auto at = key(x, y);
    endpoint_uses[at] += 1;
    endpoint_owners[at].push_back(owner);
  };
  for (const auto& line : sketch->lines) {
    add_endpoint(line.start_x, line.start_y, "line:" + line.id);
    add_endpoint(line.end_x, line.end_y, "line:" + line.id);
  }
  for (const auto& arc : sketch->arcs) {
    add_endpoint(arc.start_x, arc.start_y, "arc:" + arc.id);
    add_endpoint(arc.end_x, arc.end_y, "arc:" + arc.id);
  }
  for (const auto& spline : sketch->splines) {
    if (!spline.pole_xs.empty()) {
      add_endpoint(spline.pole_xs.front(), spline.pole_ys.front(),
                   "spline:" + spline.id);
      add_endpoint(spline.pole_xs.back(), spline.pole_ys.back(),
                   "spline:" + spline.id);
    }
  }
  size_t dangling = 0;
  for (const auto& [point, uses] : endpoint_uses) {
    if (uses == 1) {
      ++dangling;
    }
  }
  std::cout << "  distinct endpoints: " << endpoint_uses.size()
            << ", used once (dangling): " << dangling << "\n";

  // Near-miss pairs: endpoints within 0.05 mm that are NOT the same point.
  // Those are the gaps that stop a chain from closing.
  struct Point {
    double x, y;
  };
  std::vector<Point> single_points;
  for (const auto& [point, uses] : endpoint_uses) {
    if (uses == 1) {
      single_points.push_back({point.first / 1000.0, point.second / 1000.0});
    }
  }
  size_t near_misses = 0;
  double smallest_gap = 1e9;
  for (size_t a = 0; a < single_points.size(); ++a) {
    for (size_t b = a + 1; b < single_points.size(); ++b) {
      const double distance = std::hypot(single_points[a].x - single_points[b].x,
                                         single_points[a].y - single_points[b].y);
      if (distance > 1e-9 && distance < 0.05) {
        ++near_misses;
        smallest_gap = std::min(smallest_gap, distance);
      }
    }
  }
  std::cout << "  near-miss endpoint pairs (<0.05 mm, not welded): "
            << near_misses;
  if (near_misses > 0) {
    std::cout << ", smallest gap " << smallest_gap << " mm";
  }
  std::cout << "\n";

  // Degenerate entities.
  size_t zero_length = 0;
  for (const auto& line : sketch->lines) {
    if (std::hypot(line.end_x - line.start_x, line.end_y - line.start_y) <=
        0.001) {
      ++zero_length;
    }
  }
  std::cout << "  zero-length lines: " << zero_length << "\n";
  // Where exactly are the open ends?  Nothing here is a sub-tolerance gap, so
  // these are real open chains (curves whose neighbour is missing) and the user
  // needs their locations to judge what got deleted.
  for (const auto& [point, uses] : endpoint_uses) {
    if (uses != 1) {
      continue;
    }
    const double px = point.first / 1000.0;
    const double py = point.second / 1000.0;
    double nearest = 1e9;
    for (const auto& [other, other_uses] : endpoint_uses) {
      if (other_uses != 1 || other == point) continue;
      nearest = std::min(nearest,
                         std::hypot(px - other.first / 1000.0,
                                    py - other.second / 1000.0));
    }
    std::cout << "  open end at " << px << ", " << py << " r="
              << std::hypot(px, py) << " owner=";
    const auto owners = endpoint_owners.find(point);
    if (owners != endpoint_owners.end()) {
      for (size_t i = 0; i < owners->second.size(); ++i) {
        std::cout << (i > 0 ? "," : "") << owners->second[i];
      }
    }
    std::cout << " (nearest open end " << nearest << " mm)\n";
  }

  // Nested regions: is one region's outline inside another region that does NOT
  // list it as a hole?  That is the "selects inside other surfaces" case.
  const auto bbox_of = [](const SketchProfileRegion& region) {
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const auto& point : region.points) {
      min_x = std::min(min_x, point.x);
      max_x = std::max(max_x, point.x);
      min_y = std::min(min_y, point.y);
      max_y = std::max(max_y, point.y);
    }
    return std::array<double, 4>{min_x, min_y, max_x, max_y};
  };
  size_t nested_without_hole = 0;
  for (size_t a = 0; a < sketch->profiles.size(); ++a) {
    const auto outer = bbox_of(sketch->profiles[a]);
    for (size_t b = 0; b < sketch->profiles.size(); ++b) {
      if (a == b) continue;
      const auto inner = bbox_of(sketch->profiles[b]);
      const bool inside =
          inner[0] > outer[0] + 0.01 && inner[1] > outer[1] + 0.01 &&
          inner[2] < outer[2] - 0.01 && inner[3] < outer[3] - 0.01;
      if (!inside) continue;
      // Is b one of a's holes?  Compare the inner loop centroid to b's centre.
      bool is_hole = false;
      for (const auto& loop : sketch->profiles[a].inner_loops) {
        if (loop.empty()) continue;
        double cx = 0, cy = 0;
        for (const auto& point : loop) {
          cx += point.x;
          cy += point.y;
        }
        cx /= static_cast<double>(loop.size());
        cy /= static_cast<double>(loop.size());
        if (cx > inner[0] - 0.5 && cx < inner[2] + 0.5 && cy > inner[1] - 0.5 &&
            cy < inner[3] + 0.5) {
          is_hole = true;
          break;
        }
      }
      if (!is_hole) {
        ++nested_without_hole;
        if (nested_without_hole <= 6) {
          std::cout << "  region " << b << " sits inside region " << a
                    << " without being one of its holes\n";
        }
      }
    }
  }
  std::cout << "  nested regions without a hole: " << nested_without_hole
            << "\n";

  // Concentric-band diagnosis.  The user has two long arcs (dia 51.56, so r ~
  // 25.78) split by a gap and closed with lines, plus concentric arcs, forming
  // annular bands.  One band selects, the neighbouring one does not — so list
  // the arcs by radius and then every region with its edges, area and bbox.
  std::cout << "  arcs by radius:\n";
  for (const auto& arc : sketch->arcs) {
    if (arc.radius < 20.0) {
      continue;  // only the outer bands matter here
    }
    std::cout << "    r=" << arc.radius << " start=(" << arc.start_x << ","
              << arc.start_y << ") end=(" << arc.end_x << "," << arc.end_y
              << ") ccw=" << (arc.ccw ? 1 : 0) << " id=" << arc.id << "\n";
  }
  for (const auto& circle : sketch->circles) {
    if (circle.radius < 20.0) {
      continue;
    }
    std::cout << "    circle r=" << circle.radius << " at (" << circle.center_x
              << "," << circle.center_y << ")\n";
  }

  std::cout << "  regions (area, edges, kinds):\n";
  for (size_t i = 0; i < sketch->profiles.size(); ++i) {
    const auto& region = sketch->profiles[i];
    // Shoelace area of the sampled outline.
    double area = 0.0;
    const auto& points = region.points;
    for (size_t p = 0; p + 1 < points.size(); ++p) {
      area += points[p].x * points[p + 1].y - points[p + 1].x * points[p].y;
    }
    area = std::abs(area) * 0.5;
    size_t lines = 0, arcs = 0, splines = 0, others = 0;
    for (const auto& edge : region.boundary_edges) {
      if (edge.entity_kind == "line") ++lines;
      else if (edge.entity_kind == "arc" || edge.entity_kind == "circle") ++arcs;
      else if (edge.entity_kind == "spline") ++splines;
      else ++others;
    }
    const auto box = bbox_of(region);
    if (area < 5.0) {
      continue;  // skip the tiny decorative regions
    }
    std::cout << "    region " << i << ": area=" << area
              << " holes=" << region.inner_loops.size() << " edges="
              << region.boundary_edges.size() << " (line " << lines << ", arc "
              << arcs << ", spline " << splines << ", other " << others
              << ") bbox=[" << box[0] << "," << box[1] << " .. " << box[2]
              << "," << box[3] << "]\n";
  }

  // Which region CONTAINS a given point?  This is what a click does.  Probe the
  // band between r=42.5 and r=45 (the concentric circle pair) and report the
  // smallest region that contains each sample, holes honoured — i.e. exactly the
  // UI's pick rule.
  const auto point_in_loop = [](double x, double y,
                                const std::vector<SketchProfilePoint>& loop) {
    bool inside = false;
    for (size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++) {
      if (((loop[i].y > y) != (loop[j].y > y)) &&
          (x < (loop[j].x - loop[i].x) * (y - loop[i].y) /
                           (loop[j].y - loop[i].y) +
                       loop[i].x)) {
        inside = !inside;
      }
    }
    return inside;
  };
  std::cout << "  band probes (r=43.75, every 30 deg):\n";
  for (int step = 0; step < 12; ++step) {
    const double angle = step * (2.0 * 3.14159265358979 / 12.0);
    const double x = 43.75 * std::cos(angle);
    const double y = 43.75 * std::sin(angle);
    std::string hits;
    double smallest_area = 1e18;
    int smallest_id = -1;
    for (size_t i = 0; i < sketch->profiles.size(); ++i) {
      const auto& region = sketch->profiles[i];
      if (!point_in_loop(x, y, region.points)) {
        continue;
      }
      bool in_hole = false;
      for (const auto& loop : region.inner_loops) {
        if (!loop.empty() && point_in_loop(x, y, loop)) {
          in_hole = true;
          break;
        }
      }
      if (in_hole) {
        continue;
      }
      hits += (hits.empty() ? "" : ",") + std::to_string(i);
      double area = 0.0;
      for (size_t p = 0; p + 1 < region.points.size(); ++p) {
        area += region.points[p].x * region.points[p + 1].y -
                region.points[p + 1].x * region.points[p].y;
      }
      area = std::abs(area) * 0.5;
      if (area < smallest_area) {
        smallest_area = area;
        smallest_id = static_cast<int>(i);
      }
    }
    std::cout << "    (" << x << "," << y << ") -> regions [" << hits
              << "] pick=" << smallest_id << "\n";
  }

  // The user marked the failing arc by drawing a line from the origin to one of
  // its ends.  Find that line, then probe the surface on BOTH sides of the arc
  // it touches: if one side belongs to no region, the face was never built.
  const auto probe = [&](double x, double y) {
    std::string hits;
    for (size_t i = 0; i < sketch->profiles.size(); ++i) {
      const auto& region = sketch->profiles[i];
      if (!point_in_loop(x, y, region.points)) continue;
      bool in_hole = false;
      for (const auto& loop : region.inner_loops) {
        if (!loop.empty() && point_in_loop(x, y, loop)) {
          in_hole = true;
          break;
        }
      }
      if (!in_hole) hits += (hits.empty() ? "" : ",") + std::to_string(i);
    }
    return hits;
  };
  for (const auto& line : sketch->lines) {
    const double start_r = std::hypot(line.start_x, line.start_y);
    const double end_r = std::hypot(line.end_x, line.end_y);
    if (std::min(start_r, end_r) > 0.5 || std::max(start_r, end_r) < 20.0) {
      continue;  // want the user's marker: one end at the origin, one far out
    }
    const double far_x = start_r > end_r ? line.start_x : line.end_x;
    const double far_y = start_r > end_r ? line.start_y : line.end_y;
    std::cout << "  marker line " << line.id << " ends at (" << far_x << ","
              << far_y << ") r=" << std::hypot(far_x, far_y) << "\n";
    // Nearest arc end to the marker.
    const polysmith::core::SketchArc* touch = nullptr;
    double best = 1e9;
    for (const auto& arc : sketch->arcs) {
      for (const auto dx : {arc.start_x, arc.end_x}) {
        (void)dx;
      }
      const double d1 =
          std::hypot(arc.start_x - far_x, arc.start_y - far_y);
      const double d2 = std::hypot(arc.end_x - far_x, arc.end_y - far_y);
      const double d = std::min(d1, d2);
      if (d < best) {
        best = d;
        touch = &arc;
      }
    }
    if (touch == nullptr) {
      std::cout << "    no arc near the marker end\n";
      continue;
    }
    std::cout << "    nearest arc " << touch->id << " r=" << touch->radius
              << " centre=(" << touch->center_x << "," << touch->center_y
              << ") gap=" << best << " mm\n";
    // Probe on both sides of the ring RIGHT AT the marker end (where the user
    // actually clicks), not at the arc's parametric midpoint.
    const double mark_angle = std::atan2(far_y, far_x);
    for (const double offset : {-0.2, -0.05, 0.05, 0.2}) {
      const double radius = touch->radius + offset;
      const double x = radius * std::cos(mark_angle);
      const double y = radius * std::sin(mark_angle);
      std::cout << "    marker-side probe (" << x << "," << y << ") r=" << radius
                << " -> regions [" << probe(x, y) << "]\n";
    }
    // Detail on the two regions that own that area, so the UI pick can be
    // compared against what the core actually built.
    for (const auto& region : sketch->profiles) {
      if (region.id.empty()) continue;
      const auto box = bbox_of(region);
      const bool owns = point_in_loop(-3.63124, 25.4709, region.points) ||
                        point_in_loop(-3.64536, 25.5699, region.points);
      if (!owns) continue;
      std::cout << "    owning region " << region.id << ": points="
                << region.points.size() << " holes=" << region.inner_loops.size()
                << " edges=" << region.boundary_edges.size() << " bbox=["
                << box[0] << "," << box[1] << " .. " << box[2] << ","
                << box[3] << "]\n";
    }
  }
  // The user's failing surface: TWO CONCENTRIC ARCS closed by two lines at their
  // ends — an annular band.  Find the concentric arc pair around the origin, the
  // two closing lines, and probe the middle of the band.  If nothing contains
  // that point, the band face was never built.
  {
    std::vector<double> radii;
    for (const auto& arc : sketch->arcs) {
      if (std::hypot(arc.center_x, arc.center_y) > 0.01) continue;
      bool known = false;
      for (const double r : radii) {
        if (std::abs(r - arc.radius) < 0.01) known = true;
      }
      if (!known) radii.push_back(arc.radius);
    }
    std::sort(radii.begin(), radii.end());
    std::cout << "  concentric arc radii around origin:";
    for (const double r : radii) std::cout << " " << r;
    std::cout << "\n";
    if (radii.size() >= 2) {
      for (size_t i = 0; i + 1 < radii.size(); ++i) {
        const double r_inner = radii[i];
        const double r_outer = radii[i + 1];
        const double mid_radius = 0.5 * (r_inner + r_outer);
        // Closing lines: both endpoints land on the two circles.
        std::vector<double> angles;
        for (const auto& line : sketch->lines) {
          const double a_in = std::hypot(line.start_x, line.start_y);
          const double a_out = std::hypot(line.end_x, line.end_y);
          const bool inner_ok =
              std::abs(std::min(a_in, a_out) - r_inner) < 0.05;
          const bool outer_ok =
              std::abs(std::max(a_in, a_out) - r_outer) < 0.05;
          if (!inner_ok || !outer_ok) continue;
          const double far_x = a_in > a_out ? line.start_x : line.end_x;
          const double far_y = a_in > a_out ? line.start_y : line.end_y;
          angles.push_back(std::atan2(far_y, far_x));
        }
        std::cout << "  band " << r_inner << ".." << r_outer << ": closing lines="
                  << angles.size();
        if (!angles.empty()) {
          std::sort(angles.begin(), angles.end());
          // Probe EVERY side of the band: the user reports one half selectable
          // and the other not, so the widest span alone is not enough.
          for (size_t a = 0; a < angles.size(); ++a) {
            const double next =
                a + 1 < angles.size()
                    ? angles[a + 1]
                    : angles.front() + 2.0 * 3.14159265358979;
            const double span = next - angles[a];
            if (span < 0.02) continue;
            const double side_angle = angles[a] + 0.5 * span;
            const double x = mid_radius * std::cos(side_angle);
            const double y = mid_radius * std::sin(side_angle);
            std::cout << "\n    side span=" << span << " rad probe (" << x
                      << "," << y << ") -> regions [" << probe(x, y) << "]";
          }
        }
        std::cout << "\n";
        // Do the closing lines actually MEET the arcs?  If they stop short (or
      // cross), the sliver between them falls on the wrong side of the walk.
      // Report the raw ends for the innermost band, where the user's 3.3 deg
      // sliver lives.
      if (std::abs(r_inner - 25.7785) < 0.01) {
        std::cout << "    arc ends near the band:\n";
        for (const auto& arc : sketch->arcs) {
          if (std::hypot(arc.center_x, arc.center_y) > 0.01) continue;
          if (std::abs(arc.radius - r_inner) > 0.01 &&
              std::abs(arc.radius - r_outer) > 0.01) {
            continue;
          }
          std::cout << "      arc " << arc.id << " r=" << arc.radius
                    << " start=(" << arc.start_x << "," << arc.start_y
                    << ") a=" << std::atan2(arc.start_y, arc.start_x) * 180.0 /
                                       3.14159265358979
                    << "deg end=(" << arc.end_x << "," << arc.end_y
                    << ") a=" << std::atan2(arc.end_y, arc.end_x) * 180.0 /
                                       3.14159265358979
                    << "deg\n";
        }
        std::cout << "    closing line ends:\n";
        for (const auto& line : sketch->lines) {
          const double r1 = std::hypot(line.start_x, line.start_y);
          const double r2 = std::hypot(line.end_x, line.end_y);
          if (std::abs(std::min(r1, r2) - r_inner) > 0.05 ||
              std::abs(std::max(r1, r2) - r_outer) > 0.05) {
            continue;
          }
          std::cout << "      line " << line.id << " (" << line.start_x << ","
                    << line.start_y << ") r=" << r1 << " a="
                    << std::atan2(line.start_y, line.start_x) * 180.0 /
                           3.14159265358979
                    << "deg -> (" << line.end_x << "," << line.end_y
                    << ") r=" << r2 << " a="
                    << std::atan2(line.end_y, line.end_x) * 180.0 /
                           3.14159265358979
                    << "deg\n";
        }
      }
    }
  }
  }  // end of the concentric-band diagnostic block

  return true;
}

// must still import, as tessellated lines - the fallback has to keep working,
// and it is the only path that legitimately produces facets.
bool test_unrepresentable_spline_falls_back_to_lines() {
  std::ostringstream dxf;
  // Degree 2 with 5 poles: the model's degree would be min(3, 5-1) = 3, i.e. a
  // different curve, so this is not exactly representable.
  dxf << dxf_header()
      << "0\nSPLINE\n8\n0\n70\n8\n71\n2\n72\n8\n73\n5\n"
         "40\n0\n40\n0\n40\n0\n40\n1\n40\n2\n40\n3\n40\n3\n40\n3\n"
         "10\n0\n20\n0\n30\n0\n10\n1\n20\n2\n30\n0\n10\n3\n20\n0\n30\n0\n"
         "10\n4\n20\n2\n30\n0\n10\n5\n20\n0\n30\n0\n0\nENDSEC\n0\nEOF\n";
  const std::string path = write_fixture("dxf_spline_degree2.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "fallback spline no sketch")) return false;
  if (!expect(sketch->splines.empty(),
              "a non-representable spline is not imported as an entity")) {
    return false;
  }
  return expect(!sketch->lines.empty(),
                "the fallback still delivers the curve as segments");
}

bool test_near_axis_lines_import_exact() {
  // A line 0.005 off horizontal is within the H/V constraint hint
  // tolerance (0.01) - imported geometry must NOT be snapped to an
  // axis (regression: constraint inference deformed tessellated
  // splines/ellipses and near-axis DXF lines).
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_line(0.0, 0.0, 10.0, 0.005) << dxf_footer();
  const std::string path = write_fixture("dxf_near_axis.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr && sketch->lines.size() == 1,
              "near-axis import missing line")) {
    return false;
  }
  const auto& line = sketch->lines[0];
  if (!expect(near(line.start_x, 0.0) && near(line.start_y, 0.0) &&
                  near(line.end_x, 10.0) && near(line.end_y, 0.005),
              "near-axis line deformed")) {
    return false;
  }
  return expect(!line.constraint.has_value(),
                "near-axis line should carry no constraint");
}

bool test_ellipse_circular_and_elliptical() {
  // ratio 1.0 - circle of radius |major|.
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_ellipse(5.0, 0.0, 1.0) << dxf_footer();
  const std::string path = write_fixture("dxf_ell_circ.dxf", dxf.str());
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "circular ellipse no sketch")) return false;
  if (!expect(sketch->circles.size() == 1, "circular ellipse - 1 circle")) {
    return false;
  }
  if (!expect(near(sketch->circles[0].radius, 5.0) &&
                  near(sketch->circles[0].center_x, 0.0),
              "circular ellipse radius wrong")) {
    return false;
  }

  // ratio 0.5 - tessellated segments; bounding box -5 in x, -2.5 in y.
  std::ostringstream dxf2;
  dxf2 << dxf_header() << dxf_ellipse(5.0, 0.0, 0.5) << dxf_footer();
  const std::string path2 = write_fixture("dxf_ell_flat.dxf", dxf2.str());
  DocumentManager manager2;
  manager2.create_document();
  const DocumentState document2 = manager2.import_dxf(path2);
  const auto* sketch2 = active_sketch_params(document2);
  if (!expect(sketch2 != nullptr, "elliptical ellipse no sketch")) {
    return false;
  }
  // Douglas-Peucker (0.02 mm) keeps the count shape-dependent, so assert the
  // ellipse is a chained curve of several segments whose EXTENTS are right -
  // that is what matters, not a fixed 64.
  if (!expect(sketch2->lines.size() >= 8,
              "ellipse imports as a chain of segments")) {
    return false;
  }
  double min_x = 1e9;
  double max_x = -1e9;
  double min_y = 1e9;
  double max_y = -1e9;
  for (const auto& line : sketch2->lines) {
    min_x = std::min({min_x, line.start_x, line.end_x});
    max_x = std::max({max_x, line.start_x, line.end_x});
    min_y = std::min({min_y, line.start_y, line.end_y});
    max_y = std::max({max_y, line.start_y, line.end_y});
  }
  return expect(near(max_x, 5.0, 1e-3) && near(min_x, -5.0, 1e-3) &&
                    near(max_y, 2.5, 1e-3) && near(min_y, -2.5, 1e-3),
                "ellipse tessellation bounds wrong");
}

bool test_skipped_entities_counted() {
  std::ostringstream dxf;
  // TEXT + INSERT + non-planar LWPOLYLINE (elevation 5) + zero-length
  // LINE = 4 skipped entities, 0 imported.
  dxf << dxf_header() << dxf_text() << dxf_insert();
  dxf << "0\nLWPOLYLINE\n8\n0\n90\n2\n70\n0\n38\n5.0\n10\n0.0\n20\n0.0\n";
  dxf << "10\n10.0\n20\n0.0\n";
  dxf << dxf_line(1.0, 1.0, 1.0, 1.0);
  dxf << dxf_footer();
  const std::string path = write_fixture("dxf_skipped.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "skipped-entities import no sketch")) {
    return false;
  }
  if (!expect(sketch->lines.empty() && sketch->circles.empty() &&
                  sketch->arcs.empty() && sketch->projected_points.empty(),
              "skipped entities should import nothing")) {
    return false;
  }
  for (const auto& feature : document.feature_history) {
    if (feature.id == document.active_sketch_feature_id.value_or("")) {
      return expect(feature.parameters_summary.find("skipped 4") !=
                        std::string::npos,
                    "parameters_summary missing skipped count");
    }
  }
  return expect(false, "active sketch feature not found");
}

bool test_points_into_projected_points() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_point(1.0, 2.0) << dxf_point(3.0, 4.0)
      << dxf_line(0.0, 0.0, 5.0, 0.0) << dxf_footer();
  const std::string path = write_fixture("dxf_points.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "points import no sketch")) return false;
  if (!expect(sketch->projected_points.size() == 2,
              "expected 2 projected points")) {
    return false;
  }
  if (!expect(sketch->projected_points[0].source_id == "dxf:point:0" &&
                  sketch->projected_points[1].source_id == "dxf:point:1",
              "projected point source ids wrong")) {
    return false;
  }
  if (!expect(near(sketch->projected_points[1].x, 3.0) &&
                  near(sketch->projected_points[1].y, 4.0),
              "projected point coords wrong")) {
    return false;
  }

  // Imported points are editable too - no fix badges.
  for (const auto& point : sketch->projected_points) {
    if (!expect(vertex_unfixed(*sketch, point.id),
                "imported point should not be fixed")) {
      return false;
    }
  }

  // Moving the imported line forces a derived-state refresh; the
  // standalone points must survive the vertex rebuild.
  manager.update_sketch_line(sketch->lines[0].id, 0.0, 0.0, 7.0, 0.0);
  const DocumentState refreshed = *manager.get_document();
  const auto* sketch2 = active_sketch_params(refreshed);
  if (!expect(sketch2 != nullptr, "refresh lost active sketch")) return false;
  if (!expect(sketch2->projected_points.size() == 2,
              "points lost after refresh")) {
    return false;
  }
  // ... and stay unfixed after that refresh.
  for (const auto& point : sketch2->projected_points) {
    if (!expect(vertex_unfixed(*sketch2, point.id),
                "point re-fixed after refresh")) {
      return false;
    }
  }
  return true;
}

// must still import, as tessellated lines - the fallback has to keep working,

bool test_export_import_round_trip() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_line(0.0, 0.0, 10.0, 5.0)
      << dxf_circle(3.0, 4.0, 2.0) << dxf_arc(0.0, 10.0, 5.0, 0.0, 90.0)
      << dxf_point(1.0, 1.0) << dxf_footer();
  const std::string path = write_fixture("dxf_roundtrip_in.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_dxf(path);
  const auto* original = active_sketch_params(imported);
  if (!expect(original != nullptr, "round-trip import no sketch")) {
    return false;
  }

  const std::string export_path =
      (std::filesystem::temp_directory_path() / "dxf_roundtrip_out.dxf")
          .string();
  const auto result = manager.export_document_as_dxf(export_path);
  if (!expect(result.format == "dxf", "export format wrong")) return false;
  if (!expect(result.exported_feature_count == 4,
              "exported_feature_count wrong")) {
    return false;
  }
  if (!expect(std::filesystem::exists(export_path), "export file missing")) {
    return false;
  }

  // Re-import into a fresh manager and compare entity sets.
  DocumentManager manager2;
  manager2.create_document();
  const DocumentState reimported = manager2.import_dxf(export_path);
  const auto* round = active_sketch_params(reimported);
  if (!expect(round != nullptr, "re-import no sketch")) return false;
  if (!expect(round->lines.size() == original->lines.size() &&
                  round->circles.size() == original->circles.size() &&
                  round->arcs.size() == original->arcs.size() &&
                  round->projected_points.size() ==
                      original->projected_points.size(),
              "round-trip entity counts differ")) {
    return false;
  }
  if (!expect(near(round->lines[0].start_x, original->lines[0].start_x) &&
                  near(round->lines[0].start_y, original->lines[0].start_y) &&
                  near(round->lines[0].end_x, original->lines[0].end_x) &&
                  near(round->lines[0].end_y, original->lines[0].end_y),
              "round-trip line coords differ")) {
    return false;
  }
  if (!expect(near(round->circles[0].center_x, original->circles[0].center_x) &&
                  near(round->circles[0].center_y, original->circles[0].center_y) &&
                  near(round->circles[0].radius, original->circles[0].radius),
              "round-trip circle differs")) {
    return false;
  }
  return expect(
      near(round->arcs[0].center_x, original->arcs[0].center_x) &&
          near(round->arcs[0].center_y, original->arcs[0].center_y) &&
          near(round->arcs[0].radius, original->arcs[0].radius) &&
          near(round->projected_points[0].x, original->projected_points[0].x),
      "round-trip arc/point differs");
}

bool test_export_no_active_sketch() {
  DocumentManager manager;
  manager.create_document();
  const std::string export_path =
      (std::filesystem::temp_directory_path() / "dxf_no_sketch.dxf").string();
  try {
    manager.export_document_as_dxf(export_path);
  } catch (const std::runtime_error& error) {
    return expect(std::string(error.what()) == "No active sketch",
                  "wrong error message for no active sketch");
  }
  return expect(false, "export without active sketch did not throw");
}

bool test_import_missing_and_garbage_files() {
  DocumentManager manager;
  manager.create_document();
  try {
    manager.import_dxf("Z:/nonexistent/definitely_missing.dxf");
  } catch (const std::runtime_error& error) {
    if (!expect(std::string(error.what()).find("DXF file not found") !=
                    std::string::npos,
                "wrong missing-file error")) {
      return false;
    }
  }

  const std::string garbage_path =
      write_fixture("dxf_garbage.dxf", "this is definitely not a dxf file\n");
  try {
    manager.import_dxf(garbage_path);
  } catch (const std::runtime_error& error) {
    return expect(std::string(error.what()).find("Could not read DXF file") !=
                      std::string::npos,
                  "wrong garbage-file error");
  }
  return expect(false, "garbage import did not throw");
}

bool test_save_load_round_trip() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_line(0.0, 0.0, 10.0, 5.0)
      << dxf_circle(3.0, 4.0, 2.0) << dxf_point(1.0, 1.0) << dxf_footer();
  const std::string path = write_fixture("dxf_saveload.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_dxf(path);
  const auto* original = active_sketch_params(imported);

  const std::string save_path =
      (std::filesystem::temp_directory_path() / "dxf_saveload.json").string();
  manager.save_document_to_path(save_path);

  DocumentManager manager2;
  const DocumentState loaded = manager2.load_document_from_path(save_path);
  const auto* round = active_sketch_params(loaded);
  if (!expect(round != nullptr, "save/load lost active sketch")) return false;
  if (!expect(round->lines.size() == original->lines.size() &&
                  round->circles.size() == original->circles.size() &&
                  round->projected_points.size() ==
                      original->projected_points.size(),
              "save/load entity counts differ")) {
    return false;
  }
  if (!expect(near(round->lines[0].end_x, original->lines[0].end_x) &&
                  near(round->lines[0].end_y, original->lines[0].end_y),
              "save/load line coords differ")) {
    return false;
  }
  return expect(
      round->projected_points[0].source_id ==
          original->projected_points[0].source_id,
      "save/load projected point source id differs");
}

bool test_import_plane_id() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_line(0.0, 0.0, 10.0, 0.0) << dxf_footer();
  const std::string path = write_fixture("dxf_plane.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path, "ref-plane-xz");
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "plane-id import no sketch")) return false;
  if (!expect(sketch->plane_id == "ref-plane-xz",
              "import plane id not honored")) {
    return false;
  }

  try {
    manager.import_dxf(path, "not-a-plane");
  } catch (const std::runtime_error& error) {
    return expect(std::string(error.what()).find("Sketch plane not found") !=
                      std::string::npos,
                  "wrong invalid-plane error");
  }
  return expect(false, "invalid plane did not throw");
}

}  // namespace

int main() {
  std::cerr << "[dxf_import_export_test] test 0: Fusion sketch export (if present)\n";
  if (!test_fusion_sketch_when_present()) return 1;
  std::cerr << "[dxf_import_export_test] test 0b: collapsed segment dropped\n";
  if (!test_collapsed_segment_is_dropped_not_thrown()) return 1;
  std::cerr << "[dxf_import_export_test] test 0c: sub-tolerance tessellation\n";
  if (!test_sub_tolerance_tessellation_does_not_abort_import()) return 1;
  std::cerr << "[dxf_import_export_test] test 1: lines/circles/arcs\n";
  if (!test_import_lines_circles_arcs()) return 1;
  std::cerr << "[dxf_import_export_test] test 2: inches scaling\n";
  if (!test_import_insunits_inches()) return 1;
  std::cerr << "[dxf_import_export_test] test 3: bulge arcs\n";
  if (!test_lwpolyline_bulge_arc()) return 1;
  std::cerr << "[dxf_import_export_test] test 4: closed lwpolyline\n";
  if (!test_closed_lwpolyline()) return 1;
  std::cerr << "[dxf_import_export_test] test 5: legacy polyline\n";
  if (!test_polyline_legacy_closed()) return 1;
  std::cerr << "[dxf_import_export_test] test 6: exact spline import\n";
  if (!test_spline_imports_as_exact_spline()) return 1;
  std::cerr << "[dxf_import_export_test] test 6b: unrepresentable spline fallback\n";
  if (!test_unrepresentable_spline_falls_back_to_lines()) return 1;
  std::cerr << "[dxf_import_export_test] test 6c: spline profile keeps its curve\n";
  if (!test_spline_profile_keeps_curve()) return 1;
  std::cerr << "[dxf_import_export_test] test 6d: saved sketch diagnostics\n";
  if (!test_saved_sketch_diagnostics()) return 1;
  std::cerr << "[dxf_import_export_test] test 7: near-axis lines exact\n";
  if (!test_near_axis_lines_import_exact()) return 1;
  std::cerr << "[dxf_import_export_test] test 8: ellipses\n";
  if (!test_ellipse_circular_and_elliptical()) return 1;
  std::cerr << "[dxf_import_export_test] test 9: skipped entities\n";
  if (!test_skipped_entities_counted()) return 1;
  std::cerr << "[dxf_import_export_test] test 10: projected points\n";
  if (!test_points_into_projected_points()) return 1;
  std::cerr << "[dxf_import_export_test] test 11: export/import round-trip\n";
  if (!test_export_import_round_trip()) return 1;
  std::cerr << "[dxf_import_export_test] test 12: export without sketch\n";
  if (!test_export_no_active_sketch()) return 1;
  std::cerr << "[dxf_import_export_test] test 13: missing/garbage files\n";
  if (!test_import_missing_and_garbage_files()) return 1;
  std::cerr << "[dxf_import_export_test] test 14: save/load round-trip\n";
  if (!test_save_load_round_trip()) return 1;
  std::cerr << "[dxf_import_export_test] test 15: plane id\n";
  if (!test_import_plane_id()) return 1;

  std::cout << "dxf_import_export_test passed\n";
  return 0;
}
