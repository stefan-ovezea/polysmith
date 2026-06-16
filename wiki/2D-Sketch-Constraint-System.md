# 2D Sketch Constraint System - Implementation TODO

## 1. Geometric Constraints

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

## 2. Dimensional Constraints

| Constraint | Function | Auto-Inference |
| :--- | :--- | :---: |
| **Horizontal Distance** | Horizontal distance between two points | ✅ (AI/Auto-solve) |
| **Vertical Distance** | Vertical distance between two points | ✅ (AI/Auto-solve) |
| **Aligned Distance** | True straight-line distance or line length | ✅ (AI/Auto-solve) |
| **Radius / Diameter** | Size of an arc or circle | ✅ (AI/Auto-solve) |
| **Angle** | Internal angle between two non-parallel lines | ✅ (AI/Auto-solve) |

## 3. Automation Strategies

### Phase 1: Inference Constraints (Apply at creation time)
- **Horizontal** / **Vertical** (when drawn near axis-aligned)
- **Coincident** (when snapping endpoints)
- **Point on Object** (when snapping midpoints)
- **Tangent** (when placing line near tangent snap)
- **Concentric** (when snapping to center point)

### Phase 2: Manual Constraints (User applies explicitly)
- Parallel, Perpendicular, Equal, Symmetric
- All dimensional constraints (Distance, Radius, Angle)

### Phase 3: Advanced - AI Auto-Solve (Post-creation)
- System analyzes rough sketch shape
- Applies combination of Horizontal, Vertical, Parallel, Equal, and Dimensional constraints
- Goal: achieve fully constrained sketch with minimal user input

## 4. Technical Prerequisites

- [ ] **Constraint Solver Engine** (mathematical solver that moves geometry to satisfy rules)
- [ ] Inference detection system (snapping & prediction)
- [ ] UI feedback (constraint icons, color coding)
- [ ] Over-constraint detection & resolution
- [ ] Constraint deletion/editing interface

## 5. Implementation Order Suggestion

1. Solver engine core
2. Coincident + Point on Object (foundation constraints)
3. Horizontal + Vertical (basic orientation)
4. Distance constraints (aligned, horizontal, vertical)
5. Perpendicular + Parallel
6. Tangent + Concentric
7. Equal + Symmetric
8. Radius + Diameter + Angle
9. AI auto-solve (stretch goal)