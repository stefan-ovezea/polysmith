# 2D Sketch System Architecture

> **Status:** The unified selection filter, constraint inference engine, DOF
> counting, and snap gating described below are all shipped (2026-05-23/24).
> This document describes the as-built architecture. Remaining items from the
> original plan: perpendicular snap (general case), driven dimension proposal,
> and DOF colour legend.

## Overview

This document defines the core interaction systems for the 2D sketch
environment as they are currently implemented: **Constraints**, **Snapping**,
and **Selection**. These three systems are unified under a single
**Selection Filter** model — snapping and constraining are consequences of
selectability.

---

## Part 1: Constraints System

### Geometric Constraints

| Constraint | Function | Auto-Inference (Creation) |
| :--- | :--- | :---: |
| **Coincident** | Joins two points or places a point on an endpoint | ✅ Yes |
| **Point on Object** | Fixes a point onto a line, arc, or circle (non-endpoint) | ✅ Yes |
| **Horizontal** | Makes a line or two points parallel to X-axis | ✅ Yes |
| **Vertical** | Makes a line or two points parallel to Y-axis | ✅ Yes |
| **Parallel** | Makes two or more lines parallel | ❌ Rarely |
| **Perpendicular** | Creates a 90° angle between lines (or line & curve) | ❌ Rarely |
| **Tangent** | Smooth contact between line & curve or two curves | ✅ Yes |
| **Equal** | Equal lengths for lines, equal radii for arcs/circles | ❌ Rarely |
| **Symmetric** | Mirrors entities across a centerline | ❌ Rarely |
| **Concentric** | Same center point for circles, arcs, ellipses | ✅ Yes |

### Dimensional Constraints

| Constraint | Function | Auto-Inference |
| :--- | :--- | :---: |
| **Horizontal Distance** | Horizontal distance between two points | ✅ (AI/Auto-solve) |
| **Vertical Distance** | Vertical distance between two points | ✅ (AI/Auto-solve) |
| **Aligned Distance** | True straight-line distance or line length | ✅ (AI/Auto-solve) |
| **Radius / Diameter** | Size of an arc or circle | ✅ (AI/Auto-solve) |
| **Angle** | Internal angle between two non-parallel lines | ✅ (AI/Auto-solve) |

### Automation Strategies (Three Phases)

#### Phase 1: Inference Constraints (Apply at creation time)
- Horizontal / Vertical (when drawn near axis-aligned)
- Coincident (when snapping endpoints)
- Point on Object (when snapping midpoints)
- Tangent (when placing line near tangent snap)
- Concentric (when snapping to center point)

#### Phase 2: Manual Constraints (User applies explicitly)
- Parallel, Perpendicular, Equal, Symmetric
- All dimensional constraints (Distance, Radius, Angle)

#### Phase 3: Advanced - AI Auto-Solve (Post-creation)
- System analyzes rough sketch shape
- Applies combination of Horizontal, Vertical, Parallel, Equal, and Dimensional constraints
- Goal: achieve fully constrained sketch with minimal user input

---

## Part 2: Snapping System

### How Snapping Works

Snapping is **not an independent system**. A point becomes "snappable" only if:
1. The parent entity type is enabled in the Selection Filter
2. The specific snap type (Endpoint, Midpoint, Center) is enabled

### 2D Sketch Snaps

| Snap Type | Condition / Trigger | Default | User Configurable |
| :--- | :--- | :---: | :---: |
| **Grid Snap** | Cursor within tolerance distance of grid intersection | ✅ On | ✅ Yes |
| **Grid Line Snap** | Cursor anywhere on a major grid line | ⬜ Off | ✅ Yes |
| **Endpoint Snap** | Cursor near endpoint of any line, arc, or curve | ✅ On | ✅ Yes |
| **Midpoint Snap** | Cursor near exact middle of line or arc chord | ✅ On | ✅ Yes |
| **Center Snap** | Cursor near center point of circle, arc, or ellipse | ✅ On | ✅ Yes |
| **Quadrant Snap** | Cursor near 0°, 90°, 180°, 270° points on circle/arc | ⬜ Off | ✅ Yes |
| **Intersection Snap** | Cursor where two or more entities cross | ✅ On | ✅ Yes |
| **Nearest Snap** | Cursor anywhere on line/curve (not just key points) | ✅ On | ✅ Yes |
| **Perpendicular Snap** | Cursor indicates 90° offset from existing line | ⬜ Off | ✅ Yes |
| **Parallel Snap** | Cursor indicates parallel alignment to nearest line | ⬜ Off | ✅ Yes |
| **Tangent Snap** | Cursor indicates tangent point to arc/circle from line endpoint | ✅ On | ✅ Yes |
| **Polar Snap** | Angle increments (15°, 30°, 45°) at specified distances | ⬜ Off | ✅ Yes |
| **Object Snap Override** | Temporary snap by pressing modifier key | ✅ On | ❌ No (hardcoded) |

### 3D Snaps (Future Extension)

| Snap Type | Condition / Trigger | Default |
| :--- | :--- | :---: |
| **Vertex Snap** | Cursor near any 3D corner point | ✅ On |
| **Edge Snap** | Cursor anywhere on edge of face | ✅ On |
| **Face Snap (Planar)** | Cursor on flat face, aligns sketch plane to that face | ✅ On |
| **Face Snap (Offset)** | Cursor on face with specified offset distance | ⬜ Off |
| **Midpoint (3D Edge)** | Midpoint of any 3D edge | ⬜ Off |
| **Center of Mass Snap** | Snaps to computed centroid of selected solid/body | ⬜ Off |
| **Coordinate System Snap** | Snaps to origin, axes (X, Y, Z lines) | ✅ On |
| **Infinite Line Snap** | Snaps along infinite extension of 3D edges | ⬜ Off |
| **Tangent (3D Edge)** | Tangent point on curved 3D edge | ⬜ Off |

---

## Part 3: Unified Selection Filter (Recommended Architecture)

### The Core Principle

Industry-standard CAD systems do **not** maintain three separate systems (Constraints, Snaps, Selection). Instead, they operate on a unified filtering model where a single "Select" panel controls what your cursor can see, grab, or snap to.

**If a geometric element type is not "selectable," it logically cannot be snapped to or constrained.**

### How Three Actions Share One Filter

| User Action | How it uses the Filter |
| :--- | :--- |
| **Selection (Click/Drag)** | Only entity types with ☑ checked can be highlighted or chosen |
| **Snapping (During drawing)** | Only key points belonging to ☑ entity types are considered for snap targets |
| **Constraining (Applying rules)** | The constraint tool can only be applied to entity types that are ☑ selectable |

### The Unified Filter Panel

This single panel replaces separate "Snap Settings" and "Constraint Settings" pages.

```markdown
# Selection & Interaction Filter

## Sketch Geometry
☑ Select Sketch Curves          → Enables: Snap to lines/arcs, Constrain length/radius
☑ Select Sketch Points          → Enables: Snap to endpoints/midpoints, Coincident constraints
☑ Select Construction Geometry  → Enables: Snap to construction lines, Constrain symmetry
☑ Select Sketch Constraints     → Enables: Click to edit constraint properties

## Reference Geometry (Controls external snapping)
☐ Auto-project model edges      → When ON: 3D edges become selectable/snappable in sketch
☐ Auto-project work geometry    → When ON: Planes/Axes become selectable/snappable
☐ Auto-project origin           → When ON: Origin point becomes selectable/snappable

## 3D Modeling (for future extension)
☑ Select Bodies
☑ Select Faces
☑ Select Edges
☑ Select Vertices
☑ Select Components