#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

// Live link between a body face / edge / vertex and the sketch
// entities that the Project tool generated from it. Stored on the
// sketch so that `refresh_sketch_projections` (run as part of the
// dependency walker before the sketch's derived-state pass) can
// re-resolve the source on every recompute and patch the cached
// coords on the matching `lines` / `circles` / `arcs` /
// `projected_points` entries in place. End result: editing an
// upstream feature whose body the projection points at moves the
// projected geometry in lockstep, mirroring mainstream CAD's behaviour.
//
// `source_kind` mirrors the topology id's middle segment for body
// projections ("face", "edge", "vertex"); sketch profile projections
// use "profile" for UI identity. `generated_*` ids are the entity ids
// the project methods minted; the refresher walks them by id,
// finds the entity in the sketch, and rewrites its coords.
//
// `dependency_broken` is true when the most recent refresh failed
// to resolve the source (body deleted, edge curve type changed
// into something we can't project, etc.). The generated entities
// are left frozen at their last-known coords and the parent
// feature surfaces a warning via the existing `dependency_broken`
// machinery.
struct SketchProjection {
  std::string id;
  std::string source_id;
  std::string source_kind; // "face" | "edge" | "vertex" | "profile"
  std::vector<std::string> generated_line_ids;
  std::vector<std::string> generated_circle_ids;
  std::vector<std::string> generated_arc_ids;
  // For vertex projections only — the `SketchProjectedPoint::id`
  // that was minted. Empty for face / edge projections.
  std::string generated_vertex_id;
  bool dependency_broken = false;
  std::string dependency_warning;
};

}  // namespace polysmith::core
