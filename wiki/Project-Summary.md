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
- 2D sketch system: lines, rectangles, circles, arcs, points, dimensions, constraints (H/V, coincident, parallel, perpendicular, equal-length, tangent), closed-profile detection
- Contextual modeling workflow: select → invoke → floating panel → real geometry preview → confirm/cancel
- Extrude (new body) with live depth editing
- Edge/vertex selection, **Fillet & Chamfer** on edges with live preview panels
- Sketches on origin planes or solid faces; sketch re-entry
- 2D sketch fillets (line-line corners)
- Project tool (face/edge/vertex projection into active sketch)
- Save/load `.polysmith` documents (core-owned JSON format)
- STEP + STL export
- OrcaSlicer integration (native window embedding via X11/XWayland)
- Offset construction planes
- Unified selection filter panel: selection, snapping, and constraints controlled by one checkbox panel
- Comprehensive snap system (endpoint, midpoint, center, quadrant, intersection, nearest, tangent, perpendicular, parallel, polar, grid)
- Sketches on offset planes from faces/reference planes

## The Project's Mantra: Topological Naming Problem (TNP)

"Never store a naked OCCT topology index and trust it across recomputes." Every feature that references 3D geometry must **re-resolve** its references against live body shapes on every recompute. When resolution fails, degrade gracefully (`dependency_broken` + warning), never crash. This is handled for edges (fillet/chamfer), faces (construction planes, face-based sketches), and sketch projections.

## Binding UX Pattern: Contextual Modeling Workflow

Every modeling feature follows: **select inputs → invoke action → floating context panel → real geometry preview (core-recomputed) → confirm or cancel.** No fake previews, no modal dialogs. Enter confirms, Escape cancels with `undo`.

## What's Next (V1 Roadmap)

The project is roughly mid-way through Milestone 3. The next big items:

1. **Cut/subtract extrude** — needs a new viewport mesh primitive for boolean'd bodies (single largest UX unlock)
2. **Hole feature** — simple/counterbore/countersink on a face
3. **Pattern features** — linear and circular
4. **Mirror** — body/feature mirror about a plane
5. **Measure tool**, named parameters, view cube, display units toggle

## Cross-Platform

Must compile on Windows (MSVC) and POSIX (Linux/macOS, GCC/Clang). OCCT DLLs vendored, FreeType as git submodule.

## Rules for Contributions

- React does NOT own CAD state
- IPC protocol is the contract — schema, types, C++ dispatch, and docs move together
- All features follow the contextual modeling workflow
- Live previews are real geometry from the core
- Changes stay minimal and scoped — no vibe-coded rewrites
- `dev` is the default branch; feature branches from latest `dev`
