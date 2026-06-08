#pragma once

#include <optional>

#include "core/document/document.h"
#include "core/viewport/viewport_state.h"

namespace polysmith::core {

ViewportState build_viewport_state(const std::optional<DocumentState>& document);

}  // namespace polysmith::core
