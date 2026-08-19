// Regression tests for the DXF import/export feature:
//   - import_dxf (ASCII DXF → new sketch on a reference plane)
//   - entity mapping: LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE (bulge→arc),
//     POINT, SPLINE (de Boor tessellation), ELLIPSE
//   - $INSUNITS inches → mm scaling
//   - unsupported/degenerate entities skipped + counted
//   - export_document_as_dxf (active sketch → ASCII DXF) + round-trip
//   - error paths: missing/garbage files, no active sketch
//   - save/load round-trip of an imported sketch.
//
// Fixtures are hand-assembled ASCII DXF strings written into the temp
// directory (R12-style group-code/value pairs), so every path here
// exercises the same libdxfrw reader the core uses.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "dxf/dxf_export.h"
#include "dxf/dxf_import.h"

namespace {

using polysmith::core::DocumentManager;
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
  // DXF arcs sweep CCW from 0° to 90°: start on +x, end on +y.
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
  // 1 inch → 25.4 mm.
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

bool test_spline_tessellation() {
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_spline() << dxf_footer();
  const std::string path = write_fixture("dxf_spline.dxf", dxf.str());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "spline import no sketch")) return false;
  if (!expect(sketch->lines.size() == 64, "spline should make 64 lines")) {
    return false;
  }
  // Bezier end points equal the end control points.
  const auto& first = sketch->lines.front();
  const auto& last = sketch->lines.back();
  if (!expect(near(first.start_x, 0.0) && near(first.start_y, 0.0),
              "spline start wrong")) {
    return false;
  }
  return expect(near(last.end_x, 0.0) && near(last.end_y, 10.0),
                "spline end wrong");
}

bool test_near_axis_lines_import_exact() {
  // A line 0.005 off horizontal is within the H/V constraint hint
  // tolerance (0.01) — imported geometry must NOT be snapped to an
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
  // ratio 1.0 → circle of radius |major|.
  std::ostringstream dxf;
  dxf << dxf_header() << dxf_ellipse(5.0, 0.0, 1.0) << dxf_footer();
  const std::string path = write_fixture("dxf_ell_circ.dxf", dxf.str());
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_dxf(path);
  const auto* sketch = active_sketch_params(document);
  if (!expect(sketch != nullptr, "circular ellipse no sketch")) return false;
  if (!expect(sketch->circles.size() == 1, "circular ellipse → 1 circle")) {
    return false;
  }
  if (!expect(near(sketch->circles[0].radius, 5.0) &&
                  near(sketch->circles[0].center_x, 0.0),
              "circular ellipse radius wrong")) {
    return false;
  }

  // ratio 0.5 → tessellated segments; bounding box ±5 in x, ±2.5 in y.
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
  if (!expect(sketch2->lines.size() == 64, "ellipse should make 64 lines")) {
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

  // Imported points are editable too — no fix badges.
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
  std::cerr << "[dxf_import_export_test] test 6: spline tessellation\n";
  if (!test_spline_tessellation()) return 1;
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
