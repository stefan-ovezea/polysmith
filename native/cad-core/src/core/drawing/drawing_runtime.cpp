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
      per_doc.last_known[it->first] = std::move(it->second);
      it = per_doc.projections.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = per_doc.dimensions.begin(); it != per_doc.dimensions.end();) {
    if (it->second.revision != target_revision) {
      per_doc.last_known_dimensions[it->first] = std::move(it->second);
      it = per_doc.dimensions.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = per_doc.attachments.begin(); it != per_doc.attachments.end();) {
    if (it->second.revision != target_revision) {
      per_doc.last_known_attachments[it->first] = std::move(it->second);
      it = per_doc.attachments.erase(it);
    } else {
      ++it;
    }
  }
  per_doc.last_revision = target_revision;
}

const ProjectionResult* cached_preview_projection(const DocumentState& document,
                                                  const std::string& key) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto& entry = found->second.preview_projection;
  if (entry.revision != document.revision || entry.key != key) {
    return nullptr;
  }
  return &entry.projection;
}

void store_preview_projection(const DocumentState& document,
                              const std::string& key, ProjectionResult result) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.preview_projection = {document.revision, key, std::move(result)};
}

const ProjectionResult* last_known_projection(const DocumentState& document,
                                              const std::string& view_id) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto entry = found->second.last_known.find(view_id);
  if (entry == found->second.last_known.end()) {
    return nullptr;
  }
  return &entry->second.projection;
}

void invalidate(const std::string& document_id) {
  const auto found = registry().find(document_id);
  if (found == registry().end()) {
    return;
  }
  PerDocument& per_doc = found->second;
  per_doc.projections.clear();
  per_doc.dimensions.clear();
  per_doc.attachments.clear();
  // A branch switch (undo/redo) invalidates last-known state too —
  // the abandoned branch's results are not the restored branch's
  // truth.
  per_doc.last_known.clear();
  per_doc.last_known_dimensions.clear();
  per_doc.last_known_attachments.clear();
  // Same for the uncommitted preview cache — a restored branch's
  // geometry is not the abandoned branch's projection.
  per_doc.preview_projection = {};
  per_doc.last_revision = -1;
}

const ResolvedDimension* cached_dimension_at(const DocumentState& document,
                                             const std::string& annotation_id,
                                             int revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto entry = found->second.dimensions.find(annotation_id);
  if (entry == found->second.dimensions.end()) {
    return nullptr;
  }
  if (entry->second.revision != revision) {
    return nullptr;
  }
  return &entry->second.dimension;
}

const ResolvedDimension* cached_dimension(const DocumentState& document,
                                          const std::string& annotation_id) {
  return cached_dimension_at(document, annotation_id, document.revision);
}

void store_dimension_at(const DocumentState& document,
                        const std::string& annotation_id,
                        ResolvedDimension dimension, int target_revision) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.dimensions[annotation_id] =
      DimensionEntry{target_revision, std::move(dimension)};
}

const ResolvedDimension* last_known_dimension(const DocumentState& document,
                                              const std::string& annotation_id) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto entry = found->second.last_known_dimensions.find(annotation_id);
  if (entry == found->second.last_known_dimensions.end()) {
    return nullptr;
  }
  return &entry->second.dimension;
}

void erase_dimension(const std::string& document_id,
                     const std::string& annotation_id) {
  const auto found = registry().find(document_id);
  if (found == registry().end()) {
    return;
  }
  found->second.dimensions.erase(annotation_id);
  found->second.last_known_dimensions.erase(annotation_id);
}

const ResolvedAttachment* cached_attachment_at(const DocumentState& document,
                                               const std::string& annotation_id,
                                               int revision) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto entry = found->second.attachments.find(annotation_id);
  if (entry == found->second.attachments.end()) {
    return nullptr;
  }
  if (entry->second.revision != revision) {
    return nullptr;
  }
  return &entry->second.attachment;
}

const ResolvedAttachment* cached_attachment(const DocumentState& document,
                                            const std::string& annotation_id) {
  return cached_attachment_at(document, annotation_id, document.revision);
}

void store_attachment_at(const DocumentState& document,
                         const std::string& annotation_id,
                         ResolvedAttachment attachment, int target_revision) {
  PerDocument& per_doc = registry()[document.id];
  per_doc.attachments[annotation_id] =
      AttachmentEntry{target_revision, std::move(attachment)};
}

const ResolvedAttachment* last_known_attachment(
    const DocumentState& document, const std::string& annotation_id) {
  const auto found = registry().find(document.id);
  if (found == registry().end()) {
    return nullptr;
  }
  const auto entry = found->second.last_known_attachments.find(annotation_id);
  if (entry == found->second.last_known_attachments.end()) {
    return nullptr;
  }
  return &entry->second.attachment;
}

void erase_attachment(const std::string& document_id,
                      const std::string& annotation_id) {
  const auto found = registry().find(document_id);
  if (found == registry().end()) {
    return;
  }
  found->second.attachments.erase(annotation_id);
  found->second.last_known_attachments.erase(annotation_id);
}

}  // namespace polysmith::core::drawing_runtime
