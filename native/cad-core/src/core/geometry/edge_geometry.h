#pragma once

#include <optional>
#include <string>

namespace polysmith::core {

struct DocumentState;

// World-space description of a body edge or vertex, produced by
// resolving a body-derived id (`<body>:edge:<index>` or
// `<body>:vertex:<index>`) against the current OCCT compilation. This
// is the input to the Project tool — the projector takes the world
// geometry and flattens it onto the active sketch's plane.
struct EdgePoint {
  double x;
  double y;
  double z;
};

struct EdgeGeometry {
  // "line" — straight body edge from `start` to `end`.
  // "circle" — full circular edge (closed); `center`, `axis`, and
  //   `radius` define the circle.
  // "arc" — partial circular edge; `center`, `axis`, `radius`, plus
  //   `start` (the arc's first endpoint) and `end` (the second
  //   endpoint) — both lie on the circle.
  // "unsupported" — anything else (B-splines, ellipses, surface
  //   curves...). The Project tool surfaces a "not yet supported"
  //   message rather than guessing.
  std::string kind;

  EdgePoint start{};
  EdgePoint end{};

  // Populated for "circle" and "arc".
  EdgePoint center{};
  EdgePoint axis{};  // unit normal of the circle's plane
  double radius = 0.0;
};

// Resolve `edge_id` (format: "<body_id>:edge:<index>") against the
// current document state and return its world-space geometry. The
// caller is responsible for flattening it onto the active sketch
// plane. Returns nullopt when the id can't be parsed or the body /
// edge index doesn't exist; returns an "unsupported" EdgeGeometry
// when the edge resolves but its curve type isn't one we project.
std::optional<EdgeGeometry> compute_edge_geometry(
    const DocumentState& document, const std::string& edge_id);

// Same shape as `compute_edge_geometry` but for body vertices.
// Vertex ids follow "<body_id>:vertex:<index>" — see
// `enumerate_body_vertices` in viewport.cpp for the producer.
std::optional<EdgePoint> compute_vertex_position(
    const DocumentState& document, const std::string& vertex_id);

}  // namespace polysmith::core
