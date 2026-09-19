#include "core/drawing/drawing_refresh.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "core/diagnostics/logger.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_runtime.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

namespace {

// Stores a stale last-known result for a broken view so the sheet can
// still draw it (visibly marked), and records the degradation on the
// view itself.
void store_broken_result(DocumentState& document, DrawingView& view,
                         const std::string& warning,
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

      // ── Frame resolution ────────────────────────────────────
      std::optional<DrawingViewFrame> frame;
      if (view.kind == "section") {
        // Section frames derive from the cutting plane (P4): the view
        // plane IS the cutting plane; view-X is the orthogonal
        // projection of world +X onto the plane (fallbacks +Y, +Z —
        // with a unit normal the last fallback always resolves).
        if (!view.section.has_value()) {
          store_broken_result(
              document, view,
              "The section view has no section definition — the view "
              "holds its last-known projection.",
              std::nullopt, target_revision);
          continue;
        }
        DrawingViewFrame section_frame;
        section_frame.origin = view.section->cutting_plane_point;
        section_frame.normal = view.section->cutting_plane_normal;
        const auto project_onto_plane = [&](const std::array<double, 3>& axis) {
          const double dot = axis[0] * section_frame.normal[0] +
                             axis[1] * section_frame.normal[1] +
                             axis[2] * section_frame.normal[2];
          return std::array<double, 3>{
              axis[0] - dot * section_frame.normal[0],
              axis[1] - dot * section_frame.normal[1],
              axis[2] - dot * section_frame.normal[2]};
        };
        std::array<double, 3> x_axis = project_onto_plane({1.0, 0.0, 0.0});
        double x_length = std::sqrt(x_axis[0] * x_axis[0] +
                                    x_axis[1] * x_axis[1] +
                                    x_axis[2] * x_axis[2]);
        if (x_length < 1e-9) {
          x_axis = project_onto_plane({0.0, 1.0, 0.0});
          x_length = std::sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1] +
                               x_axis[2] * x_axis[2]);
        }
        if (x_length < 1e-9) {
          x_axis = project_onto_plane({0.0, 0.0, 1.0});
          x_length = std::sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1] +
                               x_axis[2] * x_axis[2]);
        }
        if (x_length < 1e-9) {
          store_broken_result(
              document, view,
              "The cutting plane normal is degenerate — the view holds "
              "its last-known projection.",
              std::nullopt, target_revision);
          continue;
        }
        section_frame.x_direction = {x_axis[0] / x_length, x_axis[1] / x_length,
                                     x_axis[2] / x_length};
        frame = section_frame;
      } else if (!view.standard_view.empty()) {
        frame = standard_view_frame(view.standard_view);
        if (!frame.has_value()) {
          polysmith::core::log_warn(
              "drawing", "the view '" + view.view_id +
                             "' has an unknown standard view '" +
                             view.standard_view + "'");
        }
      } else if (view.custom_frame.has_value()) {
        frame = view.custom_frame;
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
        store_broken_result(document, view, warning, broken_ref,
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
    }
  }
}

}  // namespace polysmith::core
