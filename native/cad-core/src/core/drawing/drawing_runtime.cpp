#include "core/drawing/drawing_runtime.h"

#include <unordered_map>
#include <utility>

#include "core/document/document.h"

namespace polysmith::core::drawing_runtime {

namespace {

std::unordered_map<std::string, PerDocument>& registry() {
  static std::unordered_map<std::string, PerDocument> documents;
  return documents;
}

const ProjectionResult* cached(const PerDocument& per_doc,
                               const std::string& view_id, int revision) {
  const auto found = per_doc.projections.find(view_id);
  if (found == per_doc.projections.end()) {
    return nullptr;
  }
  if (found->second.revision != revision) {
    return nullptr;
  }
  return &found->second.projection;
}

}  // namespace

PerDocument& document_state(const std::string& document_id) {
  return registry()[document_id];
}

void clear(const std::string& document_id) {
  registry().erase(document_id);
}

const ProjectionResult* cached_projection_at(const DocumentState& document,
                                             const std::string& view_id,
                                             int revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  return cached(found->second, view_id, revision);
}

const ProjectionResult* cached_projection(const DocumentState& document,
                                          const std::string& view_id) {
  return cached_projection_at(document, view_id, document.revision);
}

void store_projection(const DocumentState& document,
                      const std::string& view_id, ProjectionResult result) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.last_revision = document.revision;
  per_doc.projections[view_id] =
      Entry{document.revision, std::move(result)};
}

void store_projection_at(const DocumentState& document,
                         const std::string& view_id, ProjectionResult result,
                         int target_revision) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.last_revision = target_revision;
  per_doc.projections[view_id] =
      Entry{target_revision, std::move(result)};
}

void drop_stale(const DocumentState& document, int target_revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return;
  }
  PerDocument& per_doc = found->second;
  for (auto it = per_doc.projections.begin(); it != per_doc.projections.end();) {
    if (it->second.revision != target_revision) {
      it = per_doc.projections.erase(it);
    } else {
      ++it;
    }
  }
  per_doc.last_revision = target_revision;
}

void invalidate(const std::string& document_id) {
  const auto found = registry().find(document_id);
  if (found == registry().end()) {
    return;
  }
  PerDocument& per_doc = found->second;
  per_doc.projections.clear();
  per_doc.last_revision = -1;
}

}  // namespace polysmith::core::drawing_runtime
