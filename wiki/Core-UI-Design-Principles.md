# Core → UI Design Principles

## The Golden Rule

**Core sends back DOCUMENT STATE, not INTERACTION STATE.**

| Core Sends | Core Does NOT Send |
|---|---|
| Final geometry after operation | Incremental preview during drag |
| Constraint-solved positions | Mouse snap candidates |
| Feature tree after recompute | Live camera transforms |
| Selection state (what's selected) | Hover highlights (temporary) |
| Error/warning messages | Frame-by-frame animations |
| Toolpath after generation | Toolpath generation progress |

## The Thumb Rule

**Core sends things that have IDs and persist across saves.**

- ✅ Face with persistent_id
- ✅ Feature with history
- ✅ Constraint with UUID
- ✅ Toolpath with operation_id

**Core does NOT send transient interaction data.**

- ❌ Mouse position
- ❌ Hover highlight
- ❌ Drag delta / drag preview
- ❌ Snap candidates
- ❌ Animation progress

## The Test

Before adding any new core → UI message, ask:

1. **Does this data have a permanent ID?**
2. **Is this data needed after restart?**
3. **Is this data changing every frame?**
4. **Is this data the result of a completed operation?**

If the answer to any of the first three is NO, or the answer to the fourth is also NO — the data belongs in the UI, not the core.

## The Communication Pattern

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

## What This Means for Snap

> ✅ **Done (2026-06-03).** The C++ `resolve_draft_snap` and `drag_snap_result` IPC paths have been removed. `snap_engine.cpp` / `snap_engine.h` have been deleted. The `SnapCandidate` struct now lives in `viewport.h`. Static snap targets (endpoint, midpoint, center, quadrant, intersection) are emitted in `viewport_state.snap_candidates` and resolved locally by the TS `resolveSnappedSketchPoint`. Dynamic snaps (axis_lock, parallel, tangent, etc.) are a TS-side follow-up.

## What This Means for Drag

> ✅ **Done (2026-06-03).** The `drag_sketch_point` IPC path has been removed. Endpoint drag is handled entirely in the TypeScript UI layer: local snap resolution via `resolveSnappedSketchPoint`, dashed-line overlay preview during drag, and a single `update_sketch_point` commit on mouse-up.

## Mantra

> **The core owns the document. The UI owns the interaction. Never confuse them.**
> If it moves with the mouse, it's UI. If it saves to a file, it's core.
