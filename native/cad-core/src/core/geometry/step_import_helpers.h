#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>

#include "core/document/body_feature_types.h"

namespace polysmith::core {

// One solid of the imported file, with the identity the file gives it: the
// part name from the product structure and the display colour from its
// presentation style. Each becomes its own body, so a PCB assembly arrives
// as its parts instead of one undifferentiated lump.
struct StepImportedSolid {
  TopoDS_Shape shape;
  // Faces in this solid (the length `face_colors` takes when styled).
  int face_count = 0;
  // Part name from the file (e.g. "FPC0.5-30P"). Empty when the file names
  // nothing; callers fall back to a positional name.
  std::string name;
  // One entry per face of `shape` in TopExp::MapShapes(..., TopAbs_FACE)
  // order, packed 0xFFRRGGBB (0 = no style on that face). Empty when the
  // file styles nothing for this solid.
  std::vector<std::uint32_t> face_colors;
};

// One node of the file's product structure — a component OCCURRENCE, which is
// the file's name for one PLACEMENT of a part ("U1:1"), not the part's own name
// ("QFN-104"). The same part is placed many times in a board, so the occurrence
// is the only thing that distinguishes them; the part name is the fallback when
// the file names no occurrence.
struct StepAssemblyNode {
  std::string name;
  // Index of the parent node in the same vector, -1 for a top-level node.
  // Parents always precede their children (pre-order), so a consumer can build
  // the tree in one pass.
  int parent = -1;
  // Solids this occurrence is made of, as indices into `StepImportResult::solids`.
  // Only leaves carry solids: an assembly's own shape contains its children's
  // solids too, so collecting at every level would duplicate every body.
  std::vector<int> solid_indices;
};

// Result of reading a STEP file: the translated shape (a compound for
// multi-solid files), the original file units as reported by the file
// header (e.g. "MILLIMETRE", "INCH", or "unknown" when the header is
// empty), and topology counts for the feature summary.
struct StepImportResult {
  TopoDS_Shape shape;
  std::string source_units;
  int solid_count = 0;
  int face_count = 0;
  // One entry per solid in `shape`, in TopExp::MapShapes(..., TopAbs_SOLID)
  // order — the same solids, split apart, carrying their names and colours.
  // Empty only when the file has no solids at all (surface models).
  std::vector<StepImportedSolid> solids;
  // The file's component occurrences, pre-order, each pointing at the solids it
  // is made of. Empty when the file has no assembly structure (a single part),
  // or when the structure could not be matched to the solids.
  std::vector<StepAssemblyNode> assembly;
};

// Reads a STEP file and returns the translated shape in millimeters.
// Throws std::runtime_error when the file is missing, unreadable, or
// contains no transferable geometry — callers treat a throw as "import
// failed, document untouched" (parse-before-mutate).
StepImportResult read_step_file(const std::string& file_path);

// Returns the live translated shape stored in the import runtime cache.
// After a part-file load the cache is empty — the body compiler falls back
// to deserializing the persisted snapshot (this helper cannot: the
// snapshot deserializer lives in the compiler's translation unit).
TopoDS_Shape build_step_import_shape(const std::string& feature_id);

}  // namespace polysmith::core
