#include "core/geometry/mesh_import_helpers.h"

#include <cmath>
#include <filesystem>
#include <string>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <ShapeFix_Solid.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <StlAPI_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "core/diagnostics/logger.h"

namespace polysmith::core {
namespace {

// Above this face count the coplanar-facet merge is skipped — it can
// take minutes on very large meshes and the solid stays valid (just
// faceted) without it.
constexpr int kMaxUnifyFaceCount = 50000;

int count_faces(const TopoDS_Shape& shape) {
  int count = 0;
  for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    ++count;
  }
  return count;
}

}  // namespace

TopoDS_Shape build_mesh_import_shape(const MeshImportFeatureParameters& params) {
  if (params.file_path.empty() ||
      !std::filesystem::exists(params.file_path)) {
    return TopoDS_Shape();
  }

  TopoDS_Shape shape;
  try {
    StlAPI_Reader reader;
    if (!reader.Read(shape, params.file_path.c_str()) || shape.IsNull()) {
      return TopoDS_Shape();
    }
  } catch (const Standard_Failure& failure) {
    log_warn("mesh_import",
             std::string("Failed to read STL '") + params.file_path +
                 "': " + failure.GetMessageString());
    return TopoDS_Shape();
  } catch (const std::exception& error) {
    log_warn("mesh_import",
             std::string("Failed to read STL '") + params.file_path +
                 "': " + error.what());
    return TopoDS_Shape();
  }

  if (std::abs(params.scale - 1.0) > 1e-12 && params.scale > 0.0) {
    gp_Trsf transform;
    transform.SetScale(gp_Pnt(0.0, 0.0, 0.0), params.scale);
    shape = BRepBuilderAPI_Transform(shape, transform).Shape();
  }

  // Guarantee every face carries a triangulation. StlAPI_Reader's
  // MakeShapeOnMesh output builds planar faces WITHOUT triangulations,
  // and downstream consumers (the mesh-native silhouette, the viewport
  // tessellation, exports) read face triangulations directly — an
  // unmeshed shape silently produces empty results there.
  try {
    BRepMesh_IncrementalMesh mesher(shape, /*linearDeflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/false);
    (void)mesher;
  } catch (const Standard_Failure&) {
    // Meshing failed — return the shape as-is and let consumers
    // degrade instead of failing the import.
  }
  return shape;
}

TopoDS_Shape convert_mesh_to_solid(const TopoDS_Shape& mesh_shape) {
  if (mesh_shape.IsNull()) {
    return TopoDS_Shape();
  }

  // STL stores float32 coordinates; 1e-6 is a safe sew tolerance for
  // unit-scale (mm) parts.
  BRepBuilderAPI_Sewing sewer(1e-6);
  sewer.Add(mesh_shape);
  sewer.Perform();
  const TopoDS_Shape& sewn = sewer.SewedShape();
  if (sewn.IsNull()) {
    return TopoDS_Shape();
  }

  // One solid per closed shell — multi-part STLs become a compound of
  // solids, which the body compiler already handles.
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
    // STL winding is usually outward but not guaranteed — a sewn solid
    // can come out inverted (negative volume), which breaks booleans
    // downstream. Normalize per solid (same rule as the IGES/STEP
    // face-sewing imports).
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

  TopoDS_Shape result = solids;
  const int face_count = count_faces(result);
  if (face_count <= kMaxUnifyFaceCount) {
    try {
      // Merge coplanar triangles into real faces (the Fusion
      // "Convert Mesh" behaviour). Guarded by the face-count limit
      // above; on failure the faceted solid is kept.
      ShapeUpgrade_UnifySameDomain unifier(result, /*unifyFaces=*/true,
                                           /*unifyEdges=*/true,
                                           /*unifyFacesConcat=*/true);
      unifier.AllowInternalEdges(true);
      unifier.Build();
      result = unifier.Shape();
    } catch (const Standard_Failure&) {
      log_warn("mesh_import",
               "Face unification failed; keeping faceted solid.");
    }
  } else {
    log_warn("mesh_import",
             "Skipping face unification on " + std::to_string(face_count) +
                 " faces (above limit); converted body stays faceted.");
  }

  // UnifySameDomain rebuilds faces and drops their triangulations.
  // Mesh the result NOW so the creation-time snapshot (and every
  // later deserialize) carries triangulations — otherwise every body
  // compile re-meshes the whole converted solid (measured at ~830 ms
  // for an 1182-face panel vs ~70 ms when meshed).
  try {
    BRepMesh_IncrementalMesh mesher(result, /*linearDeflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/false);
    (void)mesher;
  } catch (const Standard_Failure&) {
    // Consumers re-mesh as needed — keep the unmeshed solid.
  }
  return result;
}

}  // namespace polysmith::core
