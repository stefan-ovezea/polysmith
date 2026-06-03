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

The C++ snap engine in `snap_engine.cpp` and its IPC commands (`resolve_draft_snap`, `drag_snap_result`) violate this principle — they send per-frame mouse snap candidates from core to UI. These are transient interaction data with no persistent ID, changing every frame.

**Direction:** Snap computation and resolution must move back to the TypeScript UI layer. The core provides the static geometry (document_state / viewport_state) once per operation — the UI computes snap targets locally from that geometry.

## What This Means for Drag

Endpoint drag currently sends `drag_sketch_point` per-frame to the core, which applies core-side snap and returns a `drag_snap_result`. This violates the principle: drag deltas are transient, and snap candidates are interaction data.

**Direction:** Drag must be handled entirely in the UI until mouse-up. The UI resolves snap locally, shows a preview, and sends a single `update_sketch_point` (or equivalent) on mouse-up with the final snapped position.

## Mantra

> **The core owns the document. The UI owns the interaction. Never confuse them.**
> If it moves with the mouse, it's UI. If it saves to a file, it's core.
