// Minimal TextEngine shim for cad_core_sketch_profile_test.
//
// That executable is built without the protocol/OCCT dependency chain
// (see logger_stub.cpp), but sketch_feature.cpp now calls the text
// expansion on every refresh, which references TextEngine symbols.
// Sketch-profile tests never create text entities, so the stub's no-op
// layout is never actually reached — it only satisfies the linker.

#include <algorithm>

#include "core/text_engine.h"

namespace polysmith::core::text {

TextEngine& TextEngine::instance() {
  // TextEngine's constructor / destructor are defined only in the full
  // build (they own the OCCT-backed Impl), and constructing one here
  // would force MSVC to instantiate the member destructor against the
  // incomplete Impl type. This lightweight target never reaches the
  // engine — the text expansion returns before touching it whenever
  // the sketch has no texts — so aligned, never-constructed storage is
  // sufficient for the linker and the runtime.
  alignas(TextEngine) static char storage[sizeof(TextEngine)];
  return *reinterpret_cast<TextEngine*>(storage);
}

bool TextEngine::layout(const std::string& utf8_text,
                        double anchor_x,
                        double anchor_y,
                        const TextStyle& style,
                        TextLayout* out,
                        std::string* error) {
  if (error != nullptr) {
    *error = "text engine unavailable (stub build)";
  }
  return false;
}

double TextEngine::tessellation_tolerance(double height_mm) {
  return std::clamp(height_mm / 200.0, 0.01, 0.2);
}

std::string TextEngine::bundled_font_path() {
  return "";
}

}  // namespace polysmith::core::text
