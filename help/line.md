# Line Tool

Creates a single straight-line sketch entity. The line tool supports chained
drafting, typed dimension input, parameter expressions, constraints, and
construction mode.

---

## Activation

- **Click** the Line button in the sketch toolbar (Create tab)
- **Hotkey:** `L` (configurable in settings)

The cursor changes to crosshairs. A status bar prompt reads "Click to place
start point."

---

## Interaction Modes

### Click-Click (Freehand)

1. **Click** to place the start point.
2. **Move** the mouse — a preview line follows the cursor.
3. **Click** to place the end point. The line is committed and chaining
   begins: the endpoint becomes the start of the next line.

### Click-Type (Dimension-Driven)

1. **Click** to place the start point.
2. **Move** the mouse — a preview line appears with floating dimension inputs
   for length and angle.
3. **Type** a value into the Length or Angle field.
4. **Tab** to switch between fields. The preview updates live.
5. **Commit** the line:
   - **Enter** — commits the line at the current position and keeps the Line tool armed.
   - **Click** on the canvas — commits the line and continues chaining.

Parameter names can be typed into dimension fields (e.g. `width`). They
resolve against the document's parameter table. Partial names are
debounced (300ms idle before evaluation).

### Click-Constrain

Hold **Shift** while moving the mouse to lock the line to the nearest axis
(horizontal or vertical). Release Shift to return to free movement.

---

## Dimension Fields

| Field    | Unit                           | Description |
|----------|--------------------------------|-------------|
| Length   | mm (or current display unit)   | Distance from start to end |
| Angle    | degrees (unsigned 0–180)       | Angle from positive X axis; sign (CW/CCW) is determined by the line's quadrant orientation |

When a dimension field is typed into, the auto-generated dimension is
preserved (not auto-deleted). When no fields are typed, the dimension is
deleted after commit.

### Parameter Expressions

Both fields accept parameter names and arithmetic formulas:

```
width           — resolves to the parameter named "width"
width * 2       — arithmetic expression
my_angle + 15   — parameter + constant
```

Parameters are resolved in real time during draft. Angle parameters
(`kind = "angle"`) store degrees. Length parameters store mm.

---

## Chaining

After each line is committed by **click** (not Enter), chaining begins
automatically:

- The previous line's endpoint becomes the next line's start point.
- A new preview line appears anchored at that point.
- Dimension fields are focused and ready for input.

Chaining continues until the user breaks the chain or exits the tool.

### Breaking the Chain

**Double-click** at the current endpoint to end the chain, or
**right-click** while the rubber band is visible. The line tool stays
active but unchained — the next click starts a fresh independent line
from a new start point.

**Double-click mechanism:** Two clicks within 300ms and 6px at the same
location while a draft is active. The zero-length line guard also prevents
degenerate geometry.

**Right-click mechanism:** Cancels the current rubber band (draft start,
preview, dimension session) without switching to Select mode — equivalent
to Escape without dearming the tool.

---

## Exiting the Tool

| Action                           | Result |
|----------------------------------|--------|
| **Enter** (in a dimension field) | Commits the line at the current position, keeps the Line tool armed |
| **Right-click** (rubber band active) | Cancels the rubber band / breaks the chain, keeps the Line tool armed |
| **Escape**                       | Cancels the current draft, switches to Select mode |
| **Click another tool**           | Cancels the draft, activates the new tool |
| **Hotkey for another tool**      | Same as clicking |

---

## Keyboard Shortcuts

| Key                  | Context                 | Action |
|----------------------|-------------------------|--------|
| `L`                  | Select mode             | Activate Line tool |
| `Tab`                | Dimension field focus   | Cycle to the next field (Length → Angle → Length) |
| `Shift+Tab`          | Dimension field focus   | Cycle to the previous field |
| `Enter`              | Dimension field focus   | Commit line, keep Line tool armed |
| `Right-click`        | Rubber band active      | Cancel rubber band / break chain, keep Line tool armed |
| `Escape`             | Any draft state         | Cancel draft, exit to Select |
| `Shift` (hold)       | During placement        | Lock to horizontal/vertical axis |

---

## Construction Lines

Toggle the **Construction** checkbox in the sketch tool panel while the
Line tool is active. Construction lines:

- Render as dashed lines (viewport style: `cad-construction-line`)
- Are excluded from profile detection
- Do not contribute to closed-loop profiles
- Do not generate auto-dimensions
- Can still be dimensioned manually

---

## Constraints

### Automatic

- Dragging near a horizontal/vertical axis may snap to it.
- The core enforces axis constraints after commit (via
  `set_sketch_line_constraint`).

### Manual

- After committing, use the constraint tools (Horizontal, Vertical) or
  the constraint panel to apply constraints.

### Perpendicular from Endpoint

If the start point of a new line is placed on an existing line, a
perpendicular constraint is automatically applied.

---

## Parameter-Driven Lines

To drive a line with parameters:

1. Create parameters in the Parameters panel (e.g. `line_len = 50`,
   `line_angle = 30`).
2. Activate the Line tool and place the start point.
3. Type `line_len` in the Length field and `line_angle` in the Angle field.
4. The preview resolves immediately (client-side lookup) and the line
   commits.

After commit, editing the dimension and typing the parameter name again
stores the expression on the dimension. Changing the parameter value in
the Parameters panel re-evaluates the dimension expression and updates
the geometry automatically.

### Kind Checking

- Angle parameters (`kind = "angle"`) can only be used in angle-type
  dimensions (`angle`, `line_angle`).
- Using an angle parameter in a length dimension produces an error:
  `"Angle parameter 'X' cannot be used in a length dimension"`.

---

## Internal Implementation Notes

### Core Command

```
type: "add_sketch_line"
payload: { start_x, start_y, end_x, end_y, is_construction }
```

### Auto-Dimensions Created

| Dimension | ID Pattern              | Kind          |
|-----------|-------------------------|---------------|
| Length    | `dim-line-{line_id}`    | `line_length` |
| Angle     | `dim-line-angle-{id}`   | `line_angle`  |

### Dimension Deletion

Auto-dimensions are deleted after commit unless the user typed a value
into the corresponding draft field.

### Draft Session Flow

```
handlePointerDown  → create first-point draft session
handlePointerMove  → update preview + dimension labels
handlePointerUp    → commit via add_sketch_line IPC
                   → chain: new draft from endpoint (click)
                   → or: keep tool armed, unchain (double-click / right-click)
Escape             → cancel draft, switch to select mode
Enter              → commit current position, keep tool armed
```

### Double-Click Detection

```
Refs: lastPointerDownTimeRef, lastPointerDownPosRef, chainBreakRequestedRef
Logic: <300ms && <6px from previous pointerdown → break chain
Guard: committed start ≈ committed end → skip addSketchLine (zero-length)
```
