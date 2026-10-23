#pragma once

#include <string>

#include "protocol/ipc.h"

namespace polysmith {

class CadCoreApp {
 public:
  void run();

 private:
  void init_occt() const;
  // Returns false when no handler claimed the command. The caller answers
  // with an error event carrying the real id — a silent fall-through leaves
  // the UI's awaited promise unsettled (see the id-correlation comment in
  // run()).
  bool handle_command_line(const polysmith::protocol::CommandMessage& command);
};

}  // namespace polysmith
