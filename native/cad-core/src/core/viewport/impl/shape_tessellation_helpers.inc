// Tessellate `shape` into flat positions/normals/indices arrays. Used
// for the cut-preview emission below, which renders a translucent
// "this is what's about to be cut" volume so the user can see exactly
// which material the cut extrude will remove.
//
// The output is intentionally minimal compared to body_compiler's
// tessellate_shape: per-vertex normals are computed in a single pass
// (one normal per vertex, summed across triangles) without seam-
// splitting. The cut preview is a UI overlay so visual fidelity is
// less important than getting it on screen at all.
void tessellate_shape_to_arrays(const TopoDS_Shape& shape,
                                std::vector<double>& positions,
                                std::vector<double>& normals,
                                std::vector<int>& indices) {
  if (shape.IsNull()) {
    return;
  }
  try {
    BRepMesh_IncrementalMesh mesher(shape,
                                    /*deflection=*/0.1,
                                    /*isRelative=*/false,
                                    /*angularDeflection=*/0.5,
                                    /*isInParallel=*/false);
    if (!mesher.IsDone()) {
      return;
    }
  } catch (const std::exception&) {
    return;
  }

  for (TopExp_Explorer face_explorer(shape, TopAbs_FACE);
       face_explorer.More();
       face_explorer.Next()) {
    const TopoDS_Face face = TopoDS::Face(face_explorer.Current());
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation> triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }
    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int base_index = static_cast<int>(positions.size() / 3);
    const int node_count = triangulation->NbNodes();
    for (int i = 1; i <= node_count; ++i) {
      gp_Pnt node = triangulation->Node(i);
      node.Transform(transform);
      positions.push_back(node.X());
      positions.push_back(node.Y());
      positions.push_back(node.Z());
      normals.push_back(0.0);
      normals.push_back(0.0);
      normals.push_back(0.0);
    }
    const int triangle_count = triangulation->NbTriangles();
    for (int t = 1; t <= triangle_count; ++t) {
      const Poly_Triangle& triangle = triangulation->Triangle(t);
      int n1 = 0;
      int n2 = 0;
      int n3 = 0;
      triangle.Get(n1, n2, n3);
      if (reversed) {
        std::swap(n2, n3);
      }
      const int i1 = base_index + (n1 - 1);
      const int i2 = base_index + (n2 - 1);
      const int i3 = base_index + (n3 - 1);
      indices.push_back(i1);
      indices.push_back(i2);
      indices.push_back(i3);
      // Accumulate per-triangle normal onto each vertex so the simple
      // viewer flat-shading pass has something to work with.
      const double ax = positions[3 * i1 + 0];
      const double ay = positions[3 * i1 + 1];
      const double az = positions[3 * i1 + 2];
      const double bx = positions[3 * i2 + 0];
      const double by = positions[3 * i2 + 1];
      const double bz = positions[3 * i2 + 2];
      const double cx = positions[3 * i3 + 0];
      const double cy = positions[3 * i3 + 1];
      const double cz = positions[3 * i3 + 2];
      const double ux = bx - ax;
      const double uy = by - ay;
      const double uz = bz - az;
      const double vx = cx - ax;
      const double vy = cy - ay;
      const double vz = cz - az;
      const double nx = uy * vz - uz * vy;
      const double ny = uz * vx - ux * vz;
      const double nz = ux * vy - uy * vx;
      const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
      const double inv = length > 0.0 ? 1.0 / length : 0.0;
      const double nxn = nx * inv;
      const double nyn = ny * inv;
      const double nzn = nz * inv;
      for (int idx : {i1, i2, i3}) {
        normals[3 * idx + 0] += nxn;
        normals[3 * idx + 1] += nyn;
        normals[3 * idx + 2] += nzn;
      }
    }
  }
}
