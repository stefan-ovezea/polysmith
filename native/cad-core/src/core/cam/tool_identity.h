#pragma once

#include <cstdint>
#include <random>
#include <sstream>

namespace polysmith::core {

// Random 128-bit identifier minted when a tool enters the library.
// Stable across save/load AND import/export: the LinuxCNC .tbl and
// PolySmith JSON formats both carry it so re-importing a library
// reconciles instead of duplicating (Fusion/FreeCAD GUID precedent).
inline std::string mint_tool_guid() {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());
  static std::uniform_int_distribution<std::uint64_t> dist;
  std::ostringstream out;
  out << std::hex << dist(gen) << dist(gen);
  return out.str();
}

}  // namespace polysmith::core
