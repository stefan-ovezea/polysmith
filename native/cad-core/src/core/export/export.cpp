#include "core/export/export.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <gp_Trsf.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"

namespace polysmith::core {
namespace {

// Little-endian 32-bit integer to buf (for STL triangle count header).
static void stl_write_u32(char* buf, uint32_t v) {
  buf[0] = static_cast<char>(v & 0xff);
  buf[1] = static_cast<char>((v >> 8) & 0xff);
  buf[2] = static_cast<char>((v >> 16) & 0xff);
  buf[3] = static_cast<char>((v >> 24) & 0xff);
}

// Little-endian 32-bit float to buf (for STL normals and vertices).
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

ExportResult write_stl_shape(const TopoDS_Shape& shape,
                             const std::string& file_path,
                             int exported_feature_count) {
  constexpr double kLinearDeflection = 0.1;
  constexpr double kAngularDeflection = 0.5;

  // OCCT 8.0: the precompiled binary's stream-based StlAPI_Writer and
  // RWStl::WriteBinary crash (confirmed via diagnostics — tessellation
  // succeeds, Write() kills the process).  OCCT 8.0 refactored STL I/O
  // from OSD_Path to std::ofstream; the precompiled binary has a bug in
  // this new code path.  Work around it by tessellating with OCCT (as
  // before) but writing the binary STL directly from the triangulations.
  // The format is trivial — 80-byte header, 4-byte triangle count, then
  // 50-byte facets (3×float normal + 3×3×float vertices + uint16 attr).
  BRepMesh_IncrementalMesh mesher(shape,
                                  kLinearDeflection,
                                  /*isRelative=*/false,
                                  kAngularDeflection,
                                  /*isInParallel=*/false);
  if (!mesher.IsDone()) {
    throw std::runtime_error("STL meshing failed");
  }

  uint32_t total_triangles = 0;
  {
    TopExp_Explorer exp(shape, TopAbs_FACE);
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

  char header[80] = {};
  std::memcpy(header, "Binary STL exported by PolySmith", 33);
  out.write(header, 80);

  char count_buf[4];
  stl_write_u32(count_buf, total_triangles);
  out.write(count_buf, 4);

  char facet[50];
  uint32_t written = 0;
  {
    TopExp_Explorer exp(shape, TopAbs_FACE);
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

        // Compute face normal.
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
        ++written;
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

  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  for (const auto& shape : shapes) {
    builder.Add(compound, shape);
  }

  STEPControl_Writer writer;
  const IFSelect_ReturnStatus transfer_status =
      writer.Transfer(compound, STEPControl_AsIs);
  if (transfer_status != IFSelect_RetDone) {
    throw std::runtime_error("STEP transfer failed");
  }

  const IFSelect_ReturnStatus write_status = writer.Write(file_path.c_str());
  if (write_status != IFSelect_RetDone) {
    throw std::runtime_error("STEP write failed for path: " + file_path);
  }

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
    builder.Add(compound, shape);
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
