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
  - **Milling:** Face / Pocket / Contour / Drill / Adaptive (active) + Slot / Chamfer / Thread (disabled)
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
  (Face Milling → 2D Pocket → 2D Contour → Drilling → Adaptive Clearing),
  data structures, IPC considerations, progressive preview pipeline,
  and "what NOT to build" scope.

---

## CAM Operation Taxonomy

PolySmith's CAM workspace covers four machine types. Milling is the v1
focus; the others are scaffolded for future expansion.

### Milling

```
Milling Toolbox
│
├── SETUP (required first)
│   ├── Machine Definition (3-axis, 4-axis, 5-axis)
│   ├── Stock (fixed bounding box, from solid, from mesh)
│   ├── Work Coordinate System (WCS) origin
│   ├── Safety plane & retract settings
│   └── Tool change position
│
├── 2.5D Operations (constant Z)
│   ├── Face Milling (top surface cleanup)
│   ├── Pocket (closed region, flat bottom)
│   ├── Contour (profile around outside)
│   ├── Slot (open-ended pocket)
│   ├── Chamfer (along edges at fixed angle)
│   └── Thread Milling (holes with threads)
│
├── 3D Operations (variable Z)
│   ├── Adaptive Clearing (dynamic roughing, constant engagement)
│   ├── Parallel Passes (zig-zag, one-way)
│   ├── Contour (follows part shape at varying Z)
│   ├── Pencil Milling (cleanup of fillets/corners)
│   ├── Rest Machining (remove material left by larger tool)
│   └── Project Curve (drive tool along sketched curve on 3D surface)
│
├── Drilling Operations
│   ├── Simple Drill (peck/no peck)
│   ├── Spot Drill (start holes precisely)
│   ├── Counterbore
│   ├── Countersink
│   ├── Tap (rigid/non-rigid)
│   └── Circular Pocket (drill + helical interpolate)
│
├── Advanced (TNP warning area)
│   ├── Multi-Axis (4th/5th axis continuous)
│   ├── Swarf Cutting (wall milling with tool side)
│   └── Port/Channel (specialized)
│
└── UTILITIES
    ├── Transform (rotate/translate existing toolpath)
    ├── Array (linear/circular pattern of operation)
    ├── Mirror
    └── Boundary Trim (crop toolpath to region)
```

### Turning

```
Turning Toolbox
├── Setup
│   ├── Chuck (jaw type, gripping diameter)
│   ├── Tailstock (center or no)
│   └── Stock (diameter, length)
├── Operations
│   ├── Rough Turning (remove bulk, zig-zag)
│   ├── Finish Turning (final profile)
│   ├── Facing (end of part)
│   ├── Grooving (parting tool, undercuts)
│   ├── Threading (external/internal)
│   └── Drilling (from tailstock)
└── Utilities
    ├── Part-off (cut finished part from stock)
    └── Pick-off (transfer to sub-spindle)
```

### Cutting (Laser / Plasma / Waterjet)

```
Cutting Toolbox
├── Setup
│   ├── Material (type, thickness)
│   ├── Kerf Width (cut width compensation)
│   ├── Lead-in/Lead-out (where pierce happens)
│   └── Piercing Rules (thick vs thin material)
├── 2D Operations
│   ├── External Cut (around part perimeter)
│   ├── Internal Cut (holes/pockets)
│   ├── Engrave (shallow mark, no through cut)
│   └── Score (partial depth, for bending)
└── Advanced
    ├── Nesting (arrange multiple parts on sheet)
    ├── Common Cut (cut adjacent parts together)
    └── Bridge/Tab (keep parts attached to sheet)
```

### Printing (Bridge to OrcaSlicer)

The Printing category bridges to the existing OrcaSlicer integration
rather than duplicating slicer logic. PolySmith owns model export;
OrcaSlicer owns slicing, supports, and G-code generation.

```
Printing (bridge to OrcaSlicer)
├── Setup
│   ├── Printer Profile (bed size, nozzle)
│   ├── Material Profile (PLA/ABS/PETG)
│   └── Quality Profile (layer height, infill)
├── Operations (Orca parameters)
│   ├── Perimeters (walls count, order)
│   ├── Infill (pattern, density)
│   ├── Supports (type, overhang angle)
│   └── Raft/Brim
└── Position
    ├── Bed Layout (rotate, scale, duplicate)
    └── Auto-orient
```

### Machine-Type Differences at a Glance

| Aspect | Milling | Turning | Printing | Cutting |
|---|---|---|---|---|
| Motion | 3+ axes linear | 2 axes + spindle | 3 axes + extrusion | 2 axes + pierce |
| Tool | Endmill/ball/drill | Turning insert | Nozzle | Laser/plasma/water |
| Stock | Block | Cylinder | None (additive) | Sheet |
| Key ops | Pocket, contour, drill | Rough, finish, groove | Perimeter, infill, support | Cut, engrave, score |
| Simulation | Material removal | Rotating solid | Layer buildup | Sheet cut |

---

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

All CAM messages follow the existing protocol envelope — `{ id, type, payload }`
over newline-delimited JSON on `stdin`/`stdout`. See [IPC Protocol](IPC-Protocol)
for transport rules.

#### Definitions — Common Data Types

These reusable definitions appear throughout the CAM message set. The
`geometry_reference` definition maps directly to the existing C++
`CamFaceReference` struct in `cam_operation.h`.

```json
{
  "definitions": {
    "point_3d": {
      "type": "object",
      "properties": {
        "x": { "type": "number" },
        "y": { "type": "number" },
        "z": { "type": "number" }
      },
      "required": ["x", "y", "z"]
    },

    "bounding_box": {
      "type": "object",
      "properties": {
        "min_x": { "type": "number" }, "min_y": { "type": "number" }, "min_z": { "type": "number" },
        "max_x": { "type": "number" }, "max_y": { "type": "number" }, "max_z": { "type": "number" }
      }
    },

    "geometry_reference": {
      "type": "object",
      "description": "TNP-resistant reference to CAD geometry. Maps to CamFaceReference in cam_operation.h. The core re-resolves these against live body shapes on every recompute.",
      "properties": {
        "reference_type": {
          "type": "string",
          "enum": ["face", "edge", "vertex", "wire", "solid", "sketch"]
        },
        "primary_identification": {
          "type": "object",
          "properties": {
            "persistent_id": { "type": "string", "description": "Stable id: <body_id>:<kind>:<index>" },
            "feature_id": { "type": "string" },
            "generation_id": { "type": "string" }
          }
        },
        "geometric_attestation": {
          "type": "object",
          "description": "Witness data stored at capture time. Used by resolve_face_reference() to re-identify the face after topology changes.",
          "properties": {
            "sample_points": {
              "type": "array",
              "items": { "$ref": "#/definitions/point_3d" },
              "maxItems": 16,
              "description": "World-space points distributed across the face surface. Maps to CamFaceReference.samplePoints."
            },
            "area": { "type": "number", "description": "Approximate area at capture time (mm²). Maps to CamFaceReference.capturedArea." },
            "bounds": { "$ref": "#/definitions/bounding_box" },
            "is_planar": { "type": "boolean" },
            "plane_normal": { "$ref": "#/definitions/point_3d", "description": "Surface normal at face center. Maps to CamFaceReference.capturedNormal." },
            "hole_diameter": { "type": "number" }
          }
        },
        "fallback_strategy": {
          "type": "string",
          "enum": ["fail_operation", "warn_user", "auto_resolve_by_geometry", "skip_toolpath_segment", "require_revalidation"],
          "default": "fail_operation",
          "description": "What to do when re-resolution fails. Maps to dependency_broken + dependency_warning in the CAD feature model."
        }
      },
      "required": ["reference_type", "primary_identification", "geometric_attestation"]
    },

    "tool_geometry": {
      "type": "object",
      "properties": {
        "tool_id": { "type": "string" },
        "tool_type": {
          "type": "string",
          "enum": ["endmill_flat", "endmill_ball", "endmill_bull", "drill", "facemill", "chamfer", "threadmill", "turning_insert", "laser", "plasma"]
        },
        "diameter_mm": { "type": "number", "minimum": 0.1 },
        "corner_radius_mm": { "type": "number", "minimum": 0 },
        "flute_length_mm": { "type": "number" },
        "overall_length_mm": { "type": "number" },
        "taper_angle_deg": { "type": "number", "default": 0 }
      },
      "required": ["tool_id", "tool_type", "diameter_mm"]
    },

    "cutting_parameters": {
      "type": "object",
      "properties": {
        "spindle_rpm": { "type": "number", "minimum": 0, "maximum": 50000 },
        "feedrate_mm_per_min": { "type": "number", "minimum": 1 },
        "plunge_feedrate_mm_per_min": { "type": "number" },
        "stepdown_mm": { "type": "number", "minimum": 0.01 },
        "stepover_mm": { "type": "number" },
        "cutting_direction": { "type": "string", "enum": ["climb", "conventional", "mixed"] },
        "coolant": { "type": "string", "enum": ["off", "flood", "mist", "through_tool"] }
      },
      "required": ["spindle_rpm", "feedrate_mm_per_min"]
    },

    "stock_definition": {
      "type": "object",
      "properties": {
        "stock_type": {
          "type": "string",
          "enum": ["bounding_box", "from_solid", "from_mesh", "cylindrical"]
        },
        "bounding_box": { "$ref": "#/definitions/bounding_box" },
        "solid_reference": { "$ref": "#/definitions/geometry_reference" },
        "mesh_reference": { "$ref": "#/definitions/geometry_reference" },
        "diameter_mm": { "type": "number", "description": "For cylindrical stock (turning)" },
        "length_mm": { "type": "number", "description": "For cylindrical stock (turning)" }
      }
    }
  }
}
```

#### Setup

```json
{
  "cam_setup_create": {
    "description": "Create or update the CAM setup for a document. Must exist before any milling operation. Setup is document-level config — not a regular operation.",
    "params": {
      "name": { "type": "string" },
      "machine_config": {
        "type": "object",
        "properties": {
          "machine_type": {
            "type": "string",
            "enum": ["3_axis", "4_axis", "5_axis", "lathe_2_axis", "lathe_live_tooling", "laser", "plasma", "printer"]
          },
          "axes": {
            "type": "object",
            "properties": {
              "x_mm": { "type": "number" }, "y_mm": { "type": "number" }, "z_mm": { "type": "number" },
              "a_deg": { "type": "number" }, "b_deg": { "type": "number" }, "c_deg": { "type": "number" }
            }
          },
          "max_spindle_rpm": { "type": "number" },
          "max_feedrate_mm_per_min": { "type": "number" },
          "tool_change_position": { "$ref": "#/definitions/point_3d" },
          "safety_plane_height_mm": { "type": "number" },
          "retract_height_mm": { "type": "number" }
        },
        "required": ["machine_type"]
      },
      "stock": { "$ref": "#/definitions/stock_definition" },
      "wcs_origin": { "$ref": "#/definitions/point_3d" },
      "part_solid_reference": { "$ref": "#/definitions/geometry_reference" },
      "units": { "type": "string", "enum": ["mm", "inch"], "default": "mm" }
    },
    "response": {
      "setup_id": { "type": "string" },
      "status": { "type": "string", "enum": ["ok", "invalid_stock", "machine_limits_exceeded"] },
      "warnings": { "type": "array", "items": { "type": "string" } }
    }
  },

  "cam_setup_get": {
    "params": {},
    "response": {
      "setup": { "$ref": "#/methods/cam_setup_create/params" }
    }
  }
}
```

#### CAM Operation Lifecycle

```json
{
  "cam_operation_create": {
    "params": {
      "setup_id": { "type": "string", "description": "Must reference an existing setup" },
      "operation_type": {
        "type": "string",
        "enum": ["face_milling", "pocket_2d", "contour_2d", "drilling", "adaptive_clearing", "parallel_3d", "chamfer", "thread_milling", "slot", "engrave"]
      },
      "name": { "type": "string" },

      "geometry_references": {
        "type": "object",
        "description": "TNP-resolved references to CAD geometry. The core re-resolves these against live body shapes on every recompute.",
        "properties": {
          "machining_regions": {
            "type": "array",
            "items": { "$ref": "#/definitions/geometry_reference" },
            "description": "Faces/edges/wires to machine"
          },
          "avoidance_regions": {
            "type": "array",
            "items": { "$ref": "#/definitions/geometry_reference" },
            "description": "Faces/edges to avoid (clamps, fixtures)"
          },
          "guide_curves": {
            "type": "array",
            "items": { "$ref": "#/definitions/geometry_reference" },
            "description": "Curves that drive the toolpath direction"
          },
          "check_surfaces": {
            "type": "array",
            "items": { "$ref": "#/definitions/geometry_reference" },
            "description": "Surfaces the tool must not violate"
          }
        }
      },

      "tool": { "$ref": "#/definitions/tool_geometry" },
      "parameters": { "$ref": "#/definitions/cutting_parameters" },

      "operation_specific_params": {
        "type": "object",
        "description": "Per-type parameters. Only the matching key is active.",
        "properties": {
          "face_milling": { "$ref": "#/methods/cam_operation_create/operation_params/face_milling" },
          "pocket_2d": { "$ref": "#/methods/cam_operation_create/operation_params/pocket_2d" },
          "contour_2d": { "$ref": "#/methods/cam_operation_create/operation_params/contour_2d" },
          "drilling": { "$ref": "#/methods/cam_operation_create/operation_params/drilling" },
          "adaptive_clearing": { "$ref": "#/methods/cam_operation_create/operation_params/adaptive_clearing" }
        }
      },

      "dependencies": {
        "type": "object",
        "properties": {
          "parent_operation_ids": {
            "type": "array",
            "items": { "type": "string" },
            "description": "Operations that must complete before this one"
          },
          "requires_stock_condition": {
            "type": "string",
            "enum": ["raw_stock", "any_previous", "specific_operation_id", "finish_passed"]
          },
          "stock_after_parent_id": { "type": "string" }
        }
      }
    },

    "operation_params": {
      "face_milling": {
        "type": "object",
        "properties": {
          "strategy": { "type": "string", "enum": ["zigzag", "one_way", "spiral", "offset"] },
          "stepover_percent": { "type": "number", "minimum": 10, "maximum": 90, "default": 70 },
          "stock_allowance_mm": { "type": "number", "default": 0.2 },
          "multiple_passes": { "type": "boolean", "default": false },
          "finish_pass": { "type": "boolean", "default": true }
        }
      },

      "pocket_2d": {
        "type": "object",
        "properties": {
          "clearing_strategy": { "type": "string", "enum": ["zigzag", "offset", "adaptive", "spiral"] },
          "stepover_percent": { "type": "number", "minimum": 10, "maximum": 90, "default": 45 },
          "rough_stepdown_mm": { "type": "number" },
          "finish_stepdown_mm": { "type": "number", "default": 0 },
          "finish_passes": { "type": "integer", "minimum": 0, "maximum": 5, "default": 1 },
          "island_handling": { "type": "string", "enum": ["ignore", "avoid", "machine_first"] },
          "corner_radius_mm": { "type": "number", "description": "Minimum toolpath radius for corners" }
        }
      },

      "contour_2d": {
        "type": "object",
        "properties": {
          "side": { "type": "string", "enum": ["inside", "outside", "on_line"] },
          "depth_mm": { "type": "number" },
          "stock_allowance_mm": { "type": "number", "default": 0.0 },
          "multiple_passes": { "type": "boolean", "default": false },
          "finish_passes": { "type": "integer", "minimum": 0, "maximum": 5, "default": 1 }
        }
      },

      "drilling": {
        "type": "object",
        "properties": {
          "cycle_type": {
            "type": "string",
            "enum": ["g81_standard", "g82_dwell", "g83_peck", "g73_high_speed_peck", "g84_tap", "g85_bore", "g87_back_bore"]
          },
          "hole_diameter_mm": { "type": "number" },
          "hole_depth_mm": { "type": "number" },
          "peck_depth_mm": { "type": "number", "description": "For peck cycles" },
          "dwell_seconds": { "type": "number", "default": 0 },
          "retract_height_mm": { "type": "number" },
          "start_height_mm": { "type": "number" }
        }
      },

      "adaptive_clearing": {
        "type": "object",
        "properties": {
          "stepdown_mm": { "type": "number" },
          "minimum_radius_mm": { "type": "number" },
          "stock_allowance_mm": { "type": "number", "default": 0.5 },
          "engagement_angle_deg": { "type": "number", "minimum": 5, "maximum": 45, "default": 30 },
          "helix_ramp_angle_deg": { "type": "number", "default": 3 }
        }
      }
    },

    "response": {
      "operation_id": { "type": "string" },
      "status": { "type": "string", "enum": ["ok", "invalid_setup", "geometry_resolution_failed", "dependency_unmet"] },
      "warnings": { "type": "array", "items": { "type": "string" } }
    }
  },

  "cam_operation_update": {
    "params": {
      "operation_id": { "type": "string" },
      "name": { "type": "string" },
      "geometry_references": { "$ref": "#/methods/cam_operation_create/params/geometry_references" },
      "tool": { "$ref": "#/definitions/tool_geometry" },
      "parameters": { "$ref": "#/definitions/cutting_parameters" },
      "operation_specific_params": { "$ref": "#/methods/cam_operation_create/params/operation_specific_params" }
    },
    "response": {
      "status": { "type": "string", "enum": ["ok", "not_found", "geometry_resolution_failed"] },
      "toolpath_invalidated": { "type": "boolean", "description": "True when cached toolpath was cleared" }
    }
  },

  "cam_operation_delete": {
    "params": { "operation_id": { "type": "string" } },
    "response": {
      "status": { "type": "string", "enum": ["ok", "not_found", "has_dependents"] },
      "blocked_by": { "type": "array", "items": { "type": "string" }, "description": "Operation IDs that depend on this one" }
    }
  },

  "cam_operation_list": {
    "params": {},
    "response": {
      "operations": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "id": { "type": "string" },
            "name": { "type": "string" },
            "operation_type": { "type": "string" },
            "tool_id": { "type": "string" },
            "toolpath_valid": { "type": "boolean" },
            "dependency_broken": { "type": "boolean" }
          }
        }
      }
    }
  }
}
```

#### Toolpath Preview & Generation

```json
{
  "cam_toolpath_preview": {
    "params": {
      "operation_id": { "type": "string" },
      "preview_level": {
        "type": "string",
        "enum": ["bounding_box", "wireframe", "low_resolution", "full_resolution"]
      },
      "sample_stride": {
        "type": "integer",
        "default": 10,
        "description": "For low_resolution, show every Nth point"
      }
    },
    "response": {
      "job_id": { "type": "string" },
      "estimated_points": { "type": "integer" },
      "estimated_time_ms": { "type": "integer" }
    }
  },

  "cam_toolpath_generate": {
    "params": {
      "operation_id": { "type": "string" },
      "resolution": { "type": "string", "enum": ["draft", "standard", "high"], "default": "standard" }
    },
    "response": {
      "job_id": { "type": "string" }
    }
  },

  "cam_job_status": {
    "params": { "job_id": { "type": "string" } },
    "response": {
      "status": { "type": "string", "enum": ["queued", "generating", "complete", "failed", "cancelled"] },
      "progress_percent": { "type": "number", "minimum": 0, "maximum": 100 },
      "toolpath_id": { "type": "string", "description": "When status is complete" },
      "error_message": { "type": "string", "description": "When status is failed" },
      "tnp_warnings": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "reference_type": { "type": "string" },
            "warning_severity": { "type": "string", "enum": ["note", "warning", "error", "danger"] },
            "message": { "type": "string" },
            "fallback_action": { "type": "string" }
          }
        }
      }
    }
  },

  "cam_toolpath_chunk": {
    "params": {
      "toolpath_id": { "type": "string" },
      "start": { "type": "integer", "description": "0-based point index" },
      "count": { "type": "integer", "maximum": 1000, "description": "Max points per chunk" }
    },
    "response": {
      "points": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "x": { "type": "number" }, "y": { "type": "number" }, "z": { "type": "number" },
            "is_rapid": { "type": "boolean" },
            "feedrate": { "type": "number" }
          }
        }
      },
      "has_more": { "type": "boolean" }
    }
  }
}
```

#### TNP Validation

```json
{
  "cam_operation_validate": {
    "description": "Explicitly re-resolve an operation's geometry references against the current body shape. Returns the resolution status for every reference. Called automatically during toolpath generation; also callable from the UI to surface TNP warnings before generating.",
    "params": {
      "operation_id": { "type": "string" },
      "current_body_id": { "type": "string" }
    },
    "response": {
      "valid": { "type": "boolean" },
      "resolution_status": {
        "type": "object",
        "properties": {
          "references_resolved": { "type": "integer" },
          "references_resolved_with_warning": { "type": "integer" },
          "references_failed": { "type": "integer" },
          "failed_references": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "original_id": { "type": "string" },
                "type": { "type": "string" },
                "fallback_used": { "type": "boolean" },
                "candidate_matches": {
                  "type": "array",
                  "items": { "$ref": "#/definitions/geometry_reference" },
                  "description": "Non-empty when the core found multiple ambiguous matches (FaceResolutionOutcome::Ambiguous)"
                }
              }
            }
          }
        }
      },
      "dependency_check": {
        "type": "object",
        "properties": {
          "unmet_dependencies": { "type": "array", "items": { "type": "string" } },
          "circular_dependency": { "type": "boolean" }
        }
      },
      "suggested_fixes": { "type": "array", "items": { "type": "string" } }
    }
  }
}
```

#### Tool Library

```json
{
  "cam_tool_list": {
    "params": {},
    "response": {
      "tools": {
        "type": "array",
        "items": { "$ref": "#/definitions/tool_geometry" }
      }
    }
  },

  "cam_tool_add": {
    "params": { "$ref": "#/definitions/tool_geometry" },
    "response": {
      "tool_id": { "type": "string" },
      "status": { "type": "string", "enum": ["ok", "duplicate_id"] }
    }
  },

  "cam_tool_update": {
    "params": {
      "tool_id": { "type": "string" },
      "definition": { "$ref": "#/definitions/tool_geometry" }
    },
    "response": {
      "status": { "type": "string", "enum": ["ok", "not_found"] }
    }
  },

  "cam_tool_delete": {
    "params": { "tool_id": { "type": "string" } },
    "response": {
      "status": { "type": "string", "enum": ["ok", "not_found", "in_use"] },
      "used_by_operations": { "type": "array", "items": { "type": "string" } }
    }
  }
}
```

#### Stock

```json
{
  "cam_stock_set": {
    "params": { "$ref": "#/definitions/stock_definition" },
    "response": {
      "status": { "type": "string", "enum": ["ok", "invalid_bounds"] }
    }
  },

  "cam_stock_get": {
    "params": {},
    "response": {
      "stock": { "$ref": "#/definitions/stock_definition" }
    }
  }
}
```

#### G-Code Export

```json
{
  "cam_export_gcode": {
    "params": {
      "operation_ids": { "type": "array", "items": { "type": "string" }, "minItems": 1 },
      "post_processor": { "type": "string", "enum": ["fanuc", "linuxcnc", "mach3", "grbl", "marlin"] },
      "filename": { "type": "string" },
      "options": {
        "type": "object",
        "properties": {
          "add_line_numbers": { "type": "boolean", "default": true },
          "use_arcs": { "type": "boolean", "default": true },
          "absolute_coordinates": { "type": "boolean", "default": true },
          "tool_change_mcode": { "type": "integer", "default": 6 },
          "spindle_start_mcode": { "type": "integer", "default": 3 },
          "coolant_mcode_on": { "type": "integer", "default": 8 },
          "coolant_mcode_off": { "type": "integer", "default": 9 }
        }
      }
    },
    "response": {
      "status": { "type": "string", "enum": ["success", "failed", "partial"] },
      "filename": { "type": "string" },
      "line_count": { "type": "integer" },
      "warnings": { "type": "array", "items": { "type": "string" } }
    }
  }
}
```

#### Binary Protocol (v2)

For v1, toolpath data stays in C++ memory and the UI streams chunks via
`cam_toolpath_chunk`. When toolpaths routinely exceed what chunked JSON can
handle (v2+), introduce a binary streaming channel:

```json
{
  "binary_toolpath_header": {
    "description": "v2 only. Sent once before the binary move stream begins.",
    "magic": "CAMTP",
    "version": 1,
    "toolpath_id": "uuid",
    "num_moves": 100000,
    "has_arcs": true,
    "bounds": { "min_x": 0, "max_x": 100 }
  },

  "binary_move_record": {
    "description": "v2 only. 16 bytes per linear move.",
    "flags": { "is_rapid": 1, "has_feedrate": 0, "is_arc": 0 },
    "x": "f32", "y": "f32", "z": "f32",
    "feedrate": "f32"
  },

  "binary_arc_record": {
    "description": "v2 only. 24 bytes per arc move.",
    "flags": { "is_arc": 1, "cw": 1 },
    "x": "f32", "y": "f32", "z": "f32",
    "i": "f32", "j": "f32",
    "feedrate": "f32"
  }
}
```

#### Message Flow Example

```
1. User creates setup
   → { "type": "cam_setup_create", "id": 1, "payload": { ... } }
   ← { "type": "cam_setup_create", "id": 1, "payload": { "setup_id": "cam_setup_abc123", "status": "ok" } }

2. User selects pocket face — UI captures geometry reference with attestation
   (stored locally in React, sent to core with the create command)

3. Create pocket operation
   → { "type": "cam_operation_create", "id": 2, "payload": {
        "setup_id": "cam_setup_abc123",
        "operation_type": "pocket_2d",
        "geometry_references": {
          "machining_regions": [{
            "reference_type": "face",
            "primary_identification": { "persistent_id": "body_1:face:3", ... },
            "geometric_attestation": { "area": 1234.5, "sample_points": [...], "plane_normal": {...} },
            "fallback_strategy": "require_revalidation"
          }]
        },
        "tool": { "tool_id": "endmill_6mm", "tool_type": "endmill_flat", "diameter_mm": 6.0 },
        "parameters": { "spindle_rpm": 8000, "feedrate_mm_per_min": 1200, "stepdown_mm": 1.0 },
        "operation_specific_params": { "pocket_2d": { "stepover_percent": 45, "clearing_strategy": "offset" } }
      } }
   ← { "type": "cam_operation_create", "id": 2, "payload": { "operation_id": "cam_op_456", "status": "ok" } }

4. Preview (fast bounding box)
   → { "type": "cam_toolpath_preview", "id": 3, "payload": { "operation_id": "cam_op_456", "preview_level": "wireframe" } }
   ← { "type": "cam_toolpath_preview", "id": 3, "payload": { "job_id": "job_preview_1", "estimated_points": 200 } }

5. Generate full toolpath (async)
   → { "type": "cam_toolpath_generate", "id": 4, "payload": { "operation_id": "cam_op_456" } }
   ← { "type": "cam_toolpath_generate", "id": 4, "payload": { "job_id": "job_789" } }

6. Poll status
   → { "type": "cam_job_status", "id": 5, "payload": { "job_id": "job_789" } }
   ← { "type": "cam_job_status", "id": 5, "payload": { "status": "generating", "progress_percent": 45 } }

7. Stream toolpath chunks for viewport rendering
   → { "type": "cam_toolpath_chunk", "id": 6, "payload": { "toolpath_id": "tp_abc", "start": 0, "count": 1000 } }
   ← { "type": "cam_toolpath_chunk", "id": 6, "payload": { "points": [...], "has_more": true } }

8. If CAD body changes and TNP shifts topology — validate before regenerating
   → { "type": "cam_operation_validate", "id": 7, "payload": { "operation_id": "cam_op_456", "current_body_id": "body_after_edit" } }
   ← { "type": "cam_operation_validate", "id": 7, "payload": {
        "valid": false,
        "resolution_status": {
          "references_resolved": 0,
          "references_failed": 1,
          "failed_references": [{
            "original_id": "body_1:face:3",
            "candidate_matches": [ /* 2 ambiguous faces */ ],
            "fallback_used": false
          }]
        },
        "suggested_fixes": ["Re-select pocket face", "Roll back feature edit"]
      } }

9. Export G-code
   → { "type": "cam_export_gcode", "id": 8, "payload": { "operation_ids": ["cam_op_456"], "post_processor": "grbl", "filename": "pocket.nc" } }
   ← { "type": "cam_export_gcode", "id": 8, "payload": { "status": "success", "filename": "pocket.nc", "line_count": 843 } }
```

### Schema Design Decisions

1. **Separation of concerns.** Setup owns machine configuration and stock.
   Operations own geometry references, tool selection, and toolpath generation.
   They are distinct message types because they have distinct lifecycles.
2. **TNP attestation on every reference.** Every `geometry_reference` carries
   witness data (sample points, area, normal) at capture time. The core's
   `resolve_face_reference()` uses this to re-identify faces after topology
   changes. This maps 1:1 to the existing `CamFaceReference` struct.
3. **Preview levels.** `bounding_box` (fastest, ~ms) → `wireframe` (2D path
   outline) → `low_resolution` (sparse points) → `full_resolution` (complete).
   Matches the three-level progressive preview pipeline.
4. **Explicit validation.** `cam_operation_validate` lets the UI check TNP
   status before generating toolpaths. The core runs this automatically during
   `cam_toolpath_generate`, but the UI can call it proactively to surface
   warnings early.
5. **Fallback strategies per reference.** Each geometry reference declares
   what to do when re-resolution fails — fail the operation, warn and proceed,
   auto-resolve, or skip that segment. Default is `fail_operation` (same
   behaviour as CAD `dependency_broken`).
6. **Binary channel deferred to v2.** Chunked JSON via `cam_toolpath_chunk`
   is sufficient through v1. The binary protocol schema is documented here
   so the design accounts for it, but no v1 code implements it.
7. **Dependencies explicit.** Operations declare parent operation IDs and
   stock conditions. The core enforces ordering and prevents deletion of
   operations with dependents.

---

## V1 CAM Operations (Prioritized)

V1 targets Milling only — it has the broadest hobbyist audience and shares
the most infrastructure with CAD (3D solids, planar faces). Turning uses a
different motion model and is deferred to v2.

The v1 scope is five operations covering ~90% of hobbyist milling needs,
plus Setup as a special prerequisite:

### Setup (Required First — Special Operation)

Setup is not a regular CAM operation. It is a document-level configuration
that must exist before any milling operation can be created. It defines:

- **Machine:** 3-axis only for v1.
- **Stock:** Bounding box with configurable offsets around the CAD body.
- **WCS origin:** Work coordinate system placement.
- **Safety plane:** Z height for rapid moves between operations.

Setup is stored in the document alongside the tool library and CAM operation
tree. It cannot be deleted while child operations exist. It does not produce
toolpaths — it provides the coordinate frame and stock bounds that every
other operation references.

### 1. Face Milling (First — Easiest)

**What it does:** Select a planar horizontal face. Generate a toolpath that
cleans the top surface at a fixed Z depth. Single rectangular pattern at the
face boundary.

**Scope for v1:**
- Single planar face input (horizontal only, within ~5°)
- Toolpath fills the face bounding rectangle at fixed Z
- Zigzag pattern at configurable angle
- Single depth pass
- Stepover from tool diameter
- No stock-aware boundary — mills the entire face extent

**Why first:** Face milling is the simplest toolpath to generate — a
rectangle at constant Z. It exercises the full pipeline (geometry reference
→ parameter input → toolpath generation → viewport display) with trivial
math. No offset curves, no path planning, no multi-pass.

### 2. 2D Pocket (Second — Teaches Offset Patterns)

**What it does:** Select a face with optional islands. Clear material inside
the boundary using an offset fill pattern at constant Z.

**Scope for v1:**
- Single face, counter-clockwise outer boundary
- Optional islands (clockwise inner boundaries)
- Parallel line toolpath (zigzag at configurable angle)
- Stepover from tool diameter
- Single depth pass
- No adaptive clearing, no trochoidal paths, no rest machining

**Why second:** Pocketing with islands is the first operation that requires
real path planning. OCCT's 2D offset and boolean operations handle boundary
offsetting, but the fill pattern is custom code. This is where the TNP
"ambiguous resolution" problem first shows up — islands are faces, and
faces can change IDs.

### 3. 2D Contour (Third — Profile Finishing)

**What it does:** Select a closed wire on a planar face. Generate toolpath
that follows the contour, offset by tool radius.

**Scope for v1:**
- Single closed wire input
- Inside / outside / on-line offset modes
- Single pass at fixed depth (no multi-pass)
- No lead-in/lead-out
- No tabs
- No collision detection

**Why third:** Contour is offset-curve math, well understood via OCCT's
`BRepOffsetAPI_MakeOffset`. It's simpler than pocketing (no fill pattern)
but more complex than face milling (offset direction matters). By this
point the tool library, stock, and viewport toolpath rendering are all in
place from the first two operations.

### 4. Drilling (Fourth — Point-Based, Forces Tool Table)

**What it does:** Select points on a planar face. Generate G81/G83 cycles.

**Scope for v1:**
- Point selection (sketch points, circle centers, or free picks)
- G81 (simple drill) and G83 (peck drill) cycles
- Depth, peck depth, retract height parameters
- No spot drilling, no chip breaking beyond G83

**Why fourth:** Drilling is computationally trivial (single points, no offset
curves, no path planning). It can be built at any point. Placing it fourth
ensures the tool library, TNP face reference resolution, and viewport
toolpath rendering are solid before adding it. The main work is the drill-
specific parameter form in the UI.

### 5. Adaptive Clearing (Fifth — Most Complex)

**What it does:** Select a face or closed region. Generate a dynamic roughing
toolpath with constant tool engagement — the tool spirals inward in a
trochoidal pattern, clearing bulk material efficiently.

**Scope for v1:**
- Single face or closed boundary input
- Constant-engagement spiral pattern
- Stepdown from tool parameters
- No rest machining (single tool, full clear)
- No collision detection

**Why last:** Adaptive clearing is the most algorithmically complex v1
operation. It requires dynamic offset curves, engagement-angle calculation,
and spiral path planning. Building it last means all the infrastructure
(viewport rendering, tool library, stock model, TNP resolution, preview
pipeline, post-processor) is already proven and the only new work is the
path-planning algorithm itself.

---

## Context-Aware Toolbox

PolySmith's CAD already follows **select → invoke → floating panel** as its
binding UX pattern. CAM applies the same pattern rather than relying on
generic dropdown menus as the primary navigation.

### How It Works

The CAM toolbox filters available operations based on the current selection:

| Selection | Available operations |
|---|---|
| Nothing selected | Setup only |
| Planar horizontal face | Face Milling, Pocket, Contour |
| Planar vertical face | Contour, Slot |
| Face with holes | Drill, Counterbore, Tap |
| Circular edge | Drill, Circular Pocket |
| Closed wire / sketch profile | Pocket, Contour |

The dropdown toolbox in the ribbon becomes a **fallback browser** — useful
for exploring what's available, but not the primary interaction path. The
primary path is: click a face → the floating context panel shows only the
operations that make sense for that geometry.

### Example Flow

```
User clicks top face of a pocket
  → Floating panel appears: "Pocket" | "Face Milling"
  → User clicks "Pocket"
  → Panel expands with: tool selector, depth, stepover, zigzag angle
  → Live wireframe preview updates as parameters change
  → Enter confirms, Escape cancels
```

This is the same pattern as CAD extrude/fillet. The difference is that CAM
previews are progressive (wireframe → low-res → full) rather than blocking.

---

## Operation Dependencies

CAD features form a linear history where each feature builds on the body
produced by the previous one. CAM operations have a different relationship:
they share a Setup, reference the same stock, and often depend on the
material state left by previous operations.

### Dependency Model

```typescript
interface CAMOperation {
    id: string;
    type: CAMOperationType;

    // Operations can depend on previous ones
    dependencies: {
        // Must come after roughing (adaptive clearing)
        requiresRoughingComplete: boolean;

        // Uses same coordinate system as setup
        wcsId: string;

        // Uses leftover stock from previous operations
        previousStock: string;  // ID of previous operation's result
    }
}
```

### Typical Milling Flow

```
Setup (defines stock, WCS, safety plane)
  ↓
Adaptive Clearing (removes bulk material)
  ↓
2D Pocket (clears pockets the adaptive couldn't reach)
  ↓
2D Contour (finishes walls to final dimension)
  ↓
Drilling (holes — uses finished top surface as reference)
  ↓
Face Milling (optional top surface cleanup)
```

### Rules

- **Setup is always first.** It cannot be deleted if child operations exist.
- **Order matters.** Operations execute top-to-bottom in the CAM tree. The
  stock model updates after each operation — the next operation sees the
  material left by the previous one.
- **Dependency tracking uses feature IDs** (the same pattern as CAD
  `dependency_broken`). If an upstream operation's geometry reference breaks,
  downstream operations that depend on its stock state also flag a warning.
- **Reorder by drag-and-drop** in the CAM operations panel (v2). v1 uses
  a fixed top-to-bottom list.

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

// ── Setup (special, not a regular operation) ─────────────────────

struct CamSetup {
    std::string id;
    CamStockDefinition stock;
    // WCS origin in world coordinates.
    double wcsOriginX = 0.0;
    double wcsOriginY = 0.0;
    double wcsOriginZ = 0.0;
    double safetyPlaneZ = 10.0;
    // 3-axis only for v1.
    int axisCount = 3;
};

// ── Operation types (v1 subset) ──────────────────────────────────

enum class CamOperationType {
    Setup,             // special — document config, not a toolpath producer
    FaceMilling,
    Pocket,
    Contour,
    Drill,
    AdaptiveClearing
};

// ── Geometry reference (TNP-resilient) ───────────────────────────

struct CamGeometryReference {
    std::string featureId;                // owning CAD feature
    int semanticIndex = 0;                // position in feature's output
    std::vector<std::array<double, 3>> samplePoints;  // witness data
    double capturedArea = 0.0;
    std::array<double, 3> capturedNormal = {0, 0, 0};
};

// ── Operation parameters (per-type) ──────────────────────────────

struct FaceMillingParameters {
    double depth = 0.5;
    double stepover = 2.0;    // overrides tool default if set
    double angleDeg = 0.0;    // zigzag angle
};

struct PocketParameters {
    double depth = 1.0;
    double stepover = 2.0;    // overrides tool default if set
    double angleDeg = 0.0;    // zigzag angle
};

struct ContourParameters {
    enum class Side { Inside, Outside, OnLine } side = Side::Outside;
    double depth = 1.0;
    double extraStock = 0.0;  // finishing allowance
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
    std::optional<FaceMillingParameters> faceMilling;
    std::optional<PocketParameters> pocket;
    std::optional<ContourParameters> contour;
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
- **Setup is a separate struct (`CamSetup`), not a regular `CamOperation`.**
  It defines the coordinate frame and stock; it does not produce toolpaths.
  The `CamOperationType::Setup` enum entry exists for UI grouping but the
  runtime treats Setup as document-level config.

---

## Implementation Order

Building operations in this order ensures each step proves infrastructure
before the next operation depends on it:

1. **Setup** — no geometry generation, just data. Implement the stock
   definition, WCS origin, and safety plane in the document model. Render
   the stock as a translucent bounding box in the viewport. This proves
   the CAM data can round-trip through the document and IPC.

2. **Face Milling** — easiest toolpath (rectangle at fixed Z). Proves the
   entire pipeline end-to-end: select face → TNP-resolved reference →
   toolpath generation → viewport display → post-process → G-code.

3. **2D Pocket** — first operation requiring real path planning (fill
   pattern with island avoidance). Builds on Face Milling's pipeline.
   Teaches offset pattern generation.

4. **Drilling** — point-based, computationally trivial. Forces the tool
   library to be complete. Proves that different operation types share
   the same infrastructure (viewport rendering, post-processing).

5. **Adaptive Clearing** — most complex algorithmically (dynamic offset,
   constant engagement, spiral planning). Built last when all
   infrastructure is proven and only the path-planning algorithm is new.

---

## Implementation Progress

| Step | Status |
|---|---|
| 1. TNP Witness Resolution | ✅ Done — `cam_operation.h/.cpp`, test passes |
| 2. CAM Panel UI skeleton | ✅ Done — sub-category tabs, per-category toolbars, operations panel |
| 3. Toolpath visualization in viewport | 🔲 Next |
| 4. Setup + Stock model (document data + viewport rendering) | 🔲 After |
| 5. Face Milling toolpath generation | 🔲 After |
| 6. Post-processor skeleton | 🔲 After |
| 7. 2D Pocket toolpath generation | 🔲 |
| 8. Drilling toolpath generation | 🔲 |
| 9. Adaptive Clearing toolpath generation | 🔲 |

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
- **4/5-axis.** 2.5D only. Everything is planar.
- **Simulation.** Visual preview only, no material removal simulation.
- **Tool wear compensation.** Not needed for hobbyist use.
- **Binary IPC transport.** Chunked JSON is sufficient through v1.
- **Turning, Cutting, Printing operations.** Milling only for v1.
  Scaffolding (disabled toolbar buttons) is present but no generation
  code will be written.

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
