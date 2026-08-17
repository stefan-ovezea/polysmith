# Right-Click

Right-clicking in the viewport has two distinct behaviours depending on
whether a sketch draft tool is currently active.

---

## Context: Sketch Draft Active (Line, Rectangle, Circle, Arc, Polygon)

**Trigger:** Right-click while a rubber-band preview is visible (i.e. a
draft dimension session is active).

| Action | Behavior |
|---|---|
| **Cancel Draft** | Cancels the current rubber band / breaks the polyline chain. The tool stays **armed** — the next click starts a fresh independent entity from a new start point. Equivalent to Escape but without dearming the tool. |

**Comparison with other commit/cancel actions:**

| Input | Result |
|---|---|
| **Right-click** | Cancel rubber band, keep tool armed |
| **Escape** | Cancel rubber band, dearm tool → Select mode |
| **Enter** | Commit draft at current position, keep tool armed |

**Implementation:** The `onContextMenu` handler in ViewportPanel checks
`draftDimensionSessionRef.current`. When non-null, it resets all draft state
(`lineDraftStartRef`, previews, draft session, snap/constraint state,
hovered entities) but does **not** call `setSketchToolRef("select")`.

**Files:**
- TS: `ViewportPanel.tsx` → `onContextMenu` handler (lines ~3171–3195)

---

## Context: Context Menu (no draft active)

Right-clicking different entities in the viewport when no draft is active
opens a context-sensitive menu. This document tracks all context menu
functions for reference during development and testing.

---

### Context: Dimension Label

**Trigger:** Right-click a dimension label or dimension geometry in the
active sketch.

| Action | Applies to | Behavior |
|---|---|---|
| **Show Radius / Show Diameter** | `circle_radius` dimensions only | Toggles the viewport label between `R 10.00 mm` (radius) and `⌀ 20.00 mm` (diameter). The stored value is always radius. C++: `update_sketch_dimension_display` → sets `display_as` to `"radius"` or `""`. |
| **Toggle Driving** | All dimension kinds | Toggles between driving (constrains solver) and driven / reference-only (displayed in parentheses, does not constrain). C++: `toggle_sketch_dimension_driven` → flips `driven` flag. |
| **Delete** | All dimension kinds | Removes the dimension. C++: `delete_sketch_dimension`. If the dimension was driving, geometry is freed from that constraint. |

**Implementation files:**
- TS: `ViewportContextMenu.tsx`, `viewportContextMenuActions.ts`
- C++: `sketch_entity_dimension_commands.inc`, `sketch_dimension_toggle_driven_command.inc`

---

### Context: Sketch Line

**Trigger:** Right-click a sketch line entity in the active sketch.

| Action | Behavior |
|---|---|
| **Toggle Construction** | Flips the line's `is_construction` flag. Construction lines render dashed, are excluded from profile detection, but keep all dimensions and constraints as-is (Fusion 360 behaviour). Driving/driven status is preserved. |
| **Delete** | Deletes the line and any selected sketch entities. |

**Implementation notes:**
- Construction toggle reads `is_construction` from the document state (not stale refs).
- C++ `set_sketch_line_construction` (July 2026): simplified to pure flag flip — no auto-dimension create/delete on toggle.
- As of July 2026, construction lines, circles, and polygons no longer block driving dimension creation.

**Implementation files:**
- TS: `ViewportContextMenu.tsx`, `viewportContextMenuActions.ts`, `contextMenuState.ts`
- C++: `line_entity_commands.inc`

---

### Context: Constraint Icon

**Trigger:** Right-click an H/V constraint badge or relation icon in the
active sketch.

| Action | Behavior |
|---|---|
| **Delete Constraint** | Removes the constraint. For H/V constraints: clears `line.constraint` and resets `constraint_driven`. For relations: removes the `SketchLineRelation` entry. |

---

### Context: Sketch Selection (multi-select delete)

**Trigger:** Right-click when one or more sketch entities are selected
(lines, points, or profiles), or right-click a single entity that is part
of a multi-selection.

| Action | Behavior |
|---|---|
| **Delete** | Deletes all currently selected sketch entities. |

---

### Context: Body (3D solid)

**Trigger:** Right-click a 3D body in the viewport (outside sketch mode).

| Action | Behavior |
|---|---|
| **Move** | Activates the move gizmo for repositioning the body. |
| **Copy → Linked** | Creates a linked body copy. Changes to the source propagate to the copy. |
| **Copy → Independent** | Creates a standalone body copy with no link to the source. |
| **Unlink** | Breaks the link on a linked body copy (only shown for linked copies). |
| **Export as Mesh** | Opens the STL/STEP export dialog for this body. |

---

### Context: Face / Reference Plane

**Trigger:** Right-click a solid face or reference plane.

| Action | Behavior |
|---|---|
| **Create Sketch** | Creates a new sketch on the selected face or reference plane. If a face is clicked, the sketch plane is aligned to that face. |

---

### State Tracking

The context menu state is built in `contextMenuState.ts` → `buildViewportContextMenuState()`.
It receives the hit-test result and document state, and returns a
`ViewportContextMenuState` that determines which buttons appear.

Key fields on `ViewportContextMenuState`:

| Field | Set when | Used for |
|---|---|---|
| `dimensionId` | Right-click a dimension | Toggle Driving, Show Radius/Diameter, Delete |
| `lineId` | Right-click a single line | Toggle Construction |
| `constraintKind` / `constraintId` | Right-click a constraint | Delete Constraint |
| `sketchDeleteSelection` | Right-click sketch entity/selection | Delete |
| `bodyId` | Right-click a body | Move, Copy, Unlink, Export |
| `referenceId` / `faceId` | Right-click ref plane or face | Create Sketch |
