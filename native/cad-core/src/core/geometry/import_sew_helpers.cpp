#include "core/geometry/import_sew_helpers.h"

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <ShapeFix_Solid.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

namespace polysmith::core {

TopoDS_Shape sew_faces_to_solids(const TopoDS_Shape& shape, double tolerance) {
  if (shape.IsNull()) {
    return TopoDS_Shape();
  }

  BRepBuilderAPI_Sewing sewer(tolerance);
  sewer.Add(shape);
  sewer.Perform();
  const TopoDS_Shape& sewn = sewer.SewedShape();
  if (sewn.IsNull()) {
    return TopoDS_Shape();
  }

  BRep_Builder builder;
  TopoDS_Compound solids;
  builder.MakeCompound(solids);
  bool any_solid = false;
  for (TopExp_Explorer shell_exp(sewn, TopAbs_SHELL); shell_exp.More();
       shell_exp.Next()) {
    BRepBuilderAPI_MakeSolid solid_maker(
        TopoDS::Shell(shell_exp.Current()));
    if (!solid_maker.IsDone()) {
      continue;  // open shell — not watertight
    }
    ShapeFix_Solid fixer(solid_maker.Solid());
    fixer.Perform();
    TopoDS_Solid solid = TopoDS::Solid(fixer.Solid());
    // Imported face orientations are frequently inconsistent — a sewn
    // solid can come out inverted (negative volume), which breaks
    // booleans downstream. Normalize per solid.
    GProp_GProps props;
    BRepGProp::VolumeProperties(solid, props);
    if (props.Mass() < 0.0) {
      solid.Reverse();
    }
    builder.Add(solids, solid);
    any_solid = true;
  }
  if (!any_solid) {
    return TopoDS_Shape();
  }
  return solids;
}

}  // namespace polysmith::core
