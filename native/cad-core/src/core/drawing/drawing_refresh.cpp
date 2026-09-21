#include "core/drawing/drawing_refresh.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "core/diagnostics/logger.h"
#include "core/drawing/drawing_detail_clip.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

namespace {

// Stores a stale last-known result for a broken view so the sheet can
// still draw it (visibly marked), and records the degradation on the
// view itself.
void store_broken_result(DocumentState& document, Drawing& drawing,
                         DrawingView& view, const std::string& warning,
                         std::optional<std::string> broken_ref,
                         int target_revision) {
  view.broken_ref = std::move(broken_ref);
  view.warning = warning;
  ProjectionResult stale_result;
  if (const ProjectionResult* last =
          drawing_runtime::last_known_projection(document, view.view_id)) {
    stale_result = *last;
  }
  stale_result.stale = true;
  stale_result.source_revision = target_revision;
  drawing_runtime::store_projection_at(document, view.view_id,
                                       std::move(stale_result),
                                       target_revision);
  // A broken view degrades its annotations with it — they keep their
  // last-known values marked stale (never blank, never substituted).
  for (auto& annotation : drawing.annotations) {
    if (annotation.view_id != view.view_id) {
      continue;
    }
    annotation.dependency_broken = true;
    annotation.warning = warning;
    if (is_annotation_kind(annotation.kind)) {
      if (const ResolvedAttachment* last = drawing_runtime::last_known_attachment(
              document, annotation.annotation_id)) {
        ResolvedAttachment stale_copy = *last;
        stale_copy.stale = true;
        stale_copy.warning = warning;
        drawing_runtime::store_attachment_at(document, annotation.annotation_id,
                                             std::move(stale_copy),
                                             target_revision);
      }
      continue;
    }
    if (const ResolvedDimension* last = drawing_runtime::last_known_dimension(
            document, annotation.annotation_id)) {
      ResolvedDimension stale_copy = *last;
      stale_copy.stale = true;
      stale_copy.warning = warning;
      drawing_runtime::store_dimension_at(document, annotation.annotation_id,
                                          std::move(stale_copy),
                                          target_revision);
    }
  }
}

// Re-resolves one view's annotations against its fresh projection —
// the TNP ladder (exact witness → relaxed geometry → ambiguous →
// not found).  Values and attachment geometry are memory-only; the
// persisted dependency_broken/warning mirror the degradation.
void refresh_view_annotations(DocumentState& document, Drawing& drawing,
                              DrawingView& view,
                              const ProjectionResult& fresh,
                              int target_revision) {
  for (auto& annotation : drawing.annotations) {
    if (annotation.view_id != view.view_id) {
      continue;
    }
    // Annotation kinds (GEOMETRY/SYMBOLS/ANNOTATE) resolve through
    // the same ladder into their own cache — the ladder itself is
    // shared (resolve_one), only the outcome struct differs.
    if (is_annotation_kind(annotation.kind)) {
      ResolvedAttachment resolved = resolve_annotation_attachment(
          fresh, annotation, document.drawing.decimal_separator);
      const std::string warning = resolved.warning;
      if (resolved.broken) {
        if (const ResolvedAttachment* last =
                drawing_runtime::last_known_attachment(
                    document, annotation.annotation_id)) {
          ResolvedAttachment stale_copy = *last;
          stale_copy.stale = true;
          stale_copy.warning = warning;
          drawing_runtime::store_attachment_at(
              document, annotation.annotation_id, std::move(stale_copy),
              target_revision);
        } else {
          // No last-known attachment: cache the broken marker (no
          // geometry — the flatten skips it; the panel shows the
          // warning).
          resolved.stale = true;
          drawing_runtime::store_attachment_at(
              document, annotation.annotation_id, std::move(resolved),
              target_revision);
        }
        annotation.dependency_broken = true;
        annotation.warning = warning;
      } else {
        drawing_runtime::store_attachment_at(
            document, annotation.annotation_id, std::move(resolved),
            target_revision);
        annotation.dependency_broken = false;
        annotation.warning.clear();
      }
      continue;
    }
    ResolvedDimension resolved = resolve_annotation(
        fresh, annotation, document.drawing.decimal_separator);
    const std::string warning = resolved.warning;
    if (resolved.broken) {
      if (const ResolvedDimension* last = drawing_runtime::last_known_dimension(
              document, annotation.annotation_id)) {
        ResolvedDimension stale_copy = *last;
        stale_copy.stale = true;
        stale_copy.warning = warning;
        drawing_runtime::store_dimension_at(document, annotation.annotation_id,
                                            std::move(stale_copy),
                                            target_revision);
      } else {
        // No last-known value: cache the broken marker (no geometry —
        // the flatten skips it; the panel shows the warning).
        resolved.stale = true;
        drawing_runtime::store_dimension_at(document, annotation.annotation_id,
                                            std::move(resolved),
                                            target_revision);
      }
      annotation.dependency_broken = true;
      annotation.warning = warning;
    } else {
      drawing_runtime::store_dimension_at(document, annotation.annotation_id,
                                          std::move(resolved),
                                          target_revision);
      annotation.dependency_broken = false;
      annotation.warning.clear();
    }
  }
}

}  // namespace

void refresh_drawing_dependencies(DocumentState& document,
                                  int target_revision) {
  if (document.drawing.drawings.empty()) {
    return;
  }

  // A revision bump invalidates every cached projection for this
  // document — prune them (into last_known) so the viewport never
  // draws stale geometry.
  drawing_runtime::drop_stale(document, target_revision);

  // Compile bodies once for all view-source lookups (shape-only, no
  // meshes) and only when at least one view actually needs it.
  CompiledBodies bodies;
  bool compiled = false;
  auto ensure_bodies = [&]() {
    if (!compiled) {
      bodies = compile_bodies(document, /*include_meshes=*/false);
      compiled = true;
    }
  };

  for (auto& drawing : document.drawing.drawings) {
    for (auto& view : drawing.views) {
      if (drawing_runtime::cached_projection_at(
              document, view.view_id, target_revision) != nullptr) {
        continue;  // already computed for this revision
      }
      // Detail views derive from their PARENT's projection — the
      // second pass below handles them after every parent is done.
      if (view.kind == "detail") {
        continue;
      }

      // ── Frame resolution (shared with the live view preview) ──
      std::optional<DrawingViewFrame> frame;
      if (view.kind == "section") {
        // Section frames derive from the cutting plane (P4): the view
        // plane IS the cutting plane.  Two distinct degradation
        // messages distinguish a missing definition from a degenerate
        // normal.
        if (!view.section.has_value()) {
          store_broken_result(
              document, drawing, view,
              "The section view has no section definition — the view "
              "holds its last-known projection.",
              std::nullopt, target_revision);
          continue;
        }
        frame = resolve_view_frame(view);
        if (!frame.has_value()) {
          store_broken_result(
              document, drawing, view,
              "The cutting plane normal is degenerate — the view holds "
              "its last-known projection.",
              std::nullopt, target_revision);
          continue;
        }
      } else {
        frame = resolve_view_frame(view);
        if (!frame.has_value() && !view.standard_view.empty()) {
          polysmith::core::log_warn(
              "drawing", "the view '" + view.view_id +
                             "' has an unknown standard view '" +
                             view.standard_view + "'");
        }
      }

      // ── Source body resolution ─────────────────────────────
      std::vector<SourceBody> sources;
      std::optional<std::string> broken_ref;
      if (frame.has_value()) {
        ensure_bodies();
        for (const auto& body_id : view.source_body_ids) {
          const auto found =
              std::find_if(bodies.bodies.begin(), bodies.bodies.end(),
                           [&](const CompiledBody& b) {
                             return b.id == body_id;
                           });
          if (found == bodies.bodies.end()) {
            broken_ref = body_id;
            break;
          }
          sources.push_back({body_id, found->shape});
        }
      }

      if (!frame.has_value() || broken_ref.has_value()) {
        const std::string warning =
            broken_ref.has_value()
                ? "The source body '" + broken_ref.value() +
                      "' no longer exists — the view holds its "
                      "last-known projection."
                : "The view frame could not be resolved — the view "
                  "holds its last-known projection.";
        polysmith::core::log_warn("drawing", "view '" + view.view_id +
                                                 "': " + warning);
        store_broken_result(document, drawing, view, warning, broken_ref,
                            target_revision);
        continue;
      }

      // Everything resolved — clear any previous degradation and
      // re-project.
      view.broken_ref.reset();
      view.warning.clear();

      ProjectionInput input;
      input.sources = std::move(sources);
      input.frame = frame.value();
      input.show_hidden = view.show_hidden;
      input.section = view.kind == "section" ? view.section : std::nullopt;
      // Sibling sections trace their cutting planes onto this view
      // (every edge-on view of the drawing shows the chain line).
      for (const auto& other : drawing.views) {
        if (other.view_id != view.view_id && other.section.has_value()) {
          input.section_traces.push_back({other.section.value()});
        }
      }
      input.source_revision = target_revision;
      drawing_runtime::store_projection_at(
          document, view.view_id, project(input), target_revision);

      // ── Annotation re-resolution (P6) ─────────────────────────
      if (const ProjectionResult* fresh = drawing_runtime::cached_projection_at(
              document, view.view_id, target_revision)) {
        refresh_view_annotations(document, drawing, view, *fresh,
                                 target_revision);
      }
    }

    // ── Detail views (ISO 128-3): second pass — every parent is
    //    cached at this revision by now.  The detail's projection is
    //    the parent's CLIPPED to the detail circle (same coordinate
    //    frame — the detail is a magnified window, not a re-projection).
    for (auto& view : drawing.views) {
      if (view.kind != "detail") {
        continue;
      }
      if (drawing_runtime::cached_projection_at(
              document, view.view_id, target_revision) != nullptr) {
        continue;  // already computed for this revision
      }
      if (!view.detail.has_value()) {
        store_broken_result(
            document, drawing, view,
            "The detail view has no detail definition — the view holds "
            "its last-known state.",
            std::nullopt, target_revision);
        continue;
      }
      const DrawingView* parent = nullptr;
      for (const auto& candidate : drawing.views) {
        if (candidate.view_id == view.detail.value().parent_view_id) {
          parent = &candidate;
          break;
        }
      }
      if (parent == nullptr) {
        store_broken_result(
            document, drawing, view,
            "The detail view's parent view no longer exists — the view "
            "holds its last-known state.",
            view.detail.value().parent_view_id, target_revision);
        continue;
      }
      if (parent->broken_ref.has_value()) {
        // The parent is degraded — the detail degrades WITH it (the
        // parent's stale result is cached at this revision, so the
        // clip still yields last-known content, marked stale).
        store_broken_result(document, drawing, view, parent->warning,
                            parent->broken_ref, target_revision);
        continue;
      }
      const ProjectionResult* parent_projection =
          drawing_runtime::cached_projection_at(document, parent->view_id,
                                                target_revision);
      if (parent_projection == nullptr) {
        store_broken_result(
            document, drawing, view,
            "The detail view's parent projection is unavailable — the "
            "view holds its last-known state.",
            view.detail.value().parent_view_id, target_revision);
        continue;
      }
      ProjectionResult clipped = clip_projection_to_circle(
          *parent_projection, view.detail.value().center,
          view.detail.value().radius);
      clipped.source_revision = target_revision;
      view.broken_ref.reset();
      view.warning.clear();
      drawing_runtime::store_projection_at(
          document, view.view_id, std::move(clipped), target_revision);
    }
  }
}

}  // namespace polysmith::core
