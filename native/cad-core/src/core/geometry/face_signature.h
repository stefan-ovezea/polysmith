#pragma once

#include <string>

class TopoDS_Face;  // OCCT, forward-declared — defined in the .cpp

namespace polysmith::core {

// Computes a geometric signature of a face for appearance matching: the
// surface type plus the rounded unit normal at the face's parameter-midpoint.
// Moved out of `core/document/appearance.h` so the document layer stays
// OCCT-free (the geometry kernel lives in core/geometry/).
std::string face_signature(const TopoDS_Face& face);

}  // namespace polysmith::core
