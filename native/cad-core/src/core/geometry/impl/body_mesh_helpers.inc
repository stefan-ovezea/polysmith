void tessellate_shape(const TopoDS_Shape& shape, BodyMesh& out) {
  if (shape.IsNull()) {
    return;
  }

  // The linear tolerance is ABSOLUTE (0.1 mm), which is right for the board
  // and ruinous for its parts: a 0.4 mm castellated castellation or a 0.6 mm
  // pin leg may then deviate by a quarter of its own size, and the ribbon of
  // gold half-cylinders flattens into a stepped bar. Scale the tolerance with
  // the body, so a 1 mm feature is meshed at 10 um while the 72 mm board
  // still gets the coarse 0.1 mm it can afford.
  double linear_deflection = kLinearDeflection;
  {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (!bounds.IsVoid()) {
      const double diagonal = bounds.CornerMin().Distance(bounds.CornerMax());
      if (diagonal > 0.0) {
        linear_deflection =
            std::clamp(diagonal * kRelativeLinearDeflection,
                       kMinLinearDeflection, kLinearDeflection);
      }
    }
  }

  BRepMesh_IncrementalMesh mesher(shape,
                                  linear_deflection,
                                  /*isRelative=*/false,
                                  kAngularDeflection,
                                  /*isInParallel=*/false);
  if (!mesher.IsDone()) {
    throw std::runtime_error("BRepMesh_IncrementalMesh failed");
  }

  int face_ordinal = 0;
  for (TopExp_Explorer face_explorer(shape, TopAbs_FACE);
       face_explorer.More();
       face_explorer.Next()) {
    const TopoDS_Face& face = TopoDS::Face(face_explorer.Current());
    // Face ordinal in TopExp::MapShapes order (the id the viewport and the
    // per-face colour table both use). Counted before the triangulation
    // check so a skipped face does not shift the ids.
    const int this_face = face_ordinal++;
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation> triangulation =
        BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull()) {
      continue;
    }

    const gp_Trsf transform = location.Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const int base_index = static_cast<int>(out.vertices.size() / 3);
    const int node_count = triangulation->NbNodes();

    // Append every node of the face's triangulation, transformed into
    // world space.
    for (int node_index = 1; node_index <= node_count; ++node_index) {
      gp_Pnt node = triangulation->Node(node_index);
      node.Transform(transform);
      out.vertices.push_back(node.X());
      out.vertices.push_back(node.Y());
      out.vertices.push_back(node.Z());
      out.vertex_faces.push_back(this_face);
    }

    // Append placeholder normals; we'll fill them in below per triangle.
    for (int node_index = 1; node_index <= node_count; ++node_index) {
      out.normals.push_back(0.0);
      out.normals.push_back(0.0);
      out.normals.push_back(0.0);
    }

    const int triangle_count = triangulation->NbTriangles();
    for (int tri_index = 1; tri_index <= triangle_count; ++tri_index) {
      const Poly_Triangle& triangle = triangulation->Triangle(tri_index);
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
      out.indices.push_back(i1);
      out.indices.push_back(i2);
      out.indices.push_back(i3);

      // Compute a per-triangle normal and accumulate onto each vertex
      // of the triangle. After processing all triangles we leave the
      // normals un-renormalized; three.js's flat shading material works
      // either way and the magnitudes don't drive lighting anyway.
      const auto get = [&](int vertex_index) {
        return std::array<double, 3>{
            out.vertices[3 * vertex_index + 0],
            out.vertices[3 * vertex_index + 1],
            out.vertices[3 * vertex_index + 2],
        };
      };
      const auto a = get(i1);
      const auto b = get(i2);
      const auto c = get(i3);
      const double ux = b[0] - a[0];
      const double uy = b[1] - a[1];
      const double uz = b[2] - a[2];
      const double vx = c[0] - a[0];
      const double vy = c[1] - a[1];
      const double vz = c[2] - a[2];
      const double nx = uy * vz - uz * vy;
      const double ny = uz * vx - ux * vz;
      const double nz = ux * vy - uy * vx;
      const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
      const double inv = length > 0.0 ? 1.0 / length : 0.0;
      const double normalized_x = nx * inv;
      const double normalized_y = ny * inv;
      const double normalized_z = nz * inv;
      for (int vi : {i1, i2, i3}) {
        out.normals[3 * vi + 0] += normalized_x;
        out.normals[3 * vi + 1] += normalized_y;
        out.normals[3 * vi + 2] += normalized_z;
      }
    }
  }

  // Renormalize. The accumulation above sums one UNIT normal per incident
  // triangle, so a vertex shared by six triangles comes out six times too
  // long — and shaders do not normalize what they are handed. That produced
  // lighting proportional to local triangle density: bright, flat, washed-out
  // parts (the gold castellations reading as one orange ribbon) with no safe
  // way to tell it from a colour problem. Each face's vertices are separate,
  // so this still gives crisp per-face normals on flat faces and smooth
  // shading across a curved one.
  for (std::size_t i = 0; i + 2 < out.normals.size(); i += 3) {
    const double nx = out.normals[i];
    const double ny = out.normals[i + 1];
    const double nz = out.normals[i + 2];
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 1e-12) {
      out.normals[i] = nx / length;
      out.normals[i + 1] = ny / length;
      out.normals[i + 2] = nz / length;
    } else {
      // Degenerate fan (a sliver triangle): point up rather than send a zero
      // vector, which renders black under every light.
      out.normals[i] = 0.0;
      out.normals[i + 1] = 0.0;
      out.normals[i + 2] = 1.0;
    }
  }
}

// Parse the trailing index out of an edge id formatted as
// "<owner_body_id>:edge:<index>". Returns -1 if the id doesn't match the
// expected shape — callers treat that as "skip this edge" rather than
