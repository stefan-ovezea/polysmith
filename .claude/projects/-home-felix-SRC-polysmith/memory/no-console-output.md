---
name: no-console-output
description: Tauri captures stderr/stdout — fprintf is invisible to the user
metadata:
  type: feedback
---

PolySmith is a Tauri desktop app. There is no terminal/console output visible
to the user. `fprintf(stderr, ...)` and `printf(...)` messages from the C++
core are captured and discarded by Tauri.

**How to report diagnostics to the user:**
- Use the structured logging system (`core/diagnostics/logger.h`) to emit
  log events that Tauri forwards as `cad-core-log` events to the UI.
- The UI renders these in the **Logs panel** (toolbar icon).
- Zero-length line deletions are already reported this way — grep for
  how those are logged and follow the same pattern.

**Why:** I added `fprintf(stderr, ...)` diagnostics for post-solve geometry
validation, which is useless because the user can't see them. Need to
use the structured logger instead.

**How to apply:** When adding diagnostics, use the structured logger (not
fprintf). Check how zero-length line cleanup reports errors and follow
that pattern.
