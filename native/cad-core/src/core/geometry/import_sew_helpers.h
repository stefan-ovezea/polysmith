#pragma once

#include <TopoDS_Shape.hxx>

namespace polysmith::core {

// Sews a faces-only imported shape into solids. Surface-model imports
// (faces-only IGES/STEP files) arrive without solid entities, and the
// solid-only modifiers (booleans, fillets, chamfers, shell, hole) are
// gated off shapes without solids — so a watertight faces-only file
// is sewn on import. Orientation is normalized per solid (imported
// face orientations are frequently inconsistent — a sewn solid can
// come out inverted with negative volume, which breaks booleans).
// Returns a compound of solids, or a null shape when the faces are
// not watertight at the given tolerance.
TopoDS_Shape sew_faces_to_solids(const TopoDS_Shape& shape, double tolerance);

}  // namespace polysmith::core
