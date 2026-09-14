#pragma once

#include <string>

#include "protocol/ipc.h"

namespace polysmith {

class CadCoreApp {
 public:
  void run();

 private:
  void init_occt() const;
  void handle_command_line(const polysmith::protocol::CommandMessage& command);
};

}  // namespace polysmith
