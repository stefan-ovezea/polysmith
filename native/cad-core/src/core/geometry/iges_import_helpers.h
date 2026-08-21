#pragma once

#include <string>

#include <TopoDS_Shape.hxx>

#include "core/document/body_feature_types.h"

namespace polysmith::core {

// Result of reading an IGES file: the translated shape (a compound for
// multi-solid files), the original file units as declared in the
// global section (e.g. "INCH", "MM"), and topology counts for the
// feature summary.
struct IgesImportResult {
  TopoDS_Shape shape;
  std::string source_units;
  int solid_count = 0;
  int face_count = 0;
};

// Reads an IGES file and returns the translated shape in millimeters.
// Throws std::runtime_error when the file is missing, unreadable, or
// contains no transferable geometry — callers treat a throw as "import
// failed, document untouched" (parse-before-mutate).
IgesImportResult read_iges_file(const std::string& file_path);

// Returns the live translated shape stored at import time. After a
// part-file load the handle is null — the body compiler falls back to
// deserializing the persisted snapshot (this helper cannot: the
// snapshot deserializer lives in the compiler's translation unit).
TopoDS_Shape build_iges_import_shape(const IgesImportFeatureParameters& params);

}  // namespace polysmith::core
