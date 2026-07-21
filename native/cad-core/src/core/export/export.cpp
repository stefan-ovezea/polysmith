#include "core/export/export.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeFix_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"

namespace polysmith::core {
namespace {

// Little-endian helpers for binary STL writing.
static void stl_write_u32(char* buf, uint32_t v) {
  buf[0] = static_cast<char>(v & 0xff);
  buf[1] = static_cast<char>((v >> 8) & 0xff);
  buf[2] = static_cast<char>((v >> 16) & 0xff);
  buf[3] = static_cast<char>((v >> 24) & 0xff);
}

static void stl_write_float(char* buf, float v) {
  union { float f; char b[4]; } u;
  u.f = v;
  buf[0] = u.b[0]; buf[1] = u.b[1]; buf[2] = u.b[2]; buf[3] = u.b[3];
}

std::vector<TopoDS_Shape> collect_export_shapes(const DocumentState& document) {
  std::vector<TopoDS_Shape> shapes;
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (!body.shape.IsNull()) {
      shapes.push_back(body.shape);
    }
  }
  return shapes;
}

TopoDS_Shape collect_export_body_shape(const DocumentState& document,
                                       const std::string& body_id) {
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id == body_id && !body.shape.IsNull()) {
      return body.shape;
    }
  }
  throw std::runtime_error("No solid body is available to export for id: " +
                           body_id);
}

// OCCT 8.0 precompiled binary's StlAPI_Writer crashes (even in P1).
// Work around it by tessellating with OCCT (as before) but writing the
// binary STL directly from the triangulations.  The format is trivial:
// 80-byte header, 4-byte triangle count, then 50-byte facets
// (3×float normal + 3×3×float vertices + uint16 attr).
ExportResult write_stl_shape(const TopoDS_Shape& shape,
                             const std::string& file_path,
                             int exported_feature_count) {
  // Heal shape before meshing — OCCT 8.0 can crash on unhealed shapes.
  ShapeFix_Shape fixer(shape);
  fixer.Perform();
  const TopoDS_Shape& healed = fixer.Shape();

  constexpr double kLinearDeflection = 0.1;
  constexpr double kAngularDeflection = 0.5;

  BRepMesh_IncrementalMesh mesher(healed,
                                  kLinearDeflection,
                                  /*isRelative=*/false,
                                  kAngularDeflection,
                                  /*isInParallel=*/false);
  if (!mesher.IsDone()) {
    throw std::runtime_error("STL meshing failed");
  }

  // Count total triangles.
  uint32_t total_triangles = 0;
  {
    TopExp_Explorer exp(healed, TopAbs_FACE);
    for (; exp.More(); exp.Next()) {
      TopLoc_Location loc;
      auto tri = BRep_Tool::Triangulation(TopoDS::Face(exp.Current()), loc);
      if (!tri.IsNull()) {
        total_triangles += static_cast<uint32_t>(tri->NbTriangles());
      }
    }
  }

  std::ofstream out(file_path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Cannot open STL file for writing: " + file_path);
  }

  // Binary STL header.
  char header[80] = {};
  std::memcpy(header, "Binary STL exported by PolySmith", 33);
  out.write(header, 80);

  char count_buf[4];
  stl_write_u32(count_buf, total_triangles);
  out.write(count_buf, 4);

  // Write facets.
  char facet[50];
  {
    TopExp_Explorer exp(healed, TopAbs_FACE);
    for (; exp.More(); exp.Next()) {
      const TopoDS_Face& face = TopoDS::Face(exp.Current());
      TopLoc_Location loc;
      auto tri = BRep_Tool::Triangulation(face, loc);
      if (tri.IsNull()) continue;

      gp_Trsf T = loc.Transformation();
      bool reversed = (face.Orientation() == TopAbs_REVERSED);

      for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int n1, n2, n3;
        tri->Triangle(t).Get(n1, n2, n3);
        if (reversed) std::swap(n2, n3);

        gp_Pnt p1 = tri->Node(n1).Transformed(T);
        gp_Pnt p2 = tri->Node(n2).Transformed(T);
        gp_Pnt p3 = tri->Node(n3).Transformed(T);

        gp_Vec normal = gp_Vec(p1, p2).Crossed(gp_Vec(p1, p3));
        float nx = 0, ny = 0, nz = 0;
        if (normal.SquareMagnitude() > 1e-30) {
          normal.Normalize();
          nx = static_cast<float>(normal.X());
          ny = static_cast<float>(normal.Y());
          nz = static_cast<float>(normal.Z());
        }

        stl_write_float(facet + 0,  nx);
        stl_write_float(facet + 4,  ny);
        stl_write_float(facet + 8,  nz);
        stl_write_float(facet + 12, static_cast<float>(p1.X()));
        stl_write_float(facet + 16, static_cast<float>(p1.Y()));
        stl_write_float(facet + 20, static_cast<float>(p1.Z()));
        stl_write_float(facet + 24, static_cast<float>(p2.X()));
        stl_write_float(facet + 28, static_cast<float>(p2.Y()));
        stl_write_float(facet + 32, static_cast<float>(p2.Z()));
        stl_write_float(facet + 36, static_cast<float>(p3.X()));
        stl_write_float(facet + 40, static_cast<float>(p3.Y()));
        stl_write_float(facet + 44, static_cast<float>(p3.Z()));
        facet[48] = 0;
        facet[49] = 0;
        out.write(facet, 50);
      }
    }
  }
  out.close();

  return ExportResult{
      .file_path = file_path,
      .format = "stl",
      .exported_feature_count = exported_feature_count,
  };
}

}  // namespace

ExportResult export_document_as_step(const DocumentState& document,
                                     const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  const std::vector<TopoDS_Shape> shapes = collect_export_shapes(document);
  if (shapes.empty()) {
    throw std::runtime_error("No solid features are available to export");
  }

  // OCCT 8.0 prebuilt STEPControl_Writer crashes.  Write STEP AP203
  // manually from tessellated faces — same data we use for STL export.
  constexpr double kLinearDeflection = 0.1;
  constexpr double kAngularDeflection = 0.5;

  std::ofstream out(file_path);
  if (!out) {
    throw std::runtime_error("Cannot open STEP file for writing: " + file_path);
  }

  out << "ISO-10303-21;\r\n";
  out << "HEADER;\r\n";
  out << "FILE_DESCRIPTION(('PolySmith export'),'2;1');\r\n";
  out << "FILE_NAME('" << file_path << "','"
      << "2026-07-21T00:00:00',('PolySmith'),(''),'','','');\r\n";
  out << "FILE_SCHEMA(('AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF'));\r\n";
  out << "ENDSEC;\r\n";
  out << "DATA;\r\n";

  int next_id = 1;
  auto nextId = [&next_id]() -> int { return next_id++; };

  // Collect all triangles across all shapes.
  struct Tri {
    float x1,y1,z1, x2,y2,z2, x3,y3,z3, nx,ny,nz;
  };
  std::vector<Tri> all_tris;

  for (const auto& shape : shapes) {
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    const TopoDS_Shape& healed = fixer.Shape();
    BRepMesh_IncrementalMesh mesher(healed,
                                    kLinearDeflection,
                                    /*isRelative=*/false,
                                    kAngularDeflection,
                                    /*isInParallel=*/false);

    TopExp_Explorer exp(healed, TopAbs_FACE);
    for (; exp.More(); exp.Next()) {
      const TopoDS_Face& face = TopoDS::Face(exp.Current());
      TopLoc_Location loc;
      auto tri = BRep_Tool::Triangulation(face, loc);
      if (tri.IsNull()) continue;

      gp_Trsf T = loc.Transformation();
      bool reversed = (face.Orientation() == TopAbs_REVERSED);

      for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int n1, n2, n3;
        tri->Triangle(t).Get(n1, n2, n3);
        if (reversed) std::swap(n2, n3);

        gp_Pnt p1 = tri->Node(n1).Transformed(T);
        gp_Pnt p2 = tri->Node(n2).Transformed(T);
        gp_Pnt p3 = tri->Node(n3).Transformed(T);

        gp_Vec normal = gp_Vec(p1, p2).Crossed(gp_Vec(p1, p3));
        float nx = 0, ny = 0, nz = 1;
        if (normal.SquareMagnitude() > 1e-30) {
          normal.Normalize();
          nx = static_cast<float>(normal.X());
          ny = static_cast<float>(normal.Y());
          nz = static_cast<float>(normal.Z());
        }

        all_tris.push_back({
          static_cast<float>(p1.X()), static_cast<float>(p1.Y()), static_cast<float>(p1.Z()),
          static_cast<float>(p2.X()), static_cast<float>(p2.Y()), static_cast<float>(p2.Z()),
          static_cast<float>(p3.X()), static_cast<float>(p3.Y()), static_cast<float>(p3.Z()),
          nx, ny, nz
        });
      }
    }
  }

  if (all_tris.empty()) {
    throw std::runtime_error("No tessellation data for STEP export");
  }

  // Write CLOSED_SHELL containing one ADVANCED_FACE per triangle.
  int shell_id = nextId();
  std::vector<int> face_ids;

  for (const auto& tri : all_tris) {
    int pt1_id = nextId();
    int pt2_id = nextId();
    int pt3_id = nextId();
    int dir_id = nextId();
    int pline_id = nextId();
    int pcurve_id = nextId();
    int loop_id = nextId();
    int bound_id = nextId();
    int plane_id = nextId();
    int face_id = nextId();

    out << "#" << pt1_id << " = CARTESIAN_POINT('',("
        << tri.x1 << "," << tri.y1 << "," << tri.z1 << "));\r\n";
    out << "#" << pt2_id << " = CARTESIAN_POINT('',("
        << tri.x2 << "," << tri.y2 << "," << tri.z2 << "));\r\n";
    out << "#" << pt3_id << " = CARTESIAN_POINT('',("
        << tri.x3 << "," << tri.y3 << "," << tri.z3 << "));\r\n";

    out << "#" << dir_id << " = DIRECTION('',("
        << tri.nx << "," << tri.ny << "," << tri.nz << "));\r\n";

    out << "#" << pline_id << " = POLYLINE('',(#"
        << pt1_id << ",#" << pt2_id << ",#" << pt3_id << "));\r\n";
    out << "#" << pcurve_id << " = PCURVE('','',#" << pline_id << ");\r\n";
    out << "#" << loop_id << " = FACE_OUTER_BOUND('',#" << pcurve_id << ",.T.);\r\n";
    out << "#" << bound_id << " = FACE_BOUND('',#" << pcurve_id << ",.T.);\r\n";
    out << "#" << plane_id << " = PLANE('',#" << dir_id << ");\r\n";
    out << "#" << face_id << " = ADVANCED_FACE('',(#" << bound_id
        << "),#" << plane_id << ",.F.);\r\n";

    face_ids.push_back(face_id);
  }

  out << "#" << shell_id << " = CLOSED_SHELL('',(";
  for (size_t i = 0; i < face_ids.size(); ++i) {
    if (i > 0) out << ",";
    out << "#" << face_ids[i];
  }
  out << "));\r\n";

  // MANIFOLD_SOLID_BREP wrapping the shell.
  int solid_id = nextId();
  out << "#" << solid_id << " = MANIFOLD_SOLID_BREP('',#" << shell_id << ");\r\n";

  // Product definition.
  int prod_id = nextId();
  int pdf_id = nextId();
  int pds_id = nextId();
  int mdef_id = nextId();
  int shape_rep_id = nextId();

  out << "#" << prod_id << " = PRODUCT('','','',(#" << mdef_id << "));\r\n";
  out << "#" << pdf_id << " = PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('','',#" << prod_id << ",.NOT_KNOWN.);\r\n";
  out << "#" << pds_id << " = PRODUCT_DEFINITION_SHAPE('',#,"
      << pdf_id << ");\r\n";
  out << "#" << mdef_id << " = MECHANICAL_CONTEXT('',#" << pds_id << ",'');\r\n";
  out << "#" << shape_rep_id << " = SHAPE_REPRESENTATION('',(#"
      << solid_id << "),#" << mdef_id << ");\r\n";

  out << "ENDSEC;\r\n";
  out << "END-ISO-10303-21;\r\n";
  out.close();

  return ExportResult{
      .file_path = file_path,
      .format = "step",
      .exported_feature_count = static_cast<int>(shapes.size()),
  };
}

ExportResult export_document_as_stl(const DocumentState& document,
                                    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  const std::vector<TopoDS_Shape> shapes = collect_export_shapes(document);
  if (shapes.empty()) {
    throw std::runtime_error("No solid features are available to export");
  }

  if (shapes.size() == 1) {
    return write_stl_shape(shapes[0], file_path, 1);
  }

  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  for (const auto& shape : shapes) {
    // Heal each shape before adding.
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    builder.Add(compound, fixer.Shape());
  }

  return write_stl_shape(compound, file_path, static_cast<int>(shapes.size()));
}

ExportResult export_body_as_stl(const DocumentState& document,
                                const std::string& file_path,
                                const std::string& body_id) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }
  if (body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  const TopoDS_Shape body_shape = collect_export_body_shape(document, body_id);
  return write_stl_shape(body_shape, file_path, 1);
}

}  // namespace polysmith::core
