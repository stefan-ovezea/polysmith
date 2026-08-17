// Regression test: extrude a circle profile and verify the resulting body
// contains an analytic cylindrical face (Geom_CylindricalSurface), not a
// polygon-approximated face.  This guards against accidental reintroduction
// of a raw BRepPrimAPI_MakeCylinder shortcut that bypasses the unified
// make_polygon_prism_shape path and breaks multi-profile / thin-wall /
// plane-frame coordinate mapping.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include <nlohmann/json.hpp>

#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"
#include "core/sketch/trim_engine.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// Returns true if `shape` has at least one face whose underlying geometry
// is a Geom_CylindricalSurface (an analytic cylinder).  Polygon-approximated
// extrusions produce faces backed by Geom_BSplineSurface instead.
bool has_cylindrical_face(const TopoDS_Shape& shape) {
  TopExp_Explorer explorer(shape, TopAbs_FACE);
  for (; explorer.More(); explorer.Next()) {
    const TopoDS_Face& face = TopoDS::Face(explorer.Current());
    const Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    if (surface->IsKind(STANDARD_TYPE(Geom_CylindricalSurface))) {
      return true;
    }
  }
  return false;
}

bool test_circle_extrude_is_smooth() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Draw a 20 mm radius circle centred at the origin.
  DocumentState document =
      manager.add_sketch_circle(0.0, 0.0, 20.0, /*is_construction=*/false);

  // Select the circle profile and extrude it 10 mm.
  const std::string profile_id =
      document.feature_history.back().sketch_parameters->profiles.front().id;
  document = manager.extrude_profile(profile_id, 10.0, /*mode=*/"",
                                     /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);

  if (!expect(compiled.bodies.size() == 1,
              "expected exactly one compiled body")) {
    return false;
  }

  return expect(
      has_cylindrical_face(compiled.bodies.front().shape),
      "extruded circle must have a cylindrical (analytic) face — polygon "
      "approximation detected");
}


bool test_filleted_rectangle_extrudes_full_prism() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 100.0, 80.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const auto& lines = sketch.lines;
  const auto& points = sketch.vertices;
  std::string corner_bl, corner_br, corner_tr, corner_tl;
  std::string line_bottom, line_right, line_top, line_left;
  for (const auto& pt : points) {
    if (std::abs(pt.x - 0.0) < 0.01 && std::abs(pt.y - 0.0) < 0.01)
      corner_bl = pt.id;
    else if (std::abs(pt.x - 100.0) < 0.01 && std::abs(pt.y - 0.0) < 0.01)
      corner_br = pt.id;
    else if (std::abs(pt.x - 100.0) < 0.01 && std::abs(pt.y - 80.0) < 0.01)
      corner_tr = pt.id;
    else if (std::abs(pt.x - 0.0) < 0.01 && std::abs(pt.y - 80.0) < 0.01)
      corner_tl = pt.id;
  }
  for (const auto& ln : lines) {
    const bool near_y_0 = std::abs(ln.start_y - 0.0) < 0.01 &&
                          std::abs(ln.end_y - 0.0) < 0.01;
    const bool near_y_80 = std::abs(ln.start_y - 80.0) < 0.01 &&
                           std::abs(ln.end_y - 80.0) < 0.01;
    const bool near_x_0 = std::abs(ln.start_x - 0.0) < 0.01 &&
                          std::abs(ln.end_x - 0.0) < 0.01;
    const bool near_x_100 = std::abs(ln.start_x - 100.0) < 0.01 &&
                            std::abs(ln.end_x - 100.0) < 0.01;
    if (near_y_0) line_bottom = ln.id;
    else if (near_x_100) line_right = ln.id;
    else if (near_y_80) line_top = ln.id;
    else if (near_x_0) line_left = ln.id;
  }
  if (!expect(!corner_bl.empty() && !corner_br.empty() && !corner_tr.empty() &&
                  !corner_tl.empty() && !line_bottom.empty() &&
                  !line_right.empty() && !line_top.empty() && !line_left.empty(),
              "filleted extrude: failed to locate corners / lines")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_bl, line_bottom, line_left, 5.0);
  document = manager.add_sketch_fillet(corner_br, line_bottom, line_right, 5.0);
  document = manager.add_sketch_fillet(corner_tr, line_right, line_top, 5.0);
  document = manager.add_sketch_fillet(corner_tl, line_top, line_left, 5.0);

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1,
              "filleted extrude: expected exactly one profile")) {
    return false;
  }

  document = manager.extrude_profile(profiles.front().id, /*depth=*/10.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "filleted extrude: expected exactly one compiled body")) {
    return false;
  }

  const TopoDS_Shape& shape = compiled.bodies.front().shape;
  if (!expect(!shape.IsNull(), "filleted extrude: shape must not be null")) {
    return false;
  }

  // Volume sanity: rect 100×80 minus four r=5 corners
  // (each corner removes 25 − 25π/4 ≈ 5.365), extruded 10.
  GProp_GProps props;
  BRepGProp::VolumeProperties(shape, props);
  const double volume = props.Mass();
  const double expected_area = 8000.0 - 4.0 * (25.0 - 25.0 * 3.141592653589793 / 4.0);
  const double expected_volume = expected_area * 10.0;
  std::cerr << "filleted extrude: volume=" << volume
            << " expected=" << expected_volume << "\n";

  // Count faces: 4 line walls + 4 arc walls + 2 caps = 10.
  int face_count = 0;
  for (TopExp_Explorer face_it(shape, TopAbs_FACE); face_it.More();
       face_it.Next()) {
    ++face_count;
  }
  std::cerr << "filleted extrude: faces=" << face_count << "\n";

  return expect(std::abs(volume - expected_volume) < expected_volume * 0.02,
                "filleted extrude: volume must match rounded-rect prism") &&
         expect(face_count >= 8,
                "filleted extrude: expected a closed prism (>= 8 faces)");
}


// Regression test for the part.json sketch: two tangent lines from
// (-100, 0) to a circle, a horizontal line to the circle's left quadrant,
// and a remnant arc left over from trimming another circle.  The exact
// arrangement treats every bounded closed region as a profile — the two
// tangent wedges are the extrudable regions, and the region between the
// remnant arc and the circle (a lens/cap) is a legitimate profile that
// mixes arc and circle edges (the old curve-only filter is gone).
bool test_curve_only_lens_faces_are_real_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // DocumentManager::add_sketch_line takes (start_x, start_y, end_x,
  // end_y) — no entity index (the index flavour is the sketch-level
  // helper).  Passing an index shifts every coordinate one slot and
  // silently converts the trailing y coordinate into is_construction,
  // which previously produced degenerate construction lines instead of
  // the intended tangent wedge geometry.
  manager.add_sketch_line(-100.0, 0.0, -5.940552660969672, 23.638212711184707);
  manager.add_sketch_line(-100.0, 0.0, -5.940552660969672, -23.638212711184707);
  DocumentState document =
      manager.add_sketch_line(-100.0, 0.0, -24.37324898524955, 0.0);
  document = manager.add_sketch_circle(0.0, 0.0, 24.37324898524955,
                                       /*is_construction=*/false);
  document = manager.add_sketch_arc(0.0, 24.37324898524955,
                                    0.0, -24.37324898524955,
                                    /*anchor_x=*/-14.5804, /*anchor_y=*/0.0,
                                    /*mode=*/"three_point",
                                    /*is_construction=*/false);

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;

  std::cerr << "lens filter: profile count=" << profiles.size() << "\n";
  for (const auto& p : profiles) {
    std::cerr << "  ids:";
    for (const auto& id : p.line_ids) std::cerr << " " << id;
    std::cerr << " src=" << (p.source_circle_id ? *p.source_circle_id : "none")
              << "\n";
  }

  // The tangent wedges: line-1 + line-3 + circle and line-2 + line-3 +
  // circle.  Both are legitimate regions; the wedge whose boundary
  // arrives at the tangent point ALONG THE CIRCLE is ambiguous for a
  // purely local turn rule (the tangent cusp permits two faces), so at
  // least one wedge must be detected.
  auto has_ids = [&](std::initializer_list<std::string> want) {
    return std::any_of(
        profiles.begin(), profiles.end(), [&](const auto& p) {
          std::set<std::string> ids(p.line_ids.begin(), p.line_ids.end());
          for (const auto& id : want) {
            if (!ids.count(id)) return false;
          }
          return true;
        });
  };
  const bool has_top_wedge = has_ids({"line-1", "line-3", "circle-1"});
  const bool has_bottom_wedge = has_ids({"line-2", "line-3", "circle-1"});
  if (!expect(has_top_wedge || has_bottom_wedge,
              "lens: at least one tangent wedge must be detected")) {
    return false;
  }

  // The region between the remnant arc and the circle is a real profile
  // now (it may be walked as the lens or the cap depending on the
  // tangent-cusp resolution) — it mixes arc and circle edges.
  const bool has_curve_region = std::any_of(
      profiles.begin(), profiles.end(), [](const auto& p) {
        const bool has_arc = std::any_of(
            p.line_ids.begin(), p.line_ids.end(),
            [](const std::string& id) { return id.rfind("arc-", 0) == 0; });
        const bool has_circle = std::any_of(
            p.line_ids.begin(), p.line_ids.end(),
            [](const std::string& id) { return id.rfind("circle-", 0) == 0; });
        return has_arc && has_circle;
      });
  return expect(has_curve_region,
                "lens: the arc/circle region must be a profile");
}


// User-reported regression: a rectangle filleted in the UI's corner order
// produces a clockwise edge-loop walk.  detect_edge_loop must reverse the
// point ids / edge ids in lockstep with the points — otherwise the wire
// builder pairs edge ids with the wrong coordinates and extrudes a
// degenerate sliver instead of the full rounded-rect prism.
bool test_filleted_rectangle_clockwise_walk_extrudes_full_prism() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_rectangle(-45.0, 25.0, 45.0, -25.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const auto& lines = sketch.lines;
  const auto& points = sketch.vertices;
  std::string corner_bl, corner_br, corner_tr, corner_tl;
  std::string line_bottom, line_right, line_top, line_left;
  for (const auto& pt : points) {
    if (std::abs(pt.x + 45.0) < 0.01 && std::abs(pt.y + 25.0) < 0.01)
      corner_bl = pt.id;
    else if (std::abs(pt.x - 45.0) < 0.01 && std::abs(pt.y + 25.0) < 0.01)
      corner_br = pt.id;
    else if (std::abs(pt.x - 45.0) < 0.01 && std::abs(pt.y - 25.0) < 0.01)
      corner_tr = pt.id;
    else if (std::abs(pt.x + 45.0) < 0.01 && std::abs(pt.y - 25.0) < 0.01)
      corner_tl = pt.id;
  }
  for (const auto& ln : lines) {
    const bool near_y_25 = std::abs(ln.start_y - 25.0) < 0.01 &&
                           std::abs(ln.end_y - 25.0) < 0.01;
    const bool near_y_m25 = std::abs(ln.start_y + 25.0) < 0.01 &&
                            std::abs(ln.end_y + 25.0) < 0.01;
    const bool near_x_45 = std::abs(ln.start_x - 45.0) < 0.01 &&
                           std::abs(ln.end_x - 45.0) < 0.01;
    const bool near_x_m45 = std::abs(ln.start_x + 45.0) < 0.01 &&
                            std::abs(ln.end_x + 45.0) < 0.01;
    if (near_y_25) line_top = ln.id;
    else if (near_x_45) line_right = ln.id;
    else if (near_y_m25) line_bottom = ln.id;
    else if (near_x_m45) line_left = ln.id;
  }
  if (!expect(!corner_bl.empty() && !corner_br.empty() && !corner_tr.empty() &&
                  !corner_tl.empty() && !line_bottom.empty() &&
                  !line_right.empty() && !line_top.empty() && !line_left.empty(),
              "cw-walk extrude: failed to locate corners / lines")) {
    return false;
  }

  // UI fillet order: tl(top,left), tr(top,right), br(right,bottom),
  // bl(bottom,left) — this produces the clockwise loop walk.
  document = manager.add_sketch_fillet(corner_tl, line_top, line_left, 5.0);
  document = manager.add_sketch_fillet(corner_tr, line_top, line_right, 5.0);
  document = manager.add_sketch_fillet(corner_br, line_right, line_bottom, 5.0);
  document = manager.add_sketch_fillet(corner_bl, line_bottom, line_left, 5.0);

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1,
              "cw-walk extrude: expected exactly one profile")) {
    return false;
  }
  const auto& profile = profiles.front();
  if (!expect(profile.ordered_edge_ids.size() == profile.points.size(),
              "cw-walk extrude: edge ids must align 1:1 with points")) {
    return false;
  }

  document = manager.extrude_profile(profile.id, /*depth=*/10.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "cw-walk extrude: expected one compiled body")) {
    return false;
  }
  const TopoDS_Shape& shape = compiled.bodies.front().shape;
  GProp_GProps props;
  BRepGProp::VolumeProperties(shape, props);
  const double volume = props.Mass();
  const double expected_area =
      4500.0 - 4.0 * (25.0 - 25.0 * 3.141592653589793 / 4.0);
  const double expected_volume = expected_area * 10.0;
  std::cerr << "cw-walk extrude: volume=" << volume
            << " expected=" << expected_volume << "\n";
  return expect(std::abs(volume - expected_volume) < expected_volume * 0.02,
                "cw-walk extrude: volume must match rounded-rect prism");
}



// User-reported regression: rounded rect + two diagonals touching the
// left fillet arcs at their midpoints + an interior circle.  The
// diagonals split the arc chains; the arrangement must still detect the
// rounded-rect body, the circle, and the "nose" region between the
// diagonals and the left side.
bool test_rounded_rect_with_touching_lines_and_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_rectangle(-50.0, 25.0, 50.0, -25.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const auto& lines = sketch.lines;
  const auto& points = sketch.vertices;
  std::string corner_bl, corner_br, corner_tr, corner_tl;
  std::string line_bottom, line_right, line_top, line_left;
  for (const auto& pt : points) {
    if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y + 25.0) < 0.01)
      corner_bl = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y + 25.0) < 0.01)
      corner_br = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y - 25.0) < 0.01)
      corner_tr = pt.id;
    else if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y - 25.0) < 0.01)
      corner_tl = pt.id;
  }
  for (const auto& ln : lines) {
    const bool near_y_25 = std::abs(ln.start_y - 25.0) < 0.01 &&
                           std::abs(ln.end_y - 25.0) < 0.01;
    const bool near_y_m25 = std::abs(ln.start_y + 25.0) < 0.01 &&
                            std::abs(ln.end_y + 25.0) < 0.01;
    const bool near_x_50 = std::abs(ln.start_x - 50.0) < 0.01 &&
                           std::abs(ln.end_x - 50.0) < 0.01;
    const bool near_x_m50 = std::abs(ln.start_x + 50.0) < 0.01 &&
                            std::abs(ln.end_x + 50.0) < 0.01;
    if (near_y_25) line_top = ln.id;
    else if (near_x_50) line_right = ln.id;
    else if (near_y_m25) line_bottom = ln.id;
    else if (near_x_m50) line_left = ln.id;
  }
  if (!expect(!corner_bl.empty() && !corner_br.empty() && !corner_tr.empty() &&
                  !corner_tl.empty() && !line_bottom.empty() &&
                  !line_right.empty() && !line_top.empty() && !line_left.empty(),
              "touching-lines: failed to locate corners / lines")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_tl, line_top, line_left, 5.0);
  document = manager.add_sketch_fillet(corner_tr, line_top, line_right, 5.0);
  document = manager.add_sketch_fillet(corner_br, line_right, line_bottom, 5.0);
  document = manager.add_sketch_fillet(corner_bl, line_bottom, line_left, 5.0);

  // Diagonals touching the left arcs at their midpoints (45° points).
  // DocumentManager::add_sketch_line takes (start_x, start_y, end_x,
  // end_y) — no entity index (see the lens test above).  Passing an
  // index shifted every coordinate one slot and silently converted the
  // trailing y into is_construction, so the diagonals never existed.
  document = manager.add_sketch_line(-100.0, 0.0, -48.54, 23.54);
  document = manager.add_sketch_line(-100.0, 0.0, -48.54, -23.54);
  document = manager.add_sketch_circle(0.0, 0.0, 8.33,
                                       /*is_construction=*/false);

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  std::cerr << "touching-lines: profile count=" << profiles.size() << "\n";
  for (const auto& p : profiles) {
    std::cerr << "  ids:";
    for (const auto& id : p.line_ids) std::cerr << " " << id;
    std::cerr << " src=" << (p.source_circle_id ? *p.source_circle_id : "none")
              << "\n";
  }

  // The rounded-rect body must exist: a polygon whose unique edge ids
  // include all four trimmed lines.
  const auto body_it = std::find_if(
      profiles.begin(), profiles.end(),
      [&](const auto& p) {
        if (p.kind != "polygon") return false;
        std::set<std::string> ids(p.line_ids.begin(), p.line_ids.end());
        return ids.count(line_top) > 0 && ids.count(line_right) > 0 &&
               ids.count(line_bottom) > 0 && ids.count(line_left) > 0;
      });
  if (!expect(body_it != profiles.end(),
              "touching-lines: rounded-rect body profile must be detected")) {
    return false;
  }

  // The circle interior must exist.
  const bool has_circle = std::any_of(
      profiles.begin(), profiles.end(),
      [](const auto& p) { return p.source_circle_id.has_value(); });
  if (!expect(has_circle, "touching-lines: circle profile must be detected")) {
    return false;
  }

  // The circle inside the body must be nested as its inner loop (hole):
  // extruding the body alone yields a slab with a round hole.
  if (!expect(!body_it->inner_loops.empty(),
              "touching-lines: body must carry the circle as an inner loop")) {
    return false;
  }

  // Extrude the body and verify the volume.
  document = manager.extrude_profile(body_it->id, /*depth=*/10.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "touching-lines: expected one compiled body")) {
    return false;
  }
  GProp_GProps props;
  BRepGProp::VolumeProperties(compiled.bodies.front().shape, props);
  const double volume = props.Mass();
  // Solid slab minus the round hole (r=8.33), depth 10.
  const double expected_area =
      5000.0 - 4.0 * (25.0 - 25.0 * 3.141592653589793 / 4.0) -
      3.141592653589793 * 8.33 * 8.33;
  const double expected_volume = expected_area * 10.0;
  std::cerr << "touching-lines: volume=" << volume
            << " expected=" << expected_volume << "\n";
  return expect(std::abs(volume - expected_volume) < expected_volume * 0.02,
                "touching-lines: volume must be the slab with the round hole");
}



// User-reported regression: rounded rect + two concentric circles +
// one separate circle.  Clean-bore rule: the small circle inside the
// big circle has no effect on the outer profile's holes — extruding the
// rect yields exactly two holes (big + separate), and the small circle
// remains separately selectable.
bool test_concentric_circles_clean_bore() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_rectangle(-50.0, 30.0, 50.0, -30.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const auto& lines = sketch.lines;
  const auto& points = sketch.vertices;
  std::string corner_bl, corner_br, corner_tr, corner_tl;
  std::string line_bottom, line_right, line_top, line_left;
  for (const auto& pt : points) {
    if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y + 30.0) < 0.01)
      corner_bl = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y + 30.0) < 0.01)
      corner_br = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y - 30.0) < 0.01)
      corner_tr = pt.id;
    else if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y - 30.0) < 0.01)
      corner_tl = pt.id;
  }
  for (const auto& ln : lines) {
    const bool near_y_30 = std::abs(ln.start_y - 30.0) < 0.01 &&
                           std::abs(ln.end_y - 30.0) < 0.01;
    const bool near_y_m30 = std::abs(ln.start_y + 30.0) < 0.01 &&
                            std::abs(ln.end_y + 30.0) < 0.01;
    const bool near_x_50 = std::abs(ln.start_x - 50.0) < 0.01 &&
                           std::abs(ln.end_x - 50.0) < 0.01;
    const bool near_x_m50 = std::abs(ln.start_x + 50.0) < 0.01 &&
                            std::abs(ln.end_x + 50.0) < 0.01;
    if (near_y_30) line_top = ln.id;
    else if (near_x_50) line_right = ln.id;
    else if (near_y_m30) line_bottom = ln.id;
    else if (near_x_m50) line_left = ln.id;
  }
  if (!expect(!corner_bl.empty() && !corner_br.empty() && !corner_tr.empty() &&
                  !corner_tl.empty() && !line_bottom.empty() &&
                  !line_right.empty() && !line_top.empty() && !line_left.empty(),
              "concentric: failed to locate corners / lines")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_tl, line_top, line_left, 5.0);
  document = manager.add_sketch_fillet(corner_tr, line_top, line_right, 5.0);
  document = manager.add_sketch_fillet(corner_br, line_right, line_bottom, 5.0);
  document = manager.add_sketch_fillet(corner_bl, line_bottom, line_left, 5.0);

  document = manager.add_sketch_circle(0.0, 0.0, 17.68,
                                       /*is_construction=*/false);  // big
  document = manager.add_sketch_circle(0.0, 0.0, 7.07,
                                       /*is_construction=*/false);  // small, concentric
  document = manager.add_sketch_circle(-35.0, 0.0, 6.27,
                                       /*is_construction=*/false);  // separate

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;

  // Find the rect profile.
  const auto body_it = std::find_if(
      profiles.begin(), profiles.end(),
      [&](const auto& p) {
        if (p.kind != "polygon" || p.source_circle_id.has_value()) {
          return false;
        }
        std::set<std::string> ids(p.line_ids.begin(), p.line_ids.end());
        return ids.count(line_top) > 0 && ids.count(line_right) > 0 &&
               ids.count(line_bottom) > 0 && ids.count(line_left) > 0;
      });
  if (!expect(body_it != profiles.end(),
              "concentric: rect profile must exist")) {
    return false;
  }

  // Clean-bore rule: exactly TWO inner loops (big + separate), not three.
  if (!expect(body_it->inner_loops.size() == 2,
              "concentric: rect must carry exactly 2 holes (small ignored)")) {
    return false;
  }

  // The small circle must still be separately selectable.
  const bool small_selectable = std::any_of(
      profiles.begin(), profiles.end(), [](const auto& p) {
        return p.source_circle_id.has_value() && *p.source_circle_id == "circle-2";
      });
  if (!expect(small_selectable,
              "concentric: small circle must remain a selectable profile")) {
    return false;
  }

  document = manager.extrude_profile(body_it->id, /*depth=*/20.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "concentric: expected one compiled body")) {
    return false;
  }
  GProp_GProps props;
  BRepGProp::VolumeProperties(compiled.bodies.front().shape, props);
  const double volume = props.Mass();
  const double kPi = 3.141592653589793;
  const double expected_area = 6000.0 - 4.0 * (25.0 - 25.0 * kPi / 4.0) -
                               kPi * 17.68 * 17.68 - kPi * 6.27 * 6.27;
  const double expected_volume = expected_area * 20.0;
  std::cerr << "concentric: volume=" << volume
            << " expected=" << expected_volume << "\n";
  return expect(std::abs(volume - expected_volume) < expected_volume * 0.02,
                "concentric: volume = slab with exactly the big + separate "
                "holes");
}


// User-reported regression: selecting the rect AND the big concentric
// circle must extrude the big circle as a solid boss with the smaller
// circles inside it as holes, while the rect keeps its own holes.
bool test_big_circle_selection_boss_with_holes() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_rectangle(-50.0, 35.0, 50.0, -35.0);

  const auto& sketch = document.feature_history.back().sketch_parameters.value();
  const auto& lines = sketch.lines;
  const auto& points = sketch.vertices;
  std::string corner_bl, corner_br, corner_tr, corner_tl;
  std::string line_bottom, line_right, line_top, line_left;
  for (const auto& pt : points) {
    if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y + 35.0) < 0.01)
      corner_bl = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y + 35.0) < 0.01)
      corner_br = pt.id;
    else if (std::abs(pt.x - 50.0) < 0.01 && std::abs(pt.y - 35.0) < 0.01)
      corner_tr = pt.id;
    else if (std::abs(pt.x + 50.0) < 0.01 && std::abs(pt.y - 35.0) < 0.01)
      corner_tl = pt.id;
  }
  for (const auto& ln : lines) {
    const bool near_y_35 = std::abs(ln.start_y - 35.0) < 0.01 &&
                           std::abs(ln.end_y - 35.0) < 0.01;
    const bool near_y_m35 = std::abs(ln.start_y + 35.0) < 0.01 &&
                            std::abs(ln.end_y + 35.0) < 0.01;
    const bool near_x_50 = std::abs(ln.start_x - 50.0) < 0.01 &&
                           std::abs(ln.end_x - 50.0) < 0.01;
    const bool near_x_m50 = std::abs(ln.start_x + 50.0) < 0.01 &&
                            std::abs(ln.end_x + 50.0) < 0.01;
    if (near_y_35) line_top = ln.id;
    else if (near_x_50) line_right = ln.id;
    else if (near_y_m35) line_bottom = ln.id;
    else if (near_x_m50) line_left = ln.id;
  }
  if (!expect(!corner_bl.empty() && !corner_br.empty() && !corner_tr.empty() &&
                  !corner_tl.empty() && !line_bottom.empty() &&
                  !line_right.empty() && !line_top.empty() && !line_left.empty(),
              "boss test: failed to locate corners / lines")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_tl, line_top, line_left, 5.0);
  document = manager.add_sketch_fillet(corner_tr, line_top, line_right, 5.0);
  document = manager.add_sketch_fillet(corner_br, line_right, line_bottom, 5.0);
  document = manager.add_sketch_fillet(corner_bl, line_bottom, line_left, 5.0);

  document = manager.add_sketch_circle(0.0, 0.0, 16.07,
                                       /*is_construction=*/false);  // small concentric
  document = manager.add_sketch_circle(0.0, 0.0, 28.87,
                                       /*is_construction=*/false);  // big
  document = manager.add_sketch_circle(-41.68, -18.83, 4.64,
                                       /*is_construction=*/false);  // separate
  document = manager.add_sketch_circle(-20.05, -6.57, 2.73,
                                       /*is_construction=*/false);  // inside big

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;

  // The big circle's profile must carry the two inner circles as holes.
  const auto big_it = std::find_if(
      profiles.begin(), profiles.end(), [](const auto& p) {
        return p.source_circle_id.has_value() &&
               *p.source_circle_id == "circle-2";
      });
  if (!expect(big_it != profiles.end(), "boss test: big circle profile")) {
    return false;
  }
  if (!expect(big_it->inner_loops.size() == 2,
              "boss test: big circle must carry its 2 inner circles as holes")) {
    return false;
  }

  // The rect profile must carry exactly the big + separate circles.
  const auto rect_it = std::find_if(
      profiles.begin(), profiles.end(),
      [&](const auto& p) {
        if (p.kind != "polygon" || p.source_circle_id.has_value()) {
          return false;
        }
        std::set<std::string> ids(p.line_ids.begin(), p.line_ids.end());
        return ids.count(line_top) > 0 && ids.count(line_right) > 0 &&
               ids.count(line_bottom) > 0 && ids.count(line_left) > 0;
      });
  if (!expect(rect_it != profiles.end(), "boss test: rect profile")) {
    return false;
  }
  if (!expect(rect_it->inner_loops.size() == 2,
              "boss test: rect keeps exactly 2 holes (big + separate)")) {
    return false;
  }

  // Extrude [big, rect] — the user's selection order.
  document = manager.extrude_profiles(
      {big_it->id, rect_it->id}, /*depth=*/20.0, /*mode=*/"",
      /*target_body_id=*/std::nullopt, /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);
  std::cerr << "boss test: bodies=" << compiled.bodies.size() << "\n";
  double total_volume = 0.0;
  for (const auto& body : compiled.bodies) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(body.shape, props);
    total_volume += props.Mass();
  }
  const double kPi = 3.141592653589793;
  const double rect_area = 7000.0 - 4.0 * (25.0 - 25.0 * kPi / 4.0);
  // Selecting rect + big circle: the big circle's region becomes solid
  // (the boss fills the rect's big hole), and the separate circle plus
  // the two circles inside the big one stay as holes:
  //   full slab − separate − inner1 − inner2
  const double expected_area =
      rect_area - kPi * 4.64 * 4.64 - kPi * 16.07 * 16.07 -
      kPi * 2.73 * 2.73;
  const double expected_volume = expected_area * 20.0;
  std::cerr << "boss test: total volume=" << total_volume
            << " expected=" << expected_volume << "\n";
  return expect(std::abs(total_volume - expected_volume) <
                    expected_volume * 0.02,
                "boss test: slab with separate hole plus boss with 2 holes");
}

// Trim-created entities must never reuse an id still held by another
// entity.  The old size-based numbering (entities.size() + 1) collided
// after trims deleted entities — the user's part.json carried two
// "line-6" lines and two "arc-3" / "arc-4" arcs, and the wire builder's
// id-based dedup then skipped the second group of each, producing an
// open wire that extruded as uncapped shells (open tube + partial
// ribbon wall).
// User-reported: a sketch on a face with projected corner points, a
// redrawn rectangle and a center circle — after save + reload the
// circle jumped from the center onto a corner point.  The payload
// parser dropped the circle's center_vertex_id and did not restore
// next_vertex_index, so the first vertex rebuild after load
// re-assigned "vertex-1" to the circle center and adopted the corner
// vertex's coordinates.  This test round-trips a rectangle + circle
// through save/load and verifies the center survives, including
// across a post-load edit that triggers a vertex rebuild.
bool test_load_preserves_circle_center_vertex() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  document = manager.add_sketch_circle(20.0, 10.0, 4.0,
                                       /*is_construction=*/false);

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_circle_reload_test.json";
  manager.save_document_to_path(path.string());

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path.string());

  const auto& sketch = loaded.feature_history.back().sketch_parameters;
  if (!expect(sketch.has_value(), "circle reload: sketch present")) {
    return false;
  }
  const auto circle_it = std::find_if(
      sketch->circles.begin(), sketch->circles.end(),
      [](const auto& c) { return c.id == "circle-1"; });
  if (!expect(circle_it != sketch->circles.end(),
              "circle reload: circle present")) {
    return false;
  }
  if (!expect(std::abs(circle_it->center_x - 20.0) < 1e-9 &&
                  std::abs(circle_it->center_y - 10.0) < 1e-9,
              "circle reload: center must survive the round-trip")) {
    std::cerr << "  got center=(" << circle_it->center_x << ","
              << circle_it->center_y << ")\n";
    return false;
  }
  if (!expect(!circle_it->center_vertex_id.empty(),
              "circle reload: center_vertex_id must be restored")) {
    return false;
  }

  // Trigger a vertex rebuild (any sketch edit runs
  // refresh_sketch_derived_state) and re-verify the center — this is
  // where the old collision moved the circle onto a corner.
  loaded = loaded_manager.add_sketch_circle(50.0, 10.0, 2.0,
                                            /*is_construction=*/false);
  const auto& sketch2 = loaded.feature_history.back().sketch_parameters;
  const auto circle_after = std::find_if(
      sketch2->circles.begin(), sketch2->circles.end(),
      [](const auto& c) { return c.id == "circle-1"; });
  return expect(circle_after != sketch2->circles.end() &&
                    std::abs(circle_after->center_x - 20.0) < 1e-9 &&
                    std::abs(circle_after->center_y - 10.0) < 1e-9,
                "circle reload: center must survive a post-load rebuild");
}

bool test_trim_entity_ids_never_collide() {
  return expect(polysmith::core::next_trim_entity_index({}) == 1,
                "trim ids: empty set starts at 1") &&
         expect(polysmith::core::next_trim_entity_index(
                    {"arc-1", "arc-3"}) == 4,
                "trim ids: must skip past the highest existing id") &&
         expect(polysmith::core::next_trim_entity_index(
                    {"line-2", "line-6"}) == 7,
                "trim ids: separate id spaces stay independent") &&
         expect(polysmith::core::next_trim_entity_index(
                    {"arc-1", "arc-12", "arc-9"}) == 13,
                "trim ids: multi-digit suffixes");
}

// User-reported regression: trimming a corner circle keeps the mirrored
// (complementary) arc instead of the clicked arc.  Build the top-right
// corner of the part.json sketch — a circle tangent to two incident
// lines — trim the outer 3/4 of the circle, and verify the surviving
// arc is the inner quarter between the two tangent points.
bool test_circle_trim_keeps_clicked_complement() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_line(-45.0, 35.0, 40.11533804005704, 35.0);
  document = manager.add_sketch_line(50.0, 25.115338040056997,
                                     50.0, -25.512245812427125);
  document = manager.add_sketch_circle(50.0, 35.0, 9.884661959943005,
                                       /*is_construction=*/false);

  // Click the OUTER 3/4 arc (angle 0, the +x side of the circle) so the
  // trim deletes it and keeps the inner quarter (π → 3π/2).
  document = manager.trim_sketch_entity("circle-1", 59.884661959943, 35.0);

  const auto& params = document.feature_history.back().sketch_parameters.value();
  if (!expect(params.circles.empty(),
              "circle trim: circle must be converted to an arc")) {
    return false;
  }
  if (!expect(params.arcs.size() == 1,
              "circle trim: exactly one arc must remain")) {
    return false;
  }
  const auto& arc = params.arcs.front();
  std::cerr << "circle trim: arc s=(" << arc.start_x << "," << arc.start_y
            << ") e=(" << arc.end_x << "," << arc.end_y << ") ccw=" << arc.ccw
            << "\n";
  // Inner quarter: from the left tangent point (40.115, 35) to the
  // bottom tangent point (50, 25.115), CCW around (50, 35).
  return expect(std::abs(arc.start_x - 40.11533804005704) < 1e-6 &&
                    std::abs(arc.start_y - 35.0) < 1e-6,
                "circle trim: arc must start at the left tangent point") &&
         expect(std::abs(arc.end_x - 50.0) < 1e-6 &&
                    std::abs(arc.end_y - 25.115338040056997) < 1e-6,
                "circle trim: arc must end at the bottom tangent point");
}

// User-reported regression: the trim tool stopped working on CW arcs
// (part.json: arc-1 from (50,30) to (50,-30) through (78.38,0),
// ccw=false).  The CW sweep branches in the trim engine rejected
// intersections on the sweep portion crossing the +x axis and the
// segment-selection angle test used an empty interval, so a click on
// the arc threw "Click position does not correspond to any segment".
// Reproduce the sketch — the arc plus its two tangent lines and the
// horizontal leg ending on the arc — and verify clicks between the
// real intersections trim exactly the expected piece.
bool test_cw_arc_trim_splits_at_all_intersections() {
  const double kCx = 48.333333333333336;
  const double kCy = 0.0;
  const double kR = 30.046260628866577;
  const double kQuadX = 78.37959396219992;  // rightmost point on the arc
  const double kTanX = 58.181818181818215;  // upper tangent point
  const double kTanY = 28.38635453817456;

  // Click points as arc angles: the midpoint of the top segment
  // (arc start -> upper tangent) and — after that trim moves the arc
  // start down to the tangent — the midpoint of the middle segment
  // (rightmost point -> lower tangent), which sits below the +x axis.
  const double a_start = std::atan2(30.0, 50.0 - kCx);
  const double a_tan = std::atan2(kTanY, kTanX - kCx);
  const double top_click_a = 0.5 * (a_start + a_tan);
  const double top_click_x = kCx + kR * std::cos(top_click_a);
  const double top_click_y = kCy + kR * std::sin(top_click_a);
  const double right_click_a = -0.5 * a_tan;
  const double right_click_x = kCx + kR * std::cos(right_click_a);
  const double right_click_y = kCy + kR * std::sin(right_click_a);

  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(
      50.0, 30.0, 50.0, -30.0, kQuadX, 0.0, "three_point");
  document = manager.add_sketch_line(50.0, 0.0, kQuadX, 0.0);
  document = manager.add_sketch_line(140.0, 0.0, kTanX, kTanY);
  document = manager.add_sketch_line(140.0, 0.0, kTanX, -kTanY);

  // Sanity: the three-point arc must come out CW, like part.json.
  const auto& arcs_before =
      document.feature_history.back().sketch_parameters->arcs;
  if (!expect(arcs_before.size() == 1 && !arcs_before.front().ccw,
              "cw arc trim: three-point arc must be stored ccw=false")) {
    return false;
  }

  // Clicking the top segment (arc start -> upper tangent) deletes that
  // segment: the arc's start must move down to the tangent point.
  document = manager.trim_sketch_entity("arc-1", top_click_x, top_click_y);

  {
    const auto& params =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(params.arcs.size() == 1,
                "cw arc trim: one arc must remain after an end trim")) {
      return false;
    }
    const auto& arc = params.arcs.front();
    std::cerr << "cw arc trim: s=(" << arc.start_x << "," << arc.start_y
              << ") e=(" << arc.end_x << "," << arc.end_y << ") ccw=" << arc.ccw
              << "\n";
    if (!expect(std::abs(arc.start_x - kTanX) < 1e-6 &&
                    std::abs(arc.start_y - kTanY) < 1e-6,
                "cw arc trim: arc must start at the upper tangent point") ||
        !expect(std::abs(arc.end_x - 50.0) < 1e-6 &&
                    std::abs(arc.end_y + 30.0) < 1e-6,
                "cw arc trim: arc must still end at (50,-30)")) {
      return false;
    }
  }

  // Clicking the middle segment (rightmost point -> lower tangent)
  // splits the remaining arc into two: the upper piece ends at the
  // rightmost point, the lower piece starts at the lower tangent.
  document = manager.trim_sketch_entity("arc-1", right_click_x, right_click_y);

  {
    const auto& params =
        document.feature_history.back().sketch_parameters.value();
    if (!expect(params.arcs.size() == 2,
                "cw arc trim: middle trim must split the arc into two")) {
      return false;
    }
    const auto& upper = params.arcs.front();
    const auto& lower = params.arcs.back();
    return expect(std::abs(upper.start_x - kTanX) < 1e-6 &&
                      std::abs(upper.start_y - kTanY) < 1e-6 &&
                      std::abs(upper.end_x - kQuadX) < 1e-6 &&
                      std::abs(upper.end_y) < 1e-6,
                  "cw arc trim: upper piece must run tangent -> rightmost") &&
           expect(std::abs(lower.start_x - kTanX) < 1e-6 &&
                      std::abs(lower.start_y + kTanY) < 1e-6 &&
                      std::abs(lower.end_x - 50.0) < 1e-6 &&
                      std::abs(lower.end_y + 30.0) < 1e-6,
                  "cw arc trim: lower piece must run lower tangent -> (50,-30)");
  }
}

// User-reported regression: after trimming the arc-1 pieces between
// the two tangent lines, the remaining geometry — arc stubs from the
// rectangle corners to the tangent points, the two tangent lines, and
// the shared right edge — must still form a closed region next to the
// rectangle.  The leftover horizontal line whose far endpoint used to
// sit on the trimmed-away arc is now dangling into that region; it
// must not prevent the region from becoming a profile.
bool test_dangling_line_does_not_block_trimmed_region_profile() {
  const double kTanX = 58.181818181818215;
  const double kTanY = 28.38635453817456;

  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Rectangle + the leftover line-7 whose far end (78.38, 0) used to
  // lie on the now-trimmed arc.
  DocumentState document = manager.add_sketch_line(-50.0, 30.0, 50.0, 30.0);
  document = manager.add_sketch_line(50.0, 30.0, 50.0, -30.0);
  document = manager.add_sketch_line(50.0, -30.0, -50.0, -30.0);
  document = manager.add_sketch_line(-50.0, -30.0, -50.0, 30.0);
  document = manager.add_sketch_line(50.0, 0.0, 78.37959396219992, 0.0);
  // Tangent lines and the two arc stubs (center (48.3333, 0),
  // r = 30.0463, both CW).
  document = manager.add_sketch_line(140.0, 0.0, kTanX, kTanY);
  document = manager.add_sketch_line(140.0, 0.0, kTanX, -kTanY);
  document = manager.add_sketch_arc(50.0, 30.0, kTanX, kTanY,
                                    48.333333333333336, 0.0,
                                    "center_start_end");
  document = manager.add_sketch_arc(kTanX, -kTanY, 50.0, -30.0,
                                    48.333333333333336, 0.0,
                                    "center_start_end");

  const auto& params =
      document.feature_history.back().sketch_parameters.value();
  if (!expect(params.profiles.size() == 2,
              "dangling line: rectangle and trimmed region must both be profiles")) {
    return false;
  }

  // The trimmed region: tangent lines + both arc stubs + the shared
  // right edge of the rectangle.
  // In this document's id space the tangent lines are line-6/line-7
  // (line-5 is the dangling horizontal).
  const auto region_it = std::find_if(
      params.profiles.begin(), params.profiles.end(),
      [](const auto& p) {
        return std::find(p.line_ids.begin(), p.line_ids.end(), "line-6") !=
               p.line_ids.end();
      });
  if (!expect(region_it != params.profiles.end(),
              "dangling line: region profile must exist")) {
    return false;
  }
  std::set<std::string> edge_ids;
  for (const auto& be : region_it->boundary_edges) {
    edge_ids.insert(be.entity_id);
  }
  return expect(edge_ids.size() == 5 &&
                    edge_ids.count("line-2") && edge_ids.count("line-6") &&
                    edge_ids.count("line-7") && edge_ids.count("arc-1") &&
                    edge_ids.count("arc-2"),
                "dangling line: region boundary must be the two tangent lines, "
                "the two arc stubs and the right edge");
}

// User-reported regression: after saving and reloading the trimmed
// sketch, the surface next to the tangent lines was still missing —
// the loader restored the file's stale profile list verbatim instead
// of recomputing it.  Simulate a stale save (profiles stripped from
// the JSON) and verify the load path rebuilds them from geometry.
bool test_load_recomputes_stale_profiles() {
  const double kTanX = 58.181818181818215;
  const double kTanY = 28.38635453817456;

  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(-50.0, 30.0, 50.0, 30.0);
  document = manager.add_sketch_line(50.0, 30.0, 50.0, -30.0);
  document = manager.add_sketch_line(50.0, -30.0, -50.0, -30.0);
  document = manager.add_sketch_line(-50.0, -30.0, -50.0, 30.0);
  document = manager.add_sketch_line(50.0, 0.0, 78.37959396219992, 0.0);
  document = manager.add_sketch_line(140.0, 0.0, kTanX, kTanY);
  document = manager.add_sketch_line(140.0, 0.0, kTanX, -kTanY);
  document = manager.add_sketch_arc(50.0, 30.0, kTanX, kTanY,
                                    48.333333333333336, 0.0,
                                    "center_start_end");
  document = manager.add_sketch_arc(kTanX, -kTanY, 50.0, -30.0,
                                    48.333333333333336, 0.0,
                                    "center_start_end");

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_stale_profiles_reload_test.json";
  manager.save_document_to_path(path.string());

  // Strip the saved profile list — what an older build's file looks
  // like after the arrangement pipeline could not see a region.
  {
    std::ifstream in(path.string());
    nlohmann::json payload = nlohmann::json::parse(in);
    in.close();
    auto& history = payload["feature_history"];
    if (!expect(history.is_array() && !history.empty(),
                "stale reload: saved file must carry feature history")) {
      return false;
    }
    auto& sketch = history.back()["sketch_parameters"];
    sketch["profiles"] = nlohmann::json::array();
    std::ofstream out(path.string());
    out << payload.dump(2);
    out.close();
  }

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded =
      loaded_manager.load_document_from_path(path.string());

  const auto& params =
      loaded.feature_history.back().sketch_parameters.value();
  if (!expect(params.profiles.size() == 2,
              "stale reload: profiles must be recomputed on load")) {
    return false;
  }
  return expect(std::any_of(params.profiles.begin(), params.profiles.end(),
                            [](const auto& p) {
                              return std::find(p.line_ids.begin(),
                                               p.line_ids.end(),
                                               "line-6") != p.line_ids.end();
                            }),
                "stale reload: trimmed region must be a profile after load");
}

// User-reported regression: the part.json rounded rectangle — two
// filleted corners, a trim circle at the top-right corner tangent to
// both incident lines, a trim arc at the bottom-right, and an inner
// circle.  Profile detection highlights the full outer contour, but
// the extrude came out as a thin-walled partial prism.  This test
// extrudes the detected outer profile end-to-end and checks the prism
// volume against the analytic value.
bool test_trimmed_circle_corner_extrudes_full_prism() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Outer loop, CCW: (-45,35) -> (40,35) -> circle quarter
  // (center (50,35) r=9.8847) -> (50,25.1153) -> (50,-25.5122) -> arc
  // (center (50,-35) r=9.4878) -> (40.5122,-35) -> (-45,-35) -> arc
  // (center (-45,-30) r=5) -> (-50,-30) -> (-50,30) -> arc
  // (center (-45,30) r=5) -> (-45,35).
  DocumentState document =
      manager.add_sketch_line(-45.0, 35.0, 40.11533804005704, 35.0);
  document = manager.add_sketch_line(50.0, 25.115338040056997,
                                     50.0, -25.512245812427125);
  document = manager.add_sketch_line(-50.0, -30.0, -50.0, 30.0);
  document = manager.add_sketch_line(40.512245812427125, -35.0,
                                     -45.0, -35.0);

  document = manager.add_sketch_arc(-45.0, 35.0, -50.0, 30.0,
                                    -45.0, 30.0, "center_start_end");
  document = manager.add_sketch_arc(-45.0, -35.0, -50.0, -30.0,
                                    -45.0, -30.0, "center_start_end");
  document = manager.add_sketch_arc(50.0, -25.512245812427125,
                                    40.512245812427125, -35.0,
                                    50.0, -35.0, "center_start_end");
  document = manager.add_sketch_circle(50.0, 35.0, 9.884661959943005,
                                       /*is_construction=*/false);
  document = manager.add_sketch_circle(0.0, 0.0, 14.693138517113884,
                                       /*is_construction=*/false);

  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  std::cerr << "trim-extrude: profile count=" << profiles.size() << "\n";
  for (const auto& p : profiles) {
    std::cerr << "  ids:";
    for (const auto& id : p.line_ids) std::cerr << " " << id;
    std::cerr << " src=" << (p.source_circle_id ? *p.source_circle_id : "none")
              << " npts=" << p.points.size() << "\n";
  }

  // The outer polygon profile: the non-circle polygon (the inner circle
  // profile carries source_circle_id).
  const auto outer_it = std::find_if(
      profiles.begin(), profiles.end(),
      [](const auto& p) {
        return p.kind == "polygon" && !p.source_circle_id.has_value();
      });
  if (!expect(outer_it != profiles.end(),
              "trim-extrude: outer profile must be detected")) {
    return false;
  }

  document = manager.extrude_profile(outer_it->id, /*depth=*/10.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "trim-extrude: expected one compiled body")) {
    return false;
  }
  GProp_GProps props;
  BRepGProp::VolumeProperties(compiled.bodies.front().shape, props);
  const double volume = props.Mass();

  // Analytic area: rect 100x70 minus two r=5 corner fillets, minus two
  // quarter-circle corners (r=9.8847 and r=9.4878), minus the inner
  // circle (r=14.6931).
  constexpr double kPi = 3.14159265358979323846;
  const double expected_area =
      100.0 * 70.0 - 2.0 * (25.0 - 25.0 * kPi / 4.0) -
      (9.884661959943005 * 9.884661959943005) * (1.0 - kPi / 4.0) -
      (9.487754187572875 * 9.487754187572875) * (1.0 - kPi / 4.0) -
      kPi * 14.693138517113884 * 14.693138517113884;
  const double expected_volume = expected_area * 10.0;
  std::cerr << "trim-extrude: volume=" << volume
            << " expected=" << expected_volume << "\n";
  return expect(std::abs(volume - expected_volume) < expected_volume * 0.02,
                "trim-extrude: volume must match the rounded-rect slab");
}

}  // namespace



int main() {
  if (!test_circle_extrude_is_smooth()) return 1;
  if (!test_filleted_rectangle_extrudes_full_prism()) return 1;
  if (!test_curve_only_lens_faces_are_real_profiles()) return 1;
  if (!test_filleted_rectangle_clockwise_walk_extrudes_full_prism()) return 1;
  if (!test_rounded_rect_with_touching_lines_and_circle()) return 1;
  if (!test_concentric_circles_clean_bore()) return 1;
  if (!test_big_circle_selection_boss_with_holes()) return 1;
  if (!test_load_preserves_circle_center_vertex()) return 1;
  if (!test_trim_entity_ids_never_collide()) return 1;
  if (!test_circle_trim_keeps_clicked_complement()) return 1;
  if (!test_cw_arc_trim_splits_at_all_intersections()) return 1;
  if (!test_dangling_line_does_not_block_trimmed_region_profile()) return 1;
  if (!test_load_recomputes_stale_profiles()) return 1;
  if (!test_trimmed_circle_corner_extrudes_full_prism()) return 1;

  std::cout << "extrude_quality_test passed\n";
  return 0;
}
