#pragma once

#include <string>

namespace polysmith {

class CadCoreApp {
 public:
  void run();

 private:
  void init_occt() const;
  void handle_command_line(const std::string& line);
};

}  // namespace polysmith
