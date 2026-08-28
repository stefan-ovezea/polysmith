#include "core/cam/cam_runtime.h"

#include <unordered_map>

#include "core/document/document.h"

namespace polysmith::core::cam_runtime {

namespace {

std::unordered_map<std::string, PerDocument>& registry() {
  static std::unordered_map<std::string, PerDocument> documents;
  return documents;
}

}  // namespace

PerDocument& document_state(const std::string& document_id) {
  return registry()[document_id];
}

void clear(const std::string& document_id) {
  registry().erase(document_id);
}

namespace {

const Toolpath* cached(const PerDocument& per_doc,
                       const std::unordered_map<std::string, Entry>& entries,
                       const std::string& op_id, int revision) {
  const auto found = entries.find(op_id);
  if (found == entries.end()) {
    return nullptr;
  }
  if (found->second.revision != revision) {
    return nullptr;
  }
  return &found->second.toolpath;
}

}  // namespace

const Toolpath* cached_toolpath_at(const DocumentState& document,
                                   const std::string& op_id, int revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  return cached(found->second, found->second.generated, op_id, revision);
}

const Toolpath* cached_preview_at(const DocumentState& document,
                                  const std::string& op_id, int revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  return cached(found->second, found->second.previews, op_id, revision);
}

const Toolpath* cached_toolpath(const DocumentState& document,
                                const std::string& op_id) {
  return cached_toolpath_at(document, op_id, document.revision);
}

const Toolpath* cached_preview(const DocumentState& document,
                               const std::string& op_id) {
  return cached_preview_at(document, op_id, document.revision);
}

void store_generated(const DocumentState& document, const std::string& op_id,
                     Toolpath toolpath) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.last_revision = document.revision;
  per_doc.generated[op_id] = Entry{document.revision, std::move(toolpath)};
}

void store_preview(const DocumentState& document, const std::string& op_id,
                   Toolpath toolpath) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.last_revision = document.revision;
  per_doc.previews[op_id] = Entry{document.revision, std::move(toolpath)};
}

void drop_stale(const DocumentState& document, int target_revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return;
  }
  PerDocument& per_doc = found->second;
  auto prune = [&](std::unordered_map<std::string, Entry>& entries) {
    for (auto it = entries.begin(); it != entries.end();) {
      if (it->second.revision != target_revision) {
        it = entries.erase(it);
      } else {
        ++it;
      }
    }
  };
  prune(per_doc.generated);
  prune(per_doc.previews);
  per_doc.last_revision = target_revision;
}

}  // namespace polysmith::core::cam_runtime
