#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_resolution.h"
#include "core/cam/toolpath.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core {

struct DocumentState;
struct CamSetup;
struct CamOperation;
struct ToolEntry;
struct SketchProfileRegion;

// ── Generator registry ────────────────────────────────────────────
//
// CAM operations map to generators purely by CamOperationType string.
// Adding a new operation kind (pocket_2d, contour_2d, drilling,
// turning, ...) means writing one generator and registering it — no
// changes to the document, IPC, or refresh machinery.
//
// Generators receive pre-resolved geometry: the generate driver
// resolves geometry references (face witnesses, sketch profile
// witnesses) before calling in, so a generator never touches OCCT
// reference resolution itself.  Both v1 generators complete in well
// under 100 ms, so generation is synchronous; the context carries a
// `preview` flag for fast wireframe passes.

struct CamGenerateContext {
  const DocumentState& document;
  const CamSetup& setup;
  const CamOperation& operation;
  const ToolEntry& tool;
  bool preview = false;

  struct Geometry {
    std::vector<ResolvedFaceRef> faces;                 // machining regions
    std::vector<ResolvedProfileRef> profiles;           // profile regions
    std::vector<const SketchFeatureParameters*> sketches;  // owning sketches
  } geometry;
};

struct CamGenerateResult {
  bool ok = true;
  std::string error_message;  // human-readable; becomes status_message
  std::vector<std::string> warnings;
  Toolpath toolpath;  // valid when ok
};

using CamGenerateFn = std::function<CamGenerateResult(const CamGenerateContext&)>;

struct CamGenerator {
  std::string type;  // must match a CamOperationType value
  CamGenerateFn generate;  // full toolpath
  CamGenerateFn preview;   // fast wireframe pass; defaults to generate
};

void register_cam_generator(CamGenerator generator);
const CamGenerator* find_cam_generator(const std::string& type);

// Registers all built-in generators.  Called once from CadCoreApp::run()
// after OCCT initialization — never from a static initializer, which
// would couple the registry's lifetime to OCCT's.
void register_builtin_cam_generators();

}  // namespace polysmith::core
