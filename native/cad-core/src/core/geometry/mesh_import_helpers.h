#pragma once

#include <TopoDS_Shape.hxx>

#include "core/document/body_feature_types.h"

namespace polysmith::core {

// Reads the STL file referenced by `params` and returns the compound of
// planar triangle faces StlAPI_Reader produces (shared edges, no
// sewing). Returns a null shape when the file is missing, unreadable,
// or malformed — callers treat a null shape as "body absent".
TopoDS_Shape build_mesh_import_shape(const MeshImportFeatureParameters& params);

// Converts a mesh-import compound into a regular body: sew triangles
// into shells, make one solid per closed shell (multi-part STLs become
// a compound of solids), heal, then merge coplanar facets so the
// result behaves like a normal solid. Returns a null shape when the
// mesh is not watertight or the conversion fails.
TopoDS_Shape convert_mesh_to_solid(const TopoDS_Shape& mesh_shape);

}  // namespace polysmith::core
