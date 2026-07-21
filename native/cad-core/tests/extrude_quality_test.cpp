// Regression test: extrude a circle profile and verify the resulting body
// contains an analytic cylindrical face (Geom_CylindricalSurface), not a
// polygon-approximated face.  This guards against accidental reintroduction
// of a raw BRepPrimAPI_MakeCylinder shortcut that bypasses the unified
// make_polygon_prism_shape path and breaks multi-profile / thin-wall /
// plane-frame coordinate mapping.

#include <iostream>

#include <BRep_Tool.hxx>
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

}  // namespace

int main() {
  if (!test_circle_extrude_is_smooth()) return 1;

  std::cout << "extrude_quality_test passed\n";
  return 0;
}
