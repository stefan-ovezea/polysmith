# planegcs Solver — Manual Testing Checklist

**Date:** 2026-06-05
**How to test:** Start the app with `pnpm dev`, enter a sketch on any plane, draw entities, and verify behavior.
**Solver output:** Watch stderr for `constraint solver: status=...` lines — a status of 0 (Success) or 1 (Converged) means the solver ran and applied results.

---

## 1. No Regression — Basic Drawing Still Works

Draw entities WITHOUT constraints and confirm they behave normally.

- [ ] **1.1** Draw a single line by click-drag. Line appears, auto-dimensions show up.
- [ ] **1.2** Draw a rectangle. Four lines, closed profile detected (filled area visible).
- [ ] **1.3** Draw a circle. Circle appears with center point and radius.
- [ ] **1.4** Draw a polygon (N-gon). Closed profile detected.
- [ ] **1.5** Drag an endpoint of an unconstrained line. Line reshapes freely.
- [ ] **1.6** Extrude a closed profile. 3D body appears.
- [ ] **1.7** Save and reload a `.polysmith` document with sketch entities.

---

## 2. Inline Constraints (Horizontal / Vertical)

These are stored on `SketchLine.constraint` and applied at creation time.

- [ ] **2.1** Draw a nearly-horizontal line (angle < 1°). Should snap to horizontal. Angle dimension shows 0°.
- [ ] **2.2** Draw a nearly-vertical line (angle within 1° of 90°). Should snap to vertical.
- [ ] **2.3** Drag the unconstrained endpoint of a horizontal line. Line should stay horizontal (y locked).
- [ ] **2.4** Draw a diagonal line and toggle its constraint in the constraints panel (if UI supports this).

---

## 3. Coincident Constraints

Created by the inference engine when an endpoint lands near another point.

- [ ] **3.1** Draw two lines that share an endpoint (snap to existing endpoint). The shared point should move both lines together when dragged.
- [ ] **3.2** Draw a line from an existing line's endpoint. Both lines share the point — drag the shared point, both lines follow.
- [ ] **3.3** Draw a circle center snapped to a line endpoint. Drag the line endpoint — circle should move with it.
- [ ] **3.4** Draw three lines radiating from one point. Drag the shared point — all three follow.

---

## 4. Parallel / Perpendicular / Equal Length Relations

Applied via the constraints panel or toolbar.

- [ ] **4.1** Create two lines. Apply Parallel relation. Drag one — the other stays parallel.
- [ ] **4.2** Create two lines at ~90°. Apply Perpendicular. Drag one — they maintain 90°.
- [ ] **4.3** Create two lines of different lengths. Apply Equal Length. Drag one endpoint — both lines keep same length.
- [ ] **4.4** Chain: line A ∥ line B ∥ line C. Drag A — B and C stay parallel.

---

## 5. Midpoint and Point-Line Anchors

Created automatically during drawing when snapping to midpoints or line bodies.

- [ ] **5.1** Draw a line. Start a second line from the first line's midpoint (snap indicator shows). The new line's start should ride the midpoint when the first line is dragged.
- [ ] **5.2** Draw a line. Start a second line from somewhere along the first line's body (not endpoint/midpoint). Drag the first line — the anchored point should stay on the line.
- [ ] **5.3** Draw a rectangle. The four lines should stay connected at corners (coincident) and the midpoint anchors should keep cross-lines centered.

---

## 6. Concentric Circles

- [ ] **6.1** Draw two circles with the same center (snap to center point). Drag the shared center — both circles move.
- [ ] **6.2** Change the radius of one concentric circle. Centers should remain coincident.

---

## 7. Solver Diagnostics (watch stderr)

Run the app from a terminal so you can see stderr output.

- [ ] **7.1** Draw a line + H constraint. Check stderr: should see NO solver error (status=0 or 1).
- [ ] **7.2** Create an over-constrained sketch (e.g., fix both endpoints of a line AND add a length dimension). Check stderr: should see `conflicting > 0`.
- [ ] **7.3** Draw a simple unconstrained sketch (one line, no constraints). Check stderr: solver should NOT run (no output) since there are no constraints.

---

## 8. Drag with Constraints (Interactive Solver)

- [ ] **8.1** Draw a horizontal line. Drag the unconstrained endpoint — line stays horizontal, length changes.
- [ ] **8.2** Draw a rectangle. Drag one corner — all four corners move to maintain the rectangle shape.
- [ ] **8.3** Draw a line with both endpoints fixed (is_fixed=true). Try to drag an endpoint — should not move.

---

## 9. Undo / Redo

- [ ] **9.1** Add a constraint (H/V on a line). Undo — constraint removed, line returns to diagonal.
- [ ] **9.2** Redo — constraint re-applied.
- [ ] **9.3** Delete a constrained line. Undo — line and its constraint return.

---

## 10. Save / Load Round-Trip

- [ ] **10.1** Create a sketch with multiple constraints (H/V, coincident, parallel, equal length). Save as `.polysmith`. Close and re-open. All constraints still enforced.
- [ ] **10.2** After reload, drag an endpoint — constraints still hold.

---

## Reporting

For each test, note:
- ✅ Pass — works as expected
- ❌ Fail — describe what happened (crash, wrong geometry, no effect)
- ⚠️ Partial — works but with caveats

Report failures with:
1. Steps to reproduce
2. Expected vs actual behavior
3. Any stderr output

---

**Start with test 1.1 and go in order. Report after each section (1–4 tests at a time).**
