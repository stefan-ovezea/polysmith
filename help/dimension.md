# Dimension Tool

Creates and edits sketch dimensions — linear, radial, angular, and
point-to-point distance.

---

## Activation

- **Click** the Dimension button in the sketch toolbar (Sketch tab)
- **Hotkey:** `D` (configurable in settings)

---

## Dimension Tool Modes

The dimension tool has a dropdown (split-button) in the sketch toolbar with
the following modes:

| Mode | Behavior |
|---|---|
| **Auto** (default) | Smart detection — chooses dimension type based on entities and cursor position |
| **Angular** | Only creates angle dimensions (line–line or line–horizontal) |
| **Linear** | Only creates distance dimensions (skips angle detection) |
| Aligned / Radius / Diameter / Arc Length | Reserved for future use — fall through to auto behavior |

Auto mode is the recommended default. Use Angular or Linear mode when
auto-detection ghosts/flashes are distracting during repetitive work.

---

## Single-Entity Dimensions

| Click | Auto / Linear mode | Angular mode |
| --- | --- | --- |
| **Line** | Length dimension (drag-to-choose H/V/aligned) | Line angles are created with a follow-up click (see below) |
| **Circle** | Radius / diameter dimension | Same as auto |
| **Polygon** | Radius dimension | Same as auto |

**Line placement flow (auto/linear mode):** After clicking a line, drag the
cursor to choose the dimension orientation:
- **Drag perpendicular** to the line → aligned (line length)
- **Drag vertically** (up/down from midpoint) → horizontal distance ("x")
- **Drag horizontally** (left/right from midpoint) → vertical distance ("y")

A live preview updates as you move. Click to commit, Escape to cancel.

**Hovering a second line:** While in linear placement (first line staged),
hovering over a second line that shares an endpoint shows an **angle ghost**
preview. Click the second line to create the angle dimension instead of the
linear dimension. This also works for parallel-line distance and other
two-entity relations.

After creation, drag the label to position it. Click anywhere on the canvas
to commit the automatic value.

---

## Two-Entity Dimensions

Click a first entity — a single-entity dimension is created and the entity
is staged. Click a **second, different entity** to morph into:

| Entities | Result |
|---|---|
| Two lines | **Angle dimension** (unless parallel — then parallel distance) |
| Two parallel lines | Parallel distance |
| Endpoint → endpoint | Point-to-point distance |
| Endpoint → circle centre | Point-to-centre distance |

The single-entity dimension from the first pick is deleted automatically
when the two-entity dimension is created.

---

## Angle Dimensions

### Two-Line Angle (Quadrant Selection)

When creating an angle between two lines, the dimension tool shows different
angles depending on which **quadrant** the cursor is in relative to the
two lines. Two lines × two direction choices each (forward / reverse) =
4 possible pairings = 4 angular sectors, each with a different angle:

| Quadrant | Angle shown |
|---|---|
| **Between the lines** (acute wedge) | Internal angle (≤ 90°) |
| **Outside one line** (extension side) | Supplement (180° − acute) |
| **Outside the other line** | Supplement (mirror) |
| **Opposite both lines** | Reflex (360° − acute) |

Move the cursor around the lines' shared endpoint to preview each quadrant.
Click to commit the dimension with the selected angle value.

For **crossing lines** (no shared endpoint), the same 4-quadrant model
applies — the pivot is the infinite-line intersection (virtual pivot).

### Single-Line Angle from Horizontal

In **Angular** mode only: click a line (it stages), then:

- Click **another line** — creates a two-line angle (see above)
- Click **empty canvas** (or re-click the same line) — creates a
  **line-to-horizontal** angle dimension (arc from +X axis to the line)

This is useful for adding an angle constraint to a line after it was
created without one. The dimension uses the `line_angle` kind,
constrained by the solver as a `p2p_angle` (line direction vs. +X axis).

### Reflex / External Angles

For lines sharing an endpoint, the reflex angle (> 180°) can be selected
by placing the cursor in the quadrant **opposite both lines**. The
dimension arc sweeps the long way around (CW instead of CCW) and the
label sits on the external side of the wedge.

---

## Placement & Commit

1. **Drag** the label to position it.
2. **Click** anywhere on the canvas — commits the automatic value and
   closes the editor.
3. **Type** a value and press **Enter** — commits the typed value.
4. **Escape** during placement — deletes the dimension entirely (cancel
   creation).

---

## Editing

- **Double-click** a dimension label to re-open the editor.
- Type a new value or expression, then **Enter** to commit.
- **Escape** restores the previous value and closes the editor.

---

## Expressions

Type a parameter name (e.g. `width`) or formula (`width * 2`) instead of
a raw number.

- **ArrowUp / ArrowDown** — navigate parameter suggestions.
- **Enter / Tab** — insert the selected suggestion.
- Expressions are stored on the dimension and re-evaluated when
  parameters change.

Angles are unitless (degrees for display, radians in the core). All other
dimensions use the document's display unit (mm or inch).

---

## Dimension Kinds (Reference)

| Kind | Entities | Solver constraint |
| --- | --- | --- |
| `line_length` | 1 line | `p2p_distance` |
| `line_angle` | 1 line | `p2p_angle` (vs. +X axis) |
| `angle` | 2 lines | `l2l_angle_ll` |
| `circle_radius` | 1 circle | Radius parameter |
| `polygon_radius` | 1 polygon | Radius parameter |
| `line_line_distance` | 2 parallel lines | Parallel distance |
| `circle_center_distance` | 2 circles | Centre distance |
| `circle_line_distance` | 1 circle + 1 line | Centre-to-line distance |
| `point_distance` | 2 points | Point-to-point distance |

Auto dimensions (`is_auto = true`, empty expression) are skipped by the
solver — they display the measured value but do not drive geometry.

---

## Circle Radius / Diameter Toggle

Right-click a circle dimension label and choose **Show Radius** or
**Show Diameter** to toggle the display mode. The viewport label changes:
- **Radius:** `R 10.00 mm` (stored radius value)
- **Diameter:** `⌀ 20.00 mm` (radius × 2)

The underlying value is always stored as radius in the data model.

---

## Toggle Driving / Driven

Right-click any dimension label and choose **Toggle Driving** to switch
between driving and driven (reference-only):

- **Driving** — constrains the solver. Editing the value drives geometry.
- **Driven (reference)** — displayed in parentheses `(25 mm)`. Shows the
  measured value but does not constrain geometry. Automatically updates to
  reflect current geometry.

When a new dimension would over-constrain the system, it is automatically
marked as driven on creation.

---

## Right-Click Context Menu

Right-clicking different entities in the viewport shows context-specific
actions:

| Right-click target | Menu items |
|---|---|
| **Dimension label** | Show Radius/Diameter (circle only), Toggle Driving, Delete |
| **Line** | Toggle Construction, Delete |
| **Constraint icon** | Delete Constraint |
| **Body** | Move, Copy (linked/standalone), Unlink, Export Mesh |
| **Face / Reference plane** | Create Sketch |

See `help/right-click.md` for the full reference.

---

## Known Issues

- **Angle dimension label drag:** The label does not perfectly follow the
  mouse cursor direction — it moves in a slightly different direction than
  expected, reaching a limit and then going backwards. Workaround: drag
  radially outward from the corner, then sideways.
- **Supplement angles for shared-endpoint lines:** The quadrant selection
  correctly shows all 4 angles during preview, but the committed dimension
  stores the value as a display-only measurement (auto dimension). Editing
  the value afterwards drives the line geometry.
