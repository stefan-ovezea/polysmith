# Project Summary

**What it is:** A local-first desktop CAD application for hobbyist 3D-printing workflows. Single-part parametric modeling, built with a strict UI/CAD boundary.

## Architecture

Three layers with clear ownership:

| Layer | Tech | Owns |
|---|---|---|
| **UI** | React + TypeScript | Presentation, user input, command dispatch |
| **Shell** | Tauri (Rust) | Window management, file dialogs, spawning the C++ core |
| **CAD Core** | C++ + OpenCascade | All geometry, feature history, document state, modeling operations |

Communication is via a JSON IPC protocol over `stdin`/`stdout`. The CAD core is a **separate process** — crash isolation, clean boundaries. The core is the single source of truth; React never owns CAD state.

## What's Built (shipped)

- JSON IPC bridge with versioned schema
- Document model: feature history tree, undo/redo, core-owned selection
- 2D sketch system: lines, rectangles, circles, arcs, polygons, points,
  dimensions, geometric + dimensional constraints, closed-profile detection
  with inner-loop/hole support
- Contextual modeling workflow: select → invoke → floating panel → real
  geometry preview → confirm/cancel
- Extrude (New Body, Join, Cut) with one-side/symmetric/two-side extent,
  taper, thin, and explicit target-body selection
- Revolve, Sweep, and Loft features
- Box and Cylinder primitives
- Shell body-modifying feature
- Edge/vertex selection, **Fillet & Chamfer** on edges with live preview panels
- **Hole feature:** simple, counterbore, countersink, spotface; metric/imperial
  standard presets
- Sketches on origin planes, solid faces, or construction planes; sketch re-entry
- 2D sketch fillets (line-line corners)
- Project tool (face/edge/vertex) with live parametric re-projection links
- Save/load `.polysmith` documents (core-owned JSON format)
- STEP + STL export (honours cut/join booleans)
- OrcaSlicer integration (native window embedding via X11/XWayland)
- Offset construction planes (offset, midplane, tangent, angle)
- Construction axes and points
- Unified selection filter panel: selection, snapping, and constraints
- Comprehensive snap system (endpoint, midpoint, center, quadrant, intersection,
  nearest, tangent, perpendicular, parallel, polar, grid, grid-line)
- DOF counting and entity colouring
- Dimension tool (single-entity, two-entity, placement drag, regroup-aware)
- Draft dimension visualisation (Three.js-rendered preview dimensions)
- On-demand sketch dimensions
- Per-dimension radius/diameter toggle and driven dimensions
- Trim tool (line, circle→arc, arc; core-driven hover preview)
- Parametric parameters & dimension formulas
- Endpoint drag with planegcs WASM solver (60 fps local, core commit on mouse-up)
- 3D Move tool with local-axis manipulator
- Body copy (linked/independent)
- Materials (body/face colour overrides)
- Helix, Thread, and Fastener features
- CAM workspace scaffolding (UI skeleton, TNP face witness resolution, face milling)
- Natural-language AI command bar
- View cube with cardinal face snaps, dynamic zoom-aware grids

## The Project's Mantra: Topological Naming Problem (TNP)

"Never store a naked OCCT topology index and trust it across recomputes." Every feature that references 3D geometry must **re-resolve** its references against live body shapes on every recompute. When resolution fails, degrade gracefully (`dependency_broken` + warning), never crash. This is handled for edges (fillet/chamfer), faces (construction planes, face-based sketches), and sketch projections.

## Binding UX Pattern: Contextual Modeling Workflow

Every modeling feature follows: **select inputs → invoke action → floating context panel → real geometry preview (core-recomputed) → confirm or cancel.** No fake previews, no modal dialogs. Enter confirms, Escape cancels with `undo`.

## What's Next (V1 Remaining)

The bulk of the original v1 roadmap is shipped. Remaining:

1. **Pattern features** — linear and circular
2. **Measure tool** — point-to-point, edge length, face area
4. **Text tool** — sketched text via OCCT `StdPrs_BRepFont`
5. **CAM** — beyond the current scaffolding (face milling shipped)

See [V1-Roadmap](V1-Roadmap) for current status.

## Cross-Platform

Must compile on Windows (MSVC) and POSIX (Linux/macOS, GCC/Clang). OCCT DLLs vendored, FreeType as git submodule.

## Rules for Contributions

- React does NOT own CAD state
- IPC protocol is the contract — schema, types, C++ dispatch, and docs move together
- All features follow the contextual modeling workflow
- Live previews are real geometry from the core
- Changes stay minimal and scoped — no vibe-coded rewrites
- `dev` is the default branch; feature branches from latest `dev`
