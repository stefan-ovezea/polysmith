# CAM Development Plan

> **Status:** Scaffolding in progress. The CAM workspace is wired, the UI
> skeleton is in place, and the TNP witness resolution core is implemented.
> This document defines the approach and tracks progress. It is a living
> design doc, not a spec carved in stone.

## What's Built (Session 2026-06-01)

### UI — CAM Workspace Skeleton

- **Mode switcher:** CAM entry in the workspace dropdown alongside CAD / Slicer.
  Switching preserves the model — CAM inherits the CAD document.
- **Sub-category tabs:** Milling / Turning / Printing / Cutting in the header
  ribbon, matching the CAD Create / Modify / Construct / Sketch pattern.
- **Per-category toolbars:** Each tab has its own toolbar with category-specific
  tools and a shared "Setup" button. Unimplemented tools render as disabled
  (greyed out) so the layout is visible but nothing is wired yet.
  - **Milling:** Profile / Pocket / Drill (active) + Face / Contour / Engrave (disabled)
  - **Turning:** Rough / Finish / Groove / Thread (all disabled)
  - **Printing:** Slice / Support / Infill (all disabled)
  - **Cutting:** 2D Cut / Nest / Lead In (all disabled)
- **CAM operations panel:** Sidebar panel replacing the CAD hierarchy tree in
  CAM mode. Lists CAM operations with type labels. Currently shows empty state.

### C++ — TNP Witness Resolution

- **`cam_operation.h`** — `CamFaceReference` struct (body id, sample points,
  captured area, captured normal) and resolution API:
  `capture_face_reference()` / `resolve_face_reference()`.
- **`cam_operation.cpp`** — Full implementation: UV-grid point sampling,
  point-on-face testing via `BRepClass_FaceClassifier`, area comparison,
  normal comparison, weighted scoring, ambiguity detection.
- **`cam_face_reference_test.cpp`** — Three test scenarios build and pass:
  1. Capture and re-resolve on same body → Found with correct index
  2. Resolve on wrong body → NotFound
  3. Resolve via DocumentState convenience API → Found

### Documentation

- **`wiki/CAM-Development.md`** — Full architecture plan with v1 operations
  (Profile → Drill → Pocket), data structures, IPC considerations,
  progressive preview pipeline, and "what NOT to build" scope.

## Relationship to Existing Architecture

CAM is the third workspace in PolySmith's mode switcher, alongside CAD and
Slicer. It is **not** a separate application — it lives inside the same
process, shares the same document, and extends the same architecture.

| Layer | CAD owns | CAM adds |
|---|---|---|
| UI (React) | Toolbars, panels, viewport | `CamToolbar`, CAM operation panel, toolpath visualization |
| Core (C++) | Features, sketches, bodies | `CAMOperation` tree, tool library, toolpath generation |
| IPC | JSON commands/responses | CAM-specific commands (same transport, new message types) |

**Rule:** CAM code extends the existing modules — it does not fork them.
New `.cpp`/`.h` files go in `native/cad-core/src/core/` alongside the
existing feature files. New IPC message types extend the protocol schema.
New React components live under `apps/desktop-ui/src/layout/`.

See also: [Architecture Overview](Architecture-Overview),
[IPC Protocol](IPC-Protocol).

---

## Critical Data Model Changes

The current document model assumes **feature history = final solid**. CAM
needs three additional concepts that live *alongside* the feature tree, not
inside it.

### 1. CAM Operation Tree

Parallel to the CAD feature tree. Operations reference CAD geometry but are
stored and recomputed independently.

```
Document
├── FeatureHistory (CAD — exists today)
│   ├── Sketch "Sketch 1"
│   ├── Extrude "Body"
│   └── Fillet "Edge 1"
├── CamOperations (CAM — new)
│   ├── ProfileOp "Outer contour"
│   ├── PocketOp "Pocket A"
│   └── DrillOp "Hole pattern"
├── ToolLibrary (CAM — new)
└── StockDefinition (CAM — new)
```

Operations do NOT modify the B-rep. They produce toolpaths from it. When the
CAD feature tree recomputes and the body changes, CAM operations re-resolve
their geometry references and invalidate cached toolpaths.

### 2. Tool Library

A flat list of tool definitions. Each tool has:

- **Identity:** name, tool number
- **Geometry:** diameter, flute length, overall length, corner radius
- **Cutting data:** spindle speed, feed rate, stepover, stepdown
- **Material:** not needed for v1 (assume "general")

The tool library is stored in the document. It does not need a "feature-like"
recompute cycle — it's static data the user edits.

### 3. Stock Model

The raw material the part is cut from. In v1 this can be simple: a bounding
box around the final part with configurable offsets. The stock is separate
from the CAD body — it's a *definition* of what material exists before
cutting, not a feature of the part.

Storage: `{ width, height, depth, offset_x, offset_y, offset_z }` in the
document. Visualization: a translucent box in the viewport.

### 4. Toolpaths as Generated Geometry

Toolpaths are NOT part of the B-rep. They are a separate data structure:
lists of moves (rapids and feeds) with 3D coordinates. They are generated on
demand from CAM operations and cached. When inputs change (geometry, tool,
parameters), the cache is invalidated.

Toolpaths live in memory only — they are too large for the JSON document
format. They can be regenerated from the operation parameters + the CAD body
at any time.

---

## TNP for CAM: Why It's Harder

The [Topological Naming Problem](Topological-Naming-Problem) is already
PolySmith's mantra. CAM makes it worse.

### CAD TNP vs CAM TNP

| Aspect | CAD (fillet, chamfer) | CAM (profile, pocket) |
|---|---|---|
| Failure mode | Feature breaks, user fixes | Wrong G-code → machine crash |
| Resolution | Re-resolve by geometry | Re-resolve by geometry |
| Ambiguity | Usually 0 or 1 match | Often multiple candidates |
| Cost of wrong guess | Annoying | Dangerous |

### Geometric Attestation

The study proposes storing extra geometric "witness data" with each CAM
reference to survive recompute. This is the right instinct. A sketch of the
approach:

```cpp
struct FaceReference {
    std::string featureId;       // which CAD feature owns the face
    int semanticIndex;           // e.g. "3rd face of this feature"
    std::vector<gp_Pnt> samplePoints;  // ~10 points on the face surface
    double area;                 // approximate area at capture time
    gp_Dir approximateNormal;    // for disambiguation
};
```

On recompute:

1. Get the body from `featureId`.
2. Walk all faces. For each face:
   - Check if sample points lie on it (within tolerance).
   - Check area similarity (±10%).
   - Check normal similarity.
3. Score candidates. If exactly one clear match → use it.
4. If multiple candidates → present choices to the user (**never guess**).
5. If zero candidates → `dependency_broken` + warning (same pattern as CAD).

This is the same "re-resolve, never store naked indices" strategy from the
TNP doc, extended with richer witness data for the harder problem.

### Ambiguous Resolution UX

When recompute produces multiple candidate faces, the UI must:

1. Show a warning in the CAM operation panel: "Geometry reference is
   ambiguous — select the correct face."
2. Highlight candidate faces in the viewport (different color).
3. Let the user click to confirm which face is correct.
4. Update the witness data with the confirmed face.

This is a new UX pattern that doesn't exist in CAD yet. CAD features either
resolve or break — they don't present choices. CAM needs this.

---

## The Preview Pipeline

CAD previews are cheap: recompute a fillet, tessellate, send triangles to the
viewport. CAM previews are expensive: toolpath generation involves CL-point
calculations, offset curves, arc fitting, and potentially collision checks.

### Progressive Preview

The study's three-level proposal is solid. Here it is refined with concrete
timing targets:

| Level | What it shows | Target latency | Trigger |
|---|---|---|---|
| **Wireframe** | Bounding path of the tool (2D contour on the face plane) | < 50 ms | Automatic on parameter change |
| **Low-res** | Sparse toolpath points, no arc fitting | 100-500 ms | "Preview" button or debounced auto |
| **Full** | Complete toolpath with arcs, engagement angles | 1-30 s (async) | "Generate" button |

Levels 1 and 2 are **cancellable**. If the user changes a parameter while a
low-res preview is computing, the old computation is abandoned.

### How This Differs from CAD Previews

CAD previews follow the [contextual modeling workflow](Contextual-Modeling-Workflow):
select → invoke → live preview → confirm. They are *blocking* — the preview
is recomputed on every parameter change and must finish before the next update.

CAM cannot use blocking previews. The modified pattern:

```
CAM: select geometry → set parameters → [Preview] button
     → wireframe appears immediately
     → low-res appears if fast enough
     → user clicks [Generate] for full toolpath
     → progress bar + cancellation
```

Two distinct actions: **Preview** (fast, approximate) and **Generate**
(slow, final). The UI must make this distinction clear.

---

## IPC Considerations

### When JSON Over Pipe Is Enough

For v1, JSON over stdin/stdout is sufficient for:

- CAM operation CRUD (create, update, delete, list)
- Tool library management (small data)
- Stock definition (few numbers)
- Toolpath metadata (bounding box, point count, status)
- Wireframe preview data (a few hundred points)

These messages are comparable in size to existing CAD commands.

### When You Need More

The study is right that full toolpaths (megabytes of points for complex
parts) will eventually overwhelm JSON over pipe. But this is a **v2 concern**.
For v1:

- Full toolpaths stay in C++ memory.
- The UI requests *chunks* of the toolpath for visualization:
  ```
  UI: { "command": "cam_toolpath_chunk", "op_id": "...", "start": 0, "count": 1000 }
  Core: { "points": [...], "has_more": true }
  ```
- Chunk size is capped at ~1000 points (~24 KB) to keep JSON messages
  reasonable.
- The viewport draws what it has and requests more asynchronously.

This is simpler than mmap/shared memory and works cross-platform with zero
new infrastructure. When toolpaths genuinely exceed what chunked JSON can
handle (v2+), introduce a binary streaming channel.

### New IPC Message Types (v1)

```typescript
// CAM operation lifecycle
"cam_op_create": { type, inputs, tool_id, params } → { op_id }
"cam_op_update": { op_id, params } → { status }
"cam_op_delete": { op_id } → { status }
"cam_op_list": {} → { operations: [...] }

// Preview
"cam_op_preview": { op_id, level: "wireframe"|"lowres" } → { path: Point3D[], bounding_box }
"cam_op_generate": { op_id } → { job_id }
"cam_job_status": { job_id } → { progress: 0-100, status: "computing"|"done"|"error" }

// Toolpath streaming
"cam_toolpath_chunk": { op_id, start, count } → { points: Point3D[], has_more }

// Tool library
"cam_tool_list": {} → { tools: [...] }
"cam_tool_add": { definition } → { tool_id }
"cam_tool_update": { tool_id, definition } → { status }

// Stock
"cam_stock_set": { width, height, depth, offsets } → { status }
"cam_stock_get": {} → { definition }

// Post-processing
"cam_postprocess": { op_ids: string[], post_name: string } → { gcode: string }
```

---

## V1 CAM Operations (Prioritized)

The study's ordering is correct. Here it is with concrete scope for each:

### 1. 2.5D Profile (First — Easiest)

**What it does:** Select a closed wire on a planar face. Generate toolpath
that follows the contour, offset by tool radius (inside/outside/on-line).

**Scope for v1:**
- Single closed wire input
- Inside / outside / on-line offset modes
- Single pass at fixed depth (no multi-pass)
- No lead-in/lead-out
- No tabs
- No collision detection
- Zigzag or contour-only strategy

**Why first:** It exercises the entire CAM pipeline (select geometry →
TNP-resolved reference → parameter input → toolpath generation → viewport
display → post-process) with the simplest math. Offset curves are well
understood. OCCT's `BRepOffsetAPI_MakeOffset` can do the heavy lifting.

### 2. Drilling (Second — Easy, Requires Tool Table)

**What it does:** Select points on a planar face. Generate G81/G83 cycles.

**Scope for v1:**
- Point selection (sketch points, circle centers, or free picks)
- G81 (simple drill) and G83 (peck drill) cycles
- Depth, peck depth, retract height parameters
- No spot drilling, no chip breaking beyond G83

**Why second:** Drilling is computationally trivial (single points, no offset
curves, no path planning). It forces the tool library to exist. It produces
immediately useful G-code. The main work is the tool table UI and the cycle
parameter form.

### 3. Pocket Clearing (Third — Medium)

**What it does:** Select a face with optional islands. Clear material inside
the boundary.

**Scope for v1:**
- Single face, counter-clockwise outer boundary
- Optional islands (clockwise inner boundaries)
- Parallel line toolpath (zigzag at configurable angle)
- Stepover from tool diameter
- Single depth pass
- No adaptive clearing, no trochoidal paths, no rest machining

**Why third:** Pocketing with islands is the first operation that requires
real path planning (not just offsetting a curve). OCCT's 2D offset and
boolean operations help, but the fill pattern is custom code. This is where
the TNP "ambiguous resolution" problem first shows up in practice — islands
are faces, and faces can change IDs.

---

## Suggested C++ Data Structures

These extend the pattern in `native/cad-core/src/core/feature.h`. CAM types
go in a new header `cam_operation.h` in the same directory.

```cpp
// native/cad-core/src/core/cam_operation.h

#pragma once
#include <string>
#include <vector>
#include <optional>

namespace polysmith::core {

// ── Tool definition ──────────────────────────────────────────────

struct CamToolDefinition {
    std::string id;
    std::string name;
    int toolNumber = 1;

    // Geometry
    double diameter = 6.0;         // mm
    double fluteLength = 20.0;
    double overallLength = 60.0;
    double cornerRadius = 0.0;

    // Cutting data (defaults, overridable per-operation)
    double spindleSpeed = 12000;   // RPM
    double feedRate = 1000;        // mm/min
    double stepover = 2.0;         // mm (radial engagement for pocketing)
    double stepdown = 1.0;         // mm (axial depth per pass)

    // Type hint (for UI grouping)
    enum class Type { EndMill, BallMill, Drill, VBit } type = Type::EndMill;
};

// ── Stock definition ─────────────────────────────────────────────

struct CamStockDefinition {
    double width = 100.0;
    double height = 100.0;
    double depth = 20.0;
    double offsetX = 5.0;   // extra material beyond part bounds
    double offsetY = 5.0;
    double offsetZ = 5.0;
};

// ── Operation types ──────────────────────────────────────────────

enum class CamOperationType { Profile, Pocket, Drill };

// ── Geometry reference (TNP-resilient) ───────────────────────────

struct CamGeometryReference {
    std::string featureId;                // owning CAD feature
    int semanticIndex = 0;                // position in feature's output
    std::vector<std::array<double, 3>> samplePoints;  // witness data
    double capturedArea = 0.0;
    std::array<double, 3> capturedNormal = {0, 0, 0};
};

// ── Operation parameters (per-type) ──────────────────────────────

struct ProfileParameters {
    enum class Side { Inside, Outside, OnLine } side = Side::Outside;
    double depth = 1.0;
    double extraStock = 0.0;  // finishing allowance
};

struct PocketParameters {
    double depth = 1.0;
    double stepover = 2.0;    // overrides tool default if set
    double angleDeg = 0.0;    // zigzag angle
};

struct DrillParameters {
    enum class Cycle { G81_Simple, G83_Peck } cycle = Cycle::G81_Simple;
    double depth = 5.0;
    double peckDepth = 1.0;   // only for G83
    double retractHeight = 5.0;
};

// ── Unified operation ────────────────────────────────────────────

struct CamOperation {
    std::string id;
    CamOperationType type;
    std::string toolId;
    std::vector<CamGeometryReference> inputs;

    // Per-type parameters (only one is active)
    std::optional<ProfileParameters> profile;
    std::optional<PocketParameters> pocket;
    std::optional<DrillParameters> drill;

    // Cached toolpath — invalidated when inputs/params change
    bool toolpathValid = false;
};

// ── Toolpath (generated geometry, not stored in document) ────────

struct CamToolpathPoint {
    double x, y, z;
};

struct CamToolpathMove {
    bool isRapid = false;         // G0 vs G1
    std::vector<CamToolpathPoint> points;
};

struct CamToolpath {
    std::vector<CamToolpathMove> moves;
    double minX, maxX, minY, maxY, minZ, maxZ;
    int totalPoints = 0;          // for chunked streaming
};

// ── Post-processor abstraction ───────────────────────────────────

enum class CamPostProcessor { LinuxCNC, Mach3, Grbl, GenericGCode };

} // namespace polysmith::core
```

### Design Decisions

- **Tool library lives in the document**, not in a separate file. This keeps
  everything self-contained and serializable. Users can export/import tool
  tables later.
- **Parameters are per-operation-type structs**, using `std::optional` for
  the active one. This avoids a giant union/visitor and keeps serialization
  simple.
- **Toolpath is NOT serialized** to the document. The `toolpathValid` flag
  drives regeneration on load. Toolpaths are too large for JSON and are
  pure functions of operation + body anyway.
- **`CamGeometryReference` stores witness data** inline. This is deliberately
  not a pointer to a face — it's data the resolve step uses to re-find the
  face on recompute.

---

## Implementation Progress

| Step | Status |
|---|---|
| 1. TNP Witness Resolution | ✅ Done — `cam_operation.h/.cpp`, test passes |
| 2. CAM Panel UI skeleton | ✅ Done — sub-category tabs, per-category toolbars, operations panel |
| 3. Toolpath visualization in viewport | 🔲 Next |
| 4. Post-processor skeleton | 🔲 After |

## Next Step: Toolpath Visualization in the Viewport

The viewport needs to display CAM toolpath lines before any generation code
can be tested. This is the next gate — without it, toolpath generation
produces data with no way to see it.

**What to build:**

1. **New viewport primitive type** — toolpath lines as colored polylines
   (rapid moves in one color, feed moves in another). Distinct from CAD
   sketch lines. C++ side in `viewport.h/.cpp`, sent via the existing
   `ViewportState` IPC message.

2. **IPC extension** — add an optional `toolpaths` field to the viewport
   state message. Each toolpath entry has: an id, a list of 3D points,
   and a per-segment type (rapid/feed). Start with small payloads (~1000
   points per chunk).

3. **Test with hardcoded data** — inject a sample toolpath (e.g. a square
   contour with lead-in) from C++ into the viewport state, render it,
   and verify colors are correct. No toolpath generation needed yet —
   just the display pipeline.

**Why this before the post-processor:** Toolpath visualization lets you
visually verify generated toolpaths. The post-processor converts toolpaths
to G-code text — you need to see the toolpath first to know if the G-code
is even right.

---

## What NOT to Build in V1

- **Collision detection.** Assume the user knows what they're doing. Toolpath
  visualization lets them see obvious problems.
- **Multi-pass roughing.** Single pass at full depth. Multi-pass is a
  parameterization change, not an architectural one — add it later.
- **Adaptive/trochoidal toolpaths.** Complex math, v2+.
- **4/5-axis.** 2.5D only. Everything is planar.
- **Simulation.** Visual preview only, no material removal simulation.
- **Tool wear compensation.** Not needed for hobbyist use.
- **Binary IPC transport.** Chunked JSON is sufficient through v1.

---

## Cross-Platform Notes

- **Windows:** Target Mach3/Mach4 and Grbl (common in hobby CNC). The IPC
  wizard that launches OrcaSlicer can launch a post-processor.
- **Linux:** LinuxCNC is the primary target. Native integration possible
  (same machine runs both CAD and controller).
- **macOS:** Development and simulation only — few real machines. Good
  platform for testing because it catches POSIX assumptions.

The post-processor abstraction hides platform differences. The G-code output
is plain text — the transport (save to file, send over serial, network
socket) is a separate concern outside the core.

---

## References

- [Architecture Overview](Architecture-Overview) — system layout
- [Topological Naming Problem](Topological-Naming-Problem) — TNP strategy
- [IPC Protocol](IPC-Protocol) — communication contract
- [Contextual Modeling Workflow](Contextual-Modeling-Workflow) — binding UX pattern (CAD version; CAM adapts it)
- [V1 Roadmap](V1-Roadmap) — existing priorities (CAM is post-V1)
