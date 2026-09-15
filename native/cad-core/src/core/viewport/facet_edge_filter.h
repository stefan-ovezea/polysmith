#pragma once

// Semantic-edge filter for faceted (mesh-converted) solids.
//
// Mesh-converted bodies keep every triangle edge as a solid edge
// (~10k edges / ~2MB of JSON per viewport refresh measured on a fan
// panel — the reason mesh bodies originally emitted no per-edge pick
// entries). The edges a user actually wants to pick and project are
// the semantic ones: the part's outline, steps, and hole rims. An
// edge shared by two COPLANAR faces is a triangulation seam on a flat
// region — dropped.
//
// Small meshes have their coplanar faces unified at conversion time
// (ShapeUpgrade_UnifySameDomain), so this filter is a no-op there; it
// exists for parts above the unify face-count limit (or whose
// unification failed) — the exact case that previously flooded the
// viewport payload.
//
// STL-derived faces are planar, so the plane normal is exact and
// cheap; non-planar faces fall back to a surface-normal probe at the
// parametric midpoint.

#include <cmath>

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomLProp_SLProps.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

namespace polysmith::core {

class FacetEdgeFilter {
 public:
  // OCCT 8 removed the TopTools_* collection typedefs; the ancestor
  // map is the raw NCollection type MapShapesAndAncestors fills.
  using FaceListMap = NCollection_IndexedDataMap<
      TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>;

  explicit FacetEdgeFilter(const TopoDS_Shape& body_shape) {
    if (body_shape.IsNull()) {
      return;
    }
    TopExp::MapShapesAndAncestors(body_shape, TopAbs_EDGE, TopAbs_FACE,
                                  edge_faces_);
    // Per-face normal cache: computed ONCE per face instead of once
    // per edge-keep check — a converted body shares a few thousand
    // edges across a few hundred faces, and building a surface
    // adaptor per check dominated the viewport rebuild (measured
    // ~8s on a 3.5k-edge part).
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
    TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);
    for (int i = 1; i <= face_map.Extent(); ++i) {
      const TopoDS_Face face = TopoDS::Face(face_map(i));
      if (!face.IsNull()) {
        face_normals_.Bind(face_map(i), face_normal(face));
      }
    }
  }

  // Keep boundary edges (fewer than 2 adjacent faces) and edges
  // separating faces whose normals differ by more than 0.5°.
  bool keep(const TopoDS_Edge& edge) const {
    if (edge.IsNull()) {
      return false;
    }
    const NCollection_List<TopoDS_Shape>* faces = edge_faces_.Seek(edge);
    if (faces == nullptr || faces->Size() < 2) {
      return true;  // boundary edge — always semantic
    }
    NCollection_List<TopoDS_Shape>::Iterator iterator(*faces);
    const TopoDS_Face face_a = TopoDS::Face(iterator.Value());
    iterator.Next();
    const TopoDS_Face face_b = TopoDS::Face(iterator.Value());
    if (face_a.IsNull() || face_b.IsNull()) {
      return true;
    }
    const double dot =
        std::abs(cached_normal(face_a).Dot(cached_normal(face_b)));
    // cos(0.5°) — facet pairs closer than half a degree read as one
    // flat region; coarser facets (e.g. chorded cylinders) stay
    // pickable.
    return dot < 0.99996;
  }

 private:
  gp_Dir cached_normal(const TopoDS_Face& face) const {
    const gp_Dir* cached = face_normals_.Seek(face);
    return cached != nullptr ? *cached : face_normal(face);
  }

  static gp_Dir face_normal(const TopoDS_Face& face) {
    try {
      BRepAdaptor_Surface surface(face);
      if (surface.GetType() == GeomAbs_Plane) {
        return surface.Plane().Axis().Direction();
      }
      const double u =
          0.5 * (surface.FirstUParameter() + surface.LastUParameter());
      const double v =
          0.5 * (surface.FirstVParameter() + surface.LastVParameter());
      GeomLProp_SLProps props(surface.Surface().Surface(), u, v, 1, 1e-4);
      if (props.IsNormalDefined()) {
        return props.Normal();
      }
    } catch (const std::exception&) {
    }
    return gp_Dir(0.0, 0.0, 1.0);
  }

  FaceListMap edge_faces_;
  NCollection_DataMap<TopoDS_Shape, gp_Dir, TopTools_ShapeMapHasher>
      face_normals_;
};

}  // namespace polysmith::core
