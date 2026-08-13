// Regression test: extrude a circle profile and verify the resulting body
// contains an analytic cylindrical face (Geom_CylindricalSurface), not a
// polygon-approximated face.  This guards against accidental reintroduction
// of a raw BRepPrimAPI_MakeCylinder shortcut that bypasses the unified
// make_polygon_prism_shape path and breaks multi-profile / thin-wall /
// plane-frame coordinate mapping.

#include <cmath>
#include <iostream>

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
// and a remnant arc left over from trimming another circle.  The
// arrangement must NOT emit lens faces bounded by arc+circle curve
// segments — the extrudable profiles are the two tangent wedges plus
// the circle itself.
bool test_curve_only_lens_faces_are_filtered() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  manager.add_sketch_line(1, -100.0, 0.0, -5.940552660969672, 23.638212711184707);
  manager.add_sketch_line(2, -100.0, 0.0, -5.940552660969672, -23.638212711184707);
  DocumentState document =
      manager.add_sketch_line(3, -100.0, 0.0, -24.37324898524955, 0.0);
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

  for (const auto& p : profiles) {
    const bool has_arc = std::any_of(
        p.line_ids.begin(), p.line_ids.end(),
        [](const std::string& id) { return id.rfind("arc-", 0) == 0; });
    const bool has_circle = std::any_of(
        p.line_ids.begin(), p.line_ids.end(),
        [](const std::string& id) { return id.rfind("circle-", 0) == 0; });
    if (has_arc && has_circle) {
      return expect(false,
                    "lens filter: no profile may mix arc and circle edges");
    }
  }

  // The circle interior profile must still exist.
  const bool has_circle_profile = std::any_of(
      profiles.begin(), profiles.end(),
      [](const auto& p) { return p.source_circle_id.has_value(); });
  return expect(has_circle_profile,
                "lens filter: circle interior profile must survive");
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
  document = manager.add_sketch_line(13, -100.0, 0.0, -48.54, 23.54);
  document = manager.add_sketch_line(14, -100.0, 0.0, -48.54, -23.54);
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



}  // namespace



int main() {
  if (!test_circle_extrude_is_smooth()) return 1;
  if (!test_filleted_rectangle_extrudes_full_prism()) return 1;
  if (!test_curve_only_lens_faces_are_filtered()) return 1;
  if (!test_filleted_rectangle_clockwise_walk_extrudes_full_prism()) return 1;
  if (!test_rounded_rect_with_touching_lines_and_circle()) return 1;
  if (!test_concentric_circles_clean_bore()) return 1;
  if (!test_big_circle_selection_boss_with_holes()) return 1;

  std::cout << "extrude_quality_test passed\n";
  return 0;
}
