#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>

namespace polysmith::core {

// Serializes a B-rep shape into its self-contained snapshot string (used for
// persisted import / body-copy features). Lives in core/geometry/ so the
// document layer stays OCCT-free.
inline std::string serialize_shape_snapshot(const TopoDS_Shape& shape) {
  std::ostringstream stream;
  try {
    BRepTools::Write(shape, stream);
  } catch (const Standard_Failure&) {
    throw std::runtime_error("Could not serialize copied body shape");
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("OCCT serialization error: ") + e.what());
  }
  return stream.str();
}

}  // namespace polysmith::core
