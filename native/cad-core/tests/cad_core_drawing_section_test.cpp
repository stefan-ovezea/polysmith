// Drawing section view + hatching test (P4).
//
// Pins the section pre-pass in the projection engine, the hatch
// region construction, the scanline hatcher, the cutting-plane trace
// on sibling views, and the drawing_section_update mutator:
//   - box section cut: one hatch region with the exact cross-section,
//     hidden edges suppressed (ISO 128-3 §7), golden
//   - through-hole (axis-parallel) box: donut hatch region (outer
//     loop + hole), circle rim record, golden
//   - cut direction: cut_away removes the normal side
//   - cutting-plane traces on every edge-on view
//   - mutators + refresh (create/update/undo), validation
//   - scanline hatcher geometry + determinism, golden
//   - viewport emission of hatch + cutting-plane curves

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <variant>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include "core/document/document.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"
#include "core/geometry/body_compiler.h"
#include "core/primitive/primitive_types.h"
#include "core/viewport/viewport.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::Drawing;
using polysmith::core::DrawingSheet;
using polysmith::core::DrawingView;
using polysmith::core::ProjectedEdgeRecord;
using polysmith::core::ProjectionInput;
using polysmith::core::ProjectionResult;
using polysmith::core::SectionDefinition;
using polysmith::core::SourceBody;

constexpr double kPi = 3.14159265358979323846;

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

int curve_class_count(const ProjectionResult& result,
                      const std::string& cls) {
  return static_cast<int>(std::count_if(
      result.edges.begin(), result.edges.end(),
      [&](const ProjectedEdgeRecord& r) { return r.curve_class == cls; }));
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

std::string source_identity(const ProjectedEdgeRecord& rec) {
  const auto* witness =
      std::get_if<polysmith::core::SourceEdgeWitness>(&rec.source);
  if (witness == nullptr) {
    return "outline";
  }
  if (witness->body_id.empty()) {
    return "unresolved";
  }
  return witness->body_id + "#" + std::to_string(witness->src_edge_index);
}

/// Canonical single-line form of a record (the projection-test format
/// extended with the circle geometry signature).
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
    if (rec.circle_center.has_value()) {
      os << "|c" << quant(rec.circle_center.value()[0]) << ","
         << quant(rec.circle_center.value()[1]) << ",r"
         << quant(rec.circle_radius.value());
    }
  }
  os << "|" << source_identity(rec);
  return os.str();
}

/// Canonical dump of the full section projection: records + hatch
/// regions (the golden pins region ordering and loop winding too).
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
  for (size_t r = 0; r < result.hatch_regions.size(); ++r) {
    const auto& region = result.hatch_regions[r];
    os << "\nregion " << r << " outer";
    for (const auto& p : region.outer_loop) {
      os << " " << quant(p[0]) << "," << quant(p[1]);
    }
    for (size_t h = 0; h < region.holes.size(); ++h) {
      os << "\nregion " << r << " hole " << h;
      for (const auto& p : region.holes[h]) {
        os << " " << quant(p[0]) << "," << quant(p[1]);
      }
    }
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
  return expect(buffer.str() == text,
                ("golden mismatch: " + path.string()).c_str());
}

double polygon_area(const std::vector<std::array<double, 2>>& loop) {
  double area = 0.0;
  for (size_t i = 0; i < loop.size(); ++i) {
    const auto& p = loop[i];
    const auto& q = loop[(i + 1) % loop.size()];
    area += p[0] * q[1] - q[0] * p[1];
  }
  return area / 2.0;
}

/// The section frame the refresh pass derives: the view plane IS the
/// cutting plane, view-X = the +X projected onto the plane.
polysmith::core::DrawingViewFrame section_frame(
    const SectionDefinition& section) {
  polysmith::core::DrawingViewFrame frame;
  frame.origin = section.cutting_plane_point;
  frame.normal = section.cutting_plane_normal;
  frame.x_direction = {0.0, 1.0, 0.0};
  return frame;
}

SectionDefinition plane_x(double x) {
  SectionDefinition section;
  section.cutting_plane_point = {x, 0.0, 0.0};
  section.cutting_plane_normal = {1.0, 0.0, 0.0};
  return section;
}

/// A 20x20x10 box cut with an X-axis through-cylinder (r=3, center
/// (10, 5) in the YZ plane) — a direct OCCT shape, no document.
TopoDS_Shape make_hole_box(double cylinder_start_x = 0.0,
                           double cylinder_length = 20.0) {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
  const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(
      gp_Ax2(gp_Pnt(cylinder_start_x, 10.0, 5.0), gp_Dir(1.0, 0.0, 0.0)),
      3.0, cylinder_length)
      .Shape();
  BRepAlgoAPI_Cut cut(box, cylinder);
  return cut.Shape();
}

// ── Test 1: box section cut — hatch region + hidden suppression ───

bool test_box_section() {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
  const SectionDefinition section = plane_x(10.0);

  ProjectionInput input;
  input.sources.push_back({"body-1", box});
  input.frame = section_frame(section);
  input.section = section;
  input.show_hidden = true;  // ISO 128-3 §7: ignored on section views
  input.source_revision = 1;
  const ProjectionResult result = polysmith::core::project(input);

  // One hatch region: the cut face at x=10, the 20x10 cross-section.
  if (!expect(result.hatch_regions.size() == 1,
              "box section: one hatch region")) {
    return false;
  }
  if (!expect(result.hatch_regions[0].holes.empty() &&
                  near(std::abs(polygon_area(
                           result.hatch_regions[0].outer_loop)),
                       200.0, 1e-3),
              "box section: hatch area = 20x10 = 200")) {
    std::cerr << "  area = "
              << polygon_area(result.hatch_regions[0].outer_loop) << "\n";
    std::cerr << "  regions = " << result.hatch_regions.size() << "\n";
    for (size_t r = 0; r < result.hatch_regions.size(); ++r) {
      std::cerr << "  region " << r << " outer:";
      for (const auto& p : result.hatch_regions[r].outer_loop) {
        std::cerr << " (" << p[0] << "," << p[1] << ")";
      }
      std::cerr << "\n";
    }
    return false;
  }
  // The cut face rectangle: 4 visible sharp records; the back face is
  // hidden and suppressed on a section view.
  if (!expect(visible_count(result) == 4 && hidden_count(result) == 0,
              "box section: 4 visible + 0 hidden (hidden suppressed)")) {
    std::cerr << "  got " << visible_count(result) << " visible, "
              << hidden_count(result) << " hidden\n";
    return false;
  }
  if (!check_golden("drawing_section_box", canonical_text(result))) {
    return false;
  }
  // Hidden toggle has NO effect on a section view.
  input.show_hidden = false;
  const ProjectionResult without_hidden = polysmith::core::project(input);
  return expect(canonical_text(without_hidden) == canonical_text(result),
                "box section: show_hidden is ignored on section views");
}

// ── Test 2: through-hole box — donut hatch region ─────────────────

bool test_hole_box_section() {
  const TopoDS_Shape hole_box = make_hole_box();
  const SectionDefinition section = plane_x(10.0);

  ProjectionInput input;
  input.sources.push_back({"body-1", hole_box});
  input.frame = section_frame(section);
  input.section = section;
  input.show_hidden = true;
  input.source_revision = 1;
  const ProjectionResult result = polysmith::core::project(input);

  // Donut hatch: outer rectangle + one circular hole (r=3 → π·9).
  if (!expect(result.hatch_regions.size() == 1 &&
                  result.hatch_regions[0].holes.size() == 1,
              "hole box: one hatch region with one hole")) {
    return false;
  }
  const auto& region = result.hatch_regions[0];
  // The 48-gon under-approximates the circle area by ~0.3%.
  if (!expect(near(std::abs(polygon_area(region.outer_loop)), 200.0, 1e-3) &&
                  near(std::abs(polygon_area(region.holes[0])),
                       kPi * 9.0, 0.1),
              "hole box: outer 200 + hole π·9 (48-gon)")) {
    std::cerr << "  outer = " << polygon_area(region.outer_loop)
              << " hole = " << std::abs(polygon_area(region.holes[0]))
              << "\n";
    return false;
  }
  // The hole rim is a full circle record (axis-parallel cylinder rim
  // in the cutting plane).
  bool saw_circle = false;
  for (const auto& rec : result.edges) {
    if (rec.curve_kind == "circle" && rec.line_class == "visible") {
      saw_circle = true;
    }
  }
  if (!expect(saw_circle, "hole box: the rim projects as a circle")) {
    for (const auto& rec : result.edges) {
      std::cerr << "    " << rec.line_class << " " << rec.curve_class << " "
                << rec.curve_kind << " p0(" << rec.p_start[0] << ","
                << rec.p_start[1] << ") p1(" << rec.p_end[0] << ","
                << rec.p_end[1] << ")\n";
    }
    return false;
  }
  if (!expect(hidden_count(result) == 0,
              "hole box: no hidden records on a section view")) {
    return false;
  }
  return check_golden("drawing_section_hole_box", canonical_text(result));
}

// ── Test 3: cut direction — the normal side is removed ────────────

bool test_cut_direction() {
  // Base box + a boss sticking up on the +X side (x ∈ [12,16],
  // z ∈ [10,14]).  The cutting plane x=6 crosses only the base, so
  // the boss is visible ONLY when the kept side contains it — the
  // view's vertical extent pins which side the cut removed.
  const TopoDS_Shape base = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
  const TopoDS_Shape boss = BRepPrimAPI_MakeBox(
      gp_Pnt(12.0, 10.0, 10.0), gp_Pnt(16.0, 16.0, 14.0)).Shape();
  BRepAlgoAPI_Fuse fused(base, boss);
  const TopoDS_Shape body = fused.Shape();

  SectionDefinition away_plus = plane_x(6.0);
  away_plus.cutting_plane_normal = {1.0, 0.0, 0.0};
  SectionDefinition away_minus = plane_x(6.0);
  away_minus.cutting_plane_normal = {-1.0, 0.0, 0.0};

  ProjectionInput input;
  input.sources.push_back({"body-1", body});
  input.frame = section_frame(away_plus);
  input.show_hidden = true;
  input.source_revision = 1;
  input.section = away_plus;
  const ProjectionResult cut_plus = polysmith::core::project(input);
  input.section = away_minus;
  const ProjectionResult cut_minus = polysmith::core::project(input);

  const auto max_view_y = [](const ProjectionResult& r) {
    double max_y = -1e18;
    for (const auto& rec : r.edges) {
      if (rec.line_class != "visible") {
        continue;
      }
      max_y = std::max({max_y, rec.p_start[1], rec.p_end[1]});
    }
    return max_y;
  };
  // +X cut (normal side away): the boss (x ≥ 12) is removed — the
  // view tops out at the base's z=10.  -X cut: the boss stays and is
  // visible above the cut face — the view reaches z=14.
  if (!expect(near(max_view_y(cut_plus), 10.0, 1e-4) &&
                  near(max_view_y(cut_minus), 14.0, 1e-4),
              "cut direction: the normal side is removed (boss gone/stays)")) {
    std::cerr << "  cut_plus max view-Y = " << max_view_y(cut_plus)
              << ", cut_minus max view-Y = " << max_view_y(cut_minus) << "\n";
    return false;
  }
  // The hatch is the cut face at x=6 either way: the full base
  // rectangle (the boss never crosses the plane).
  return expect(cut_plus.hatch_regions.size() == 1 &&
                    cut_plus.hatch_regions[0].holes.empty() &&
                    cut_minus.hatch_regions.size() == 1 &&
                    cut_minus.hatch_regions[0].holes.empty() &&
                    near(std::abs(polygon_area(
                             cut_plus.hatch_regions[0].outer_loop)),
                         200.0, 1e-3),
                "cut direction: hatch stays the plane's cross-section");
}

// ── Test 4: cutting-plane trace on sibling views ──────────────────

bool test_cutting_plane_trace() {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 10.0).Shape();
  // Sibling section: plane y=10, normal +Y (a right-facing cut).
  SectionDefinition sibling;
  sibling.cutting_plane_point = {0.0, 10.0, 0.0};
  sibling.cutting_plane_normal = {0.0, 1.0, 0.0};
  sibling.label = "A";

  const auto project_with_trace = [&](const polysmith::core::DrawingViewFrame&
                                          frame) {
    ProjectionInput input;
    input.sources.push_back({"body-1", box});
    input.frame = frame;
    input.show_hidden = false;
    input.section_traces.push_back({sibling});
    input.source_revision = 1;
    return polysmith::core::project(input);
  };

  // Front view (normal +X): the y=10 plane is edge-on — the trace is
  // the vertical segment (10, 0)-(10, 10) in view space.
  const ProjectionResult front =
      project_with_trace(polysmith::core::standard_view_frame("front").value());
  if (!expect(curve_class_count(front, "cutting_plane") == 1,
              "front view carries one cutting-plane trace")) {
    return false;
  }
  const auto& trace = *std::find_if(
      front.edges.begin(), front.edges.end(),
      [](const ProjectedEdgeRecord& r) {
        return r.curve_class == "cutting_plane";
      });
  double x0 = trace.p_start[0], y0 = trace.p_start[1];
  double x1 = trace.p_end[0], y1 = trace.p_end[1];
  if (x1 < x0 || (x1 == x0 && y1 < y0)) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  if (!expect(near(x0, 10.0) && near(x1, 10.0) && near(y0, 0.0, 1e-4) &&
                  near(y1, 10.0, 1e-4),
              "front trace: (10,0)-(10,10)")) {
    return false;
  }

  // Top view (normal +Z): the trace is vertical along view-Y = -X,
  // spanning -20..0.
  const ProjectionResult top =
      project_with_trace(polysmith::core::standard_view_frame("top").value());
  if (!expect(curve_class_count(top, "cutting_plane") == 1,
              "top view carries one cutting-plane trace")) {
    return false;
  }

  // Right view (normal +Y): the plane is parallel to the view — no
  // edge-on trace.
  const ProjectionResult right =
      project_with_trace(polysmith::core::standard_view_frame("right").value());
  if (!expect(curve_class_count(right, "cutting_plane") == 0,
              "right view (plane parallel) carries no trace")) {
    return false;
  }
  return check_golden("drawing_section_trace_front", canonical_text(front));
}

// ── Test 5: mutators + refresh + section update ───────────────────

bool test_section_mutators() {
  DocumentManager manager;
  manager.create_document();
  manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      manager.get_document().value(), /*include_meshes=*/false);
  const std::string body_id = bodies.bodies[0].id;

  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  drawing.sheets.push_back(sheet);
  DocumentState document = manager.drawing_create(drawing);
  const std::string drawing_id = document.drawing.drawings[0].drawing_id;
  const std::string sheet_id = document.drawing.drawings[0].sheets[0].sheet_id;

  // A section view through the mutator: the bump runs the refresh,
  // which derives the frame from the cutting plane and caches the
  // projection with the hatch region.
  DrawingView view;
  view.kind = "section";
  view.source_body_ids = {body_id};
  view.show_hidden = true;  // suppressed on section views
  view.section = plane_x(10.0);
  document = manager.drawing_view_create(drawing_id, sheet_id, view);
  const std::string view_id = document.drawing.drawings[0].views[0].view_id;

  const ProjectionResult* cached = polysmith::core::drawing_runtime::
      cached_projection(document, view_id);
  if (!expect(cached != nullptr && !cached->stale &&
                  cached->hatch_regions.size() == 1 &&
                  near(std::abs(polygon_area(
                           cached->hatch_regions[0].outer_loop)),
                       200.0, 1e-3) &&
                  hidden_count(*cached) == 0,
              "section view_create caches the cut projection with hatch")) {
    return false;
  }

  // Section update: label + hatch parameters re-project (the bump).
  SectionDefinition updated = plane_x(10.0);
  updated.label = "B";
  updated.hatch_angle_deg = 30.0;
  updated.hatch_spacing_mm = 2.0;
  document = manager.drawing_section_update(drawing_id, view_id, updated);
  if (!expect(document.drawing.drawings[0].views[0].section.has_value() &&
                  document.drawing.drawings[0].views[0].section->label == "B" &&
                  near(document.drawing.drawings[0].views[0]
                           .section->hatch_spacing_mm, 2.0),
              "drawing_section_update replaces the definition")) {
    return false;
  }
  cached = polysmith::core::drawing_runtime::cached_projection(document,
                                                               view_id);
  if (!expect(cached != nullptr && cached->hatch_regions.size() == 1,
              "section update re-projects")) {
    return false;
  }

  // Validation: degenerate normal and non-section views are rejected
  // BEFORE the undo push.
  const auto rejects = [&](const SectionDefinition& bad) {
    bool threw = false;
    try {
      manager.drawing_section_update(drawing_id, view_id, bad);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    return threw;
  };
  SectionDefinition degenerate = plane_x(10.0);
  degenerate.cutting_plane_normal = {0.0, 0.0, 0.0};
  if (!expect(rejects(degenerate),
              "degenerate cutting plane normal rejected")) {
    return false;
  }
  SectionDefinition zero_spacing = plane_x(10.0);
  zero_spacing.hatch_spacing_mm = 0.0;
  if (!expect(rejects(zero_spacing), "non-positive hatch spacing rejected")) {
    return false;
  }

  // drawing_section_update on a projection view is rejected.
  DrawingView projection_view;
  projection_view.kind = "projection";
  projection_view.standard_view = "top";
  projection_view.source_body_ids = {body_id};
  document = manager.drawing_view_create(drawing_id, sheet_id, projection_view);
  const std::string projection_view_id =
      document.drawing.drawings[0].views[1].view_id;
  bool threw = false;
  try {
    manager.drawing_section_update(drawing_id, projection_view_id, updated);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!expect(threw, "drawing_section_update on a projection view rejected")) {
    return false;
  }

  // Undo removes the projection view first; the NEXT undo restores
  // the previous section definition (and re-projects it).
  document = manager.undo();
  if (!expect(document.drawing.drawings[0].views.size() == 1,
              "undo removes the projection view")) {
    return false;
  }
  document = manager.undo();
  if (!expect(document.drawing.drawings[0].views[0].section.has_value() &&
                  document.drawing.drawings[0].views[0].section->label == "A",
              "undo restores the previous section definition")) {
    return false;
  }
  return expect(polysmith::core::drawing_runtime::cached_projection(
                    document, view_id) != nullptr,
                "undo re-projects the restored section");
}

// ── Test 6: scanline hatcher geometry + determinism ───────────────

bool test_hatch_segments() {
  // A square region: segments must stay inside the bounding box and
  // be deterministic.
  polysmith::core::HatchRegion square;
  square.outer_loop = {{0.0, 0.0}, {20.0, 0.0}, {20.0, 10.0}, {0.0, 10.0}};
  const auto segments = polysmith::core::compute_hatch_segments(
      square, /*angle_deg=*/45.0, /*spacing_mm=*/3.0);
  if (!expect(!segments.empty(), "square hatch produces segments")) {
    return false;
  }
  for (const auto& segment : segments) {
    for (const auto& p : segment) {
      if (!expect(p[0] >= -1e-6 && p[0] <= 20.0 + 1e-6 &&
                      p[1] >= -1e-6 && p[1] <= 10.0 + 1e-6,
                  "hatch segments stay inside the region")) {
        return false;
      }
    }
  }
  const auto again = polysmith::core::compute_hatch_segments(
      square, 45.0, 3.0);
  if (!expect(segments == again, "hatch segments are deterministic")) {
    return false;
  }

  // The hole cuts the total hatch LENGTH down (even-odd across
  // loops) — scanlines split around the hole, so the SEGMENT COUNT
  // may rise while the length always falls.
  const auto total_length = [](const auto& segs) {
    double length = 0.0;
    for (const auto& segment : segs) {
      length += std::hypot(segment[0][0] - segment[1][0],
                           segment[0][1] - segment[1][1]);
    }
    return length;
  };
  polysmith::core::HatchRegion donut = square;
  std::vector<std::array<double, 2>> hole;
  for (int i = 0; i < 48; ++i) {
    const double a = 2.0 * kPi * i / 48.0;
    hole.push_back({10.0 + 3.0 * std::cos(a), 5.0 + 3.0 * std::sin(a)});
  }
  donut.holes.push_back(hole);
  const auto donut_segments = polysmith::core::compute_hatch_segments(
      donut, 45.0, 3.0);
  if (!expect(total_length(donut_segments) < total_length(segments),
              "the hole removes hatch length")) {
    return false;
  }

  // Golden: canonical segment dump (sorted, quantized).
  std::vector<std::string> lines;
  for (const auto& segment : segments) {
    std::ostringstream os;
    os << std::fixed;
    os.precision(6);
    os << quant(segment[0][0]) << "," << quant(segment[0][1]) << "|"
       << quant(segment[1][0]) << "," << quant(segment[1][1]);
    lines.push_back(os.str());
  }
  std::sort(lines.begin(), lines.end());
  std::ostringstream dump;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i) dump << "\n";
    dump << lines[i];
  }
  return check_golden("drawing_section_hatch_segments", dump.str());
}

// ── Test 7: viewport emission of hatch + trace curves ─────────────

bool test_section_emission() {
  DocumentManager manager;
  manager.create_document();
  manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto bodies = polysmith::core::compile_bodies(
      manager.get_document().value(), /*include_meshes=*/false);
  const std::string body_id = bodies.bodies[0].id;

  Drawing drawing;
  drawing.name = "Test Drawing";
  DrawingSheet sheet;
  sheet.name = "Sheet 1";
  drawing.sheets.push_back(sheet);
  DocumentState document = manager.drawing_create(drawing);
  const std::string drawing_id = document.drawing.drawings[0].drawing_id;
  const std::string sheet_id = document.drawing.drawings[0].sheets[0].sheet_id;

  // A front view + a section view (plane y=10) whose cutting plane
  // traces onto the front view.
  DrawingView front;
  front.kind = "projection";
  front.standard_view = "front";
  front.source_body_ids = {body_id};
  front.sheet_position = {30.0, 40.0};
  document = manager.drawing_view_create(drawing_id, sheet_id, front);

  DrawingView section_view;
  section_view.kind = "section";
  section_view.source_body_ids = {body_id};
  SectionDefinition section;
  section.cutting_plane_point = {0.0, 10.0, 0.0};
  section.cutting_plane_normal = {0.0, 1.0, 0.0};
  section_view.section = section;
  section_view.scale = 0.5;
  section_view.sheet_position = {120.0, 40.0};
  document = manager.drawing_view_create(drawing_id, sheet_id, section_view);

  const auto state = polysmith::core::build_viewport_state(document);
  if (!expect(state.drawing_sheets.size() == 1,
              "viewport emits one sheet")) {
    return false;
  }
  const auto& emitted = state.drawing_sheets[0];

  int hatch_count = 0;
  for (const auto& curve : emitted.curves) {
    if (curve.curve_class != "hatch") {
      continue;
    }
    ++hatch_count;
    if (!expect(curve.kind == "line" && curve.line_class == "visible",
                "hatch curves are thin visible lines")) {
      return false;
    }
  }
  if (!expect(hatch_count > 0, "the section view emits hatch lines")) {
    return false;
  }

  // The front view emits the sibling section's cutting-plane chain
  // line — the P5 flatten dashes it into 3 segments (dash 6 / dot /
  // final dash).
  int trace_count = 0;
  for (const auto& curve : emitted.curves) {
    if (curve.curve_class == "cutting_plane") {
      ++trace_count;
    }
  }
  return expect(trace_count == 3,
                "the front view emits the dashed cutting-plane trace");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_section_test\n";
  std::cout << "  Test 1: box section cut... ";
  if (test_box_section()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: through-hole box section... ";
  if (test_hole_box_section()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: cut direction... ";
  if (test_cut_direction()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: cutting-plane trace... ";
  if (test_cutting_plane_trace()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: section mutators... ";
  if (test_section_mutators()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: scanline hatcher... ";
  if (test_hatch_segments()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: section emission... ";
  if (test_section_emission()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cad_core_drawing_section_test passed\n";
    return 0;
  }
  return 1;
}
