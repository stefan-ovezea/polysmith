#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

// Directory for recovery journals: POLYSMITH_JOURNAL_DIR, defaulting to a
// temp location when unset.
std::string journal_directory();

// Appends one raw command JSON line to <journal-dir>/<doc-id>.jsonl.
void journal_append(const std::string& doc_id, const std::string& command_json);

// Removes the journal for `doc_id` (called after a successful save — the
// saved file becomes the checkpoint and the journal is only unsaved changes).
void journal_truncate(const std::string& doc_id);

// Reads the journal lines for `doc_id` (for recovery replay).
std::vector<std::string> journal_lines(const std::string& doc_id);

}  // namespace polysmith::core
