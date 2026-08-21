#pragma once

#include <string>

#include <TopoDS_Shape.hxx>

#include "core/document/body_feature_types.h"

namespace polysmith::core {

// Result of reading a STEP file: the translated shape (a compound for
// multi-solid files), the original file units as reported by the file
// header (e.g. "MILLIMETRE", "INCH", or "unknown" when the header is
// empty), and topology counts for the feature summary.
struct StepImportResult {
  TopoDS_Shape shape;
  std::string source_units;
  int solid_count = 0;
  int face_count = 0;
};

// Reads a STEP file and returns the translated shape in millimeters.
// Throws std::runtime_error when the file is missing, unreadable, or
// contains no transferable geometry — callers treat a throw as "import
// failed, document untouched" (parse-before-mutate).
StepImportResult read_step_file(const std::string& file_path);

// Returns the live translated shape stored at import time. After a
// part-file load the handle is null — the body compiler falls back to
// deserializing the persisted snapshot (this helper cannot: the
// snapshot deserializer lives in the compiler's translation unit).
TopoDS_Shape build_step_import_shape(const StepImportFeatureParameters& params);

}  // namespace polysmith::core
