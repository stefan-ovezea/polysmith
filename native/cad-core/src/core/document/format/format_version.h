#pragma once

namespace polysmith::core {

// The current on-disk document format version. Bump this and add one migration
// step (see migrations.cpp) for every schema change — never edit an
// already-shipped step.
inline constexpr int kCurrentFormatVersion = 1;

}  // namespace polysmith::core
