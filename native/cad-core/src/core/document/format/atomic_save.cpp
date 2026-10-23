#include "core/document/format/atomic_save.h"

#include <cstdio>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace polysmith::core {

void write_file_atomically(const std::filesystem::path& path,
                           const std::string& content) {
  const std::string tmp = path.string() + ".tmp";
  const std::string bak = path.string() + ".bak";

  // Write + fsync the temp file so the bytes are on disk before the rename.
  FILE* file = std::fopen(tmp.c_str(), "wb");
  if (file == nullptr) {
    throw std::runtime_error("Failed to open temporary file for writing: " + tmp);
  }
  const std::size_t written = std::fwrite(content.data(), 1, content.size(), file);
  const bool write_ok = (written == content.size());
  std::fflush(file);
  if (write_ok) {
#ifdef _WIN32
    _commit(_fileno(file));
#else
    fsync(fileno(file));
#endif
  }
  std::fclose(file);
  if (!write_ok) {
    std::remove(tmp.c_str());
    throw std::runtime_error("Failed to write document to: " + path.string());
  }

  // Back up the previous file, then atomically move the temp into place.
  std::error_code ec;
  if (std::filesystem::exists(path)) {
    std::filesystem::remove(bak, ec);
    ec.clear();
    std::filesystem::rename(path, bak, ec);
    if (ec) {
      std::remove(tmp.c_str());
      throw std::runtime_error("Failed to back up document: " + ec.message());
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    throw std::runtime_error("Failed to move temporary file into place: " +
                             ec.message());
  }
}

}  // namespace polysmith::core
