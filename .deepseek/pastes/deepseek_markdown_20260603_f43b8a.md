# Core → UI Interface Specification

## The Golden Rule

**Core sends back DOCUMENT STATE, not INTERACTION STATE.**

| Core Sends | Core Does NOT Send |
|------------|-------------------|
| Final geometry after operation | Incremental preview during drag |
| Constraint-solved positions | Mouse snap candidates |
| Feature tree after recompute | Live camera transforms |
| Selection state (what's selected) | Hover highlights (temporary) |
| Error/warning messages | Frame-by-frame animations |
| Toolpath after generation | Toolpath generation progress |

## Core → UI Message Types

### 1. Command Response

```json
{
  "id": 42,
  "result": {
    "status": "success" | "error" | "warning",
    "data": {},
    "error": {},
    "warnings": []
  }
}
```

### 2. Document State

```json
{
  "type": "document_update",
  "data": {
    "feature_tree": {
      "features": [
        {"id": "sketch_1", "type": "sketch", "name": "Base Sketch"},
        {"id": "extrude_1", "type": "extrude", "name": "Extrude 1"}
      ]
    },
    "current_body": {
      "bounds": {"min_x": 0, "min_y": 0, "min_z": 0, "max_x": 100, "max_y": 50, "max_z": 20},
      "face_count": 42,
      "edge_count": 128
    },
    "selection": {
      "selected_entities": [
        {"id": "face_789", "type": "face", "bounds": {}}
      ]
    }
  }
}
```

### 3. Geometry Snapshot

```json
{
  "type": "geometry_update",
  "data": {
    "revision": 7,
    "meshes": [
      {
        "id": "body_1",
        "type": "solid",
        "vertices": [[0,0,0], [10,0,0], [10,10,0]],
        "indices": [[0,1,2], [0,2,3]],
        "normals": [[0,0,1]],
        "wireframe_edges": [[0,1], [1,2]]
      }
    ],
    "sketches": [
      {
        "id": "sketch_1",
        "plane": {"origin": [0,0,0], "normal": [0,0,1]},
        "curves": [
          {"type": "line", "start": [0,0], "end": [10,0]},
          {"type": "arc", "center": [5,5], "radius": 5, "start_angle": 0, "end_angle": 90}
        ]
      }
    ]
  }
}
```

### 4. Operation Preview

```json
{
  "type": "operation_preview",
  "data": {
    "operation_type": "extrude",
    "preview_mesh": {
      "vertices": [],
      "indices": []
    },
    "affected_features": ["sketch_1"],
    "warnings": ["Extrude will create non-manifold geometry"],
    "can_confirm": true
  }
}
```

### 5. Toolpath Data (CAM)

```json
{
  "type": "toolpath_ready",
  "data": {
    "operation_id": "cam_op_456",
    "toolpath_id": "tp_789",
    "metadata": {
      "total_length_mm": 1234.5,
      "estimated_time_seconds": 45.2,
      "num_moves": 12500
    },
    "moves_inline": [],
    "binary_channel": "toolpath_tp_789"
  }
}
```

### 6. Selection Validation

```json
{
  "type": "selection_validation",
  "data": {
    "ui_hover_id": "temp_face_123",
    "valid": true,
    "stable_reference": {
      "persistent_id": "face_xyz_789",
      "geometric_attestation": {}
    }
  }
}
```

### 7. Error & Warning

```json
{
  "type": "error",
  "data": {
    "severity": "warning" | "error" | "fatal",
    "code": "TNP_RESOLUTION_FAILED",
    "message": "Could not find referenced face after recompute",
    "affected_features": ["fillet_3", "chamfer_4"],
    "suggested_actions": ["Re-select face for fillet_3", "Delete and recreate"]
  }
}
```

## What Core Does NOT Send

### ❌ No Mouse Snap Candidates
```json
// NEVER SEND
{"type": "snap_candidates", "data": {"points": [[10,20], [30,40]]}}
```

### ❌ No Incremental Drag Updates
```json
// NEVER SEND
{"type": "line_drag_preview", "data": {"line_id": "L1", "new_position": [15, 30]}}
```

### ❌ No Real-time Camera Data
```json
// NEVER SEND
{"type": "view_matrix", "data": [[1,0,0,0], [0,1,0,0]]}
```

### ❌ No Animation Frames
```json
// NEVER SEND
{"type": "animation_frame", "frame": 42, "mesh": {}}
```

### ❌ No Temporary Hover State
```json
// NEVER SEND
{"type": "hover_highlight", "entity_id": "face_temp_999"}
```

## The Complete Communication Pattern

```
UI                                      Core
 │                                        │
 │ 1. USER ACTION (mouse click on button) │
 ├───────────────────────────────────────>│
 │   {"method": "extrude", ...}           │
 │                                        │
 │                                        │ 2. Core computes
 │                                        │
 │ 3. COMPLETE RESULT                     │
 │<───────────────────────────────────────┤
 │   {"type": "document_update", ...}     │
 │   {"type": "geometry_update", ...}     │
 │                                        │
 │ (NO COMMUNICATION DURING MOUSE DRAG)   │
 │                                        │
 │ 5. MOUSE UP (final position)           │
 ├───────────────────────────────────────>│
 │   {"method": "move_line", ...}         │
 │                                        │
 │ 6. FINAL RESULT                        │
 │<───────────────────────────────────────┤
```

## The Thumb Rule

**Core sends things that have IDs and persist across saves.**

- ✅ Face with persistent_id
- ✅ Feature with history
- ✅ Constraint with UUID
- ✅ Toolpath with operation_id

**Core does NOT send transient interaction data.**

- ❌ Mouse position
- ❌ Hover highlight
- ❌ Drag delta
- ❌ Animation progress

## The Test

Before adding any new core → UI message, ask:

1. **Does this data have a permanent ID?**
2. **Is this data needed after restart?**
3. **Is this data changing every frame?**
4. **Is this data the result of a completed operation?**

If you answer NO to question 4, the message doesn't belong.