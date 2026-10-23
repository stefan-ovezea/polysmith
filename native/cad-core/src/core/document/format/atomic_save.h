#pragma once

#include <filesystem>
#include <string>

namespace polysmith::core {

// Writes `content` to `path` atomically: write a temp file, flush + fsync,
// back up the previous file as `path.bak`, then rename the temp into place.
// A crash at any instant leaves either the old file, the new file, or
// `path.bak` + `path.tmp`, never a truncated `path`.
void write_file_atomically(const std::filesystem::path& path,
                           const std::string& content);

}  // namespace polysmith::core
