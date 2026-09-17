#pragma once

#include <string>
#include <vector>

#include "core/cam/cam_types.h"

namespace polysmith::core {

// ── Tool table import/export ───────────────────────────────────────
//
// Two interchange formats, both text-based so the file I/O stays in
// the shell (Tauri dialogs) and only the text crosses the IPC:
//
// 1. LinuxCNC tool.tbl — the de-facto lingua franca (LinuxCNC,
//    FreeCAD export, Tormach PathPilot).  Lossy: the table stores
//    T/P, axis offsets (X/Z kept), D, lathe I/J/Q and a comment —
//    every other PolySmith field is filled with defaults on import
//    and dropped on export.  The parser is deliberately lenient:
//    bad lines are skipped with warnings, never fatal.
//
// 2. PolySmith tools JSON — lossless, versioned:
//    { "version": 1, "tools": [ <payload-shaped tool objects> ] }.
//    Guid-preserving, so re-imports reconcile instead of duplicating.

// Parses a LinuxCNC tool table.  On success fills `tools` (each
// parsed line becomes one ToolEntry) and `warnings` (per-line notes:
// skipped T0, dropped offsets, malformed lines).  `error` is set only
// for a hard failure (the whole text is unusable).
void parse_linuxcnc_tool_table(const std::string& text,
                               std::vector<ToolEntry>& tools,
                               std::vector<std::string>& warnings,
                               std::string& error);

// Canonical table text: a ";" header line, then one tool per line in
// ascending tool-number order.
std::string serialize_linuxcnc_tool_table(
    const std::vector<ToolEntry>& tools);

// Lossless PolySmith tools JSON round-trip (version 1).
std::vector<ToolEntry> parse_polysmith_tools_json(
    const std::string& text, std::vector<std::string>& warnings,
    std::string& error);
std::string serialize_polysmith_tools_json(
    const std::vector<ToolEntry>& tools);

}  // namespace polysmith::core
