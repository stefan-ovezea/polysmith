#pragma once

#include <string>

namespace polysmith::core {

struct ExportResult;
struct SketchFeatureParameters;

// Writes the sketch's geometry to an ASCII R2013 (AC1027) DXF file:
// lines → LINE, circles → CIRCLE, arcs → ARC, projected points →
// POINT. Polygon records are not re-emitted (add_sketch_polygon
// already decomposes them into constituent lines). Construction lines
// are exported as regular lines (DXF has no construction concept).
// The header echoes the document units via $INSUNITS (mm → 4, in → 1,
// anything else → 0). Throws std::runtime_error on I/O failure.
ExportResult export_sketch_as_dxf(const SketchFeatureParameters& sketch,
                                  const std::string& units,
                                  const std::string& file_path);

}  // namespace polysmith::core
