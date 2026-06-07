/**
 * Help content index for the dynamic help system.
 *
 * Each tool/panel/command is a HelpEntry with sections keyed by heading.
 * Sections can be rendered as collapsible blocks in floating popovers.
 *
 * Source of truth: help/*.md files (human-readable documentation).
 * This module is the runtime data format consumed by the app.
 */

export interface HelpSection {
  heading: string;
  body: string;
}

export interface HelpShortcut {
  key: string;
  context: string;
  action: string;
}

export interface HelpEntry {
  title: string;
  summary: string;
  sections: HelpSection[];
  shortcuts: HelpShortcut[];
  activation: string;
}

function sec(heading: string, body: string): HelpSection {
  return { heading, body };
}

function sc(key: string, context: string, action: string): HelpShortcut {
  return { key, context, action };
}

// ---------------------------------------------------------------------------
// Line tool
// ---------------------------------------------------------------------------

const lineEntry: HelpEntry = {
  title: "Line Tool",
  summary:
    "Creates straight-line sketch entities with chained drafting, dimension input, and parameter support.",
  activation:
    "Click the **Line** button in the sketch toolbar, or press `L`.",
  shortcuts: [
    sc("L", "Select mode", "Activate Line tool"),
    sc("Tab", "Dimension field focus", "Cycle to next field (Length → Angle)"),
    sc("Shift+Tab", "Dimension field focus", "Cycle to previous field"),
    sc("Enter", "Dimension field focus", "Commit line, exit to Select"),
    sc("Escape", "Any draft state", "Cancel draft, exit to Select"),
    sc("Shift (hold)", "During placement", "Lock to horizontal/vertical axis"),
    sc("Double-click", "At endpoint", "Break chain, stay in line tool"),
  ],
  sections: [
    sec(
      "Interaction Modes",
      "**Click-Click:** Click start → move → click end. Chains automatically.\n" +
        "**Click-Type:** Click start → move → type Length/Angle → Enter to commit and exit, or click to commit and chain.\n" +
        "**Click-Constrain:** Hold Shift to lock to the nearest axis.",
    ),
    sec(
      "Dimension Fields",
      "**Length** — mm (or display unit). Distance from start to end.\n" +
        "**Angle** — degrees (0–180). From positive X axis; sign determined by quadrant.",
    ),
    sec(
      "Chaining",
      "Click commits continue chaining: the endpoint becomes the next start.\n" +
        "**Double-click** at the endpoint breaks the chain. The tool stays active but unchained.",
    ),
    sec(
      "Parameter Expressions",
      "Type parameter names (e.g. `width`) or formulas (`width * 2`) into dimension fields.\n" +
        "Resolved client-side during draft (debounced 300ms). After commit, expressions are stored on dimensions and re-evaluated on parameter changes.\n" +
        "Angle parameters (`kind = \"angle\"`) can only be used in angle dimensions.",
    ),
    sec(
      "Construction Lines",
      "Toggle **Construction** in the sketch tool panel. Dashed rendering, excluded from profiles.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Circle tool
// ---------------------------------------------------------------------------

const circleEntry: HelpEntry = {
  title: "Circle Tool",
  summary:
    "Creates sketch circles with multiple modes (center-radius, two-point, three-point).",
  activation:
    "Click the **Circle** button in the sketch toolbar, or press `C`.",
  shortcuts: [
    sc("C", "Select mode", "Activate Circle tool"),
    sc("Enter", "Dimension field", "Commit, exit to Select"),
    sc("Escape", "Draft state", "Cancel, exit to Select"),
  ],
  sections: [
    sec(
      "Creation Modes",
      "**Center-Radius:** Click center → move → click circumference.\n" +
        "**Two-Point:** Click one diameter endpoint → click opposite endpoint.\n" +
        "**Three-Point:** Click three points on the circumference.\n" +
        "**Tangent modes** reserved for future support.",
    ),
    sec(
      "Dimension Fields",
      "**Diameter** — mm (or display unit). Core stores radius in `circle_radius` dimension kind.",
    ),
    sec(
      "Parameter Expressions",
      "Type parameter names into the diameter field. Same syntax as the Line tool.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Rectangle tool
// ---------------------------------------------------------------------------

const rectangleEntry: HelpEntry = {
  title: "Rectangle Tool",
  summary:
    "Creates sketch rectangles with corner-corner, center-point, and three-point modes.",
  activation:
    "Click the **Rectangle** button in the sketch toolbar, or press `R`.",
  shortcuts: [
    sc("R", "Select mode", "Activate Rectangle tool"),
    sc("Enter", "Dimension field", "Commit, exit to Select"),
    sc("Escape", "Draft state", "Cancel, exit to Select"),
  ],
  sections: [
    sec(
      "Creation Modes",
      "**Corner-Corner:** Click first corner → click opposite corner.\n" +
        "**Center-Point:** Click center → click a corner.\n" +
        "**Three-Point:** Click first corner → click second corner (defines edge) → click third point for width.",
    ),
    sec(
      "Dimension Fields",
      "**Width** and **Length** — mm (or display unit).",
    ),
    sec(
      "Parameter Expressions",
      "Both fields accept parameter names and formulas.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Parameters panel
// ---------------------------------------------------------------------------

const parametersEntry: HelpEntry = {
  title: "Parameters",
  summary:
    "Document-scoped named values that can be referenced in dimension expressions.",
  activation:
    "Click the **f(x)** button in the top ribbon to open the Parameters panel.",
  shortcuts: [],
  sections: [
    sec(
      "Adding a Parameter",
      "1. Click **+ Add Parameter**.\n" +
        "2. Enter a unique **Name**.\n" +
        "3. Enter an **Expression** (e.g. `50`, `width * 2`).\n" +
        "4. Select **Kind** — Length (mm) or Angle (degrees).\n" +
        "5. Selecting the kind commits immediately.",
    ),
    sec(
      "Expression Syntax",
      "Numbers: `50`, `3.14` · Arithmetic: `+ - * /` · Parens: `(a+b)/3`\n" +
        "Parameter references: `width`, `my_param` · Unary minus: `-50`",
    ),
    sec(
      "Kind Checking",
      "**Length** — mm. **Angle** — degrees.\n" +
        "Angle parameters cannot be used in length-type dimensions (core throws an error).",
    ),
    sec(
      "Using in Dimensions",
      "Type the parameter name in any dimension field. Resolved during draft (client-side, 300ms debounce), during edit (core-side), and on parameter change (all expressions re-evaluate automatically).",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Select tool
// ---------------------------------------------------------------------------

const selectEntry: HelpEntry = {
  title: "Select Tool",
  summary:
    "Selects sketch entities, faces, edges, and vertices for editing or constraints.",
  activation:
    "Click the **Select** button in the sketch toolbar, or press `V`. Also active by default when no other tool is active.",
  shortcuts: [
    sc("V", "Select mode", "Activate Select tool"),
    sc("Shift+click", "Selection", "Add to selection (toggle)"),
  ],
  sections: [
    sec(
      "Selection",
      "Click a sketch line, circle, arc, point, dimension, constraint icon, or profile to select it.\n" +
        "Hold **Shift** and click to add or remove from the selection.\n" +
        "Click empty space to deselect.",
    ),
    sec(
      "Interaction",
      "Selected entities show highlight handles. Selected dimensions open an inline editor.\n" +
        "Press **Delete** or **Backspace** to remove selected entities.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Dimension tool
// ---------------------------------------------------------------------------

const dimensionEntry: HelpEntry = {
  title: "Dimension Tool",
  summary:
    "Creates and edits sketch dimensions — linear, radial, angular, and point-to-point distance.",
  activation:
    "Click the **Dimension** button in the sketch toolbar, or press `D`.",
  shortcuts: [
    sc("D", "Select mode", "Activate Dimension tool"),
    sc("Click", "Placement", "Commit (accept auto value)"),
    sc("Enter", "Dimension editor", "Commit typed value, close editor"),
    sc("Escape", "Placement", "Delete dimension (cancel creation)"),
    sc("Escape", "Dimension editor", "Restore previous value, close editor"),
    sc("Double‑click", "Dimension label", "Re‑open editor for editing"),
  ],
  sections: [
    sec(
      "Single‑Entity Dimensions",
      "Click a **line** → length dimension.\n" +
        "Click a **circle** → radius/diameter dimension.\n" +
        "Click a **polygon** → radius dimension.",
    ),
    sec(
      "Two‑Entity Dimensions",
      "Click a first entity (line, circle, or polygon) — a single‑entity dimension is created and the entity is staged.\n" +
        "Click a **second, different entity** to morph into:\n" +
        "• **Angle** — two lines sharing an endpoint.\n" +
        "• **Parallel distance** — two parallel lines.\n" +
        "• **Point‑to‑point** — click two endpoints (line ends, circle centres).",
    ),
    sec(
      "Placement & Commit",
      "After a dimension is created, drag the label to position it.\n" +
        "**Click** anywhere on the canvas to commit the automatic value.\n" +
        "**Escape** during placement deletes the dimension entirely.\n" +
        "If you type a value, press **Enter** to commit.",
    ),
    sec(
      "Editing",
      "**Double‑click** a dimension label to re‑open the editor.\n" +
        "Type a new value or expression, then **Enter** to commit.\n" +
        "**Escape** restores the previous value and closes the editor.",
    ),
    sec(
      "Expressions",
      "Type a parameter name (e.g. `width`) or formula (`width * 2`) instead of a raw number.\n" +
        "Use **ArrowUp**/**ArrowDown** and **Enter**/**Tab** to select from parameter suggestions.\n" +
        "Expressions are stored on the dimension and re‑evaluated when parameters change.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Arc tool
// ---------------------------------------------------------------------------

const arcEntry: HelpEntry = {
  title: "Arc Tool",
  summary:
    "Creates curved sketch arcs using three-point or center-start-end modes.",
  activation:
    "Click the **Arc** button in the sketch toolbar.",
  shortcuts: [
    sc("Enter", "Draft state", "Commit arc, exit to Select"),
    sc("Escape", "Draft state", "Cancel, exit to Select"),
  ],
  sections: [
    sec(
      "Three-Point",
      "Click first endpoint → click second endpoint → click a point on the arc.",
    ),
    sec(
      "Center-Start-End",
      "Click center → click start angle → click end angle.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Polygon tool
// ---------------------------------------------------------------------------

const polygonEntry: HelpEntry = {
  title: "Polygon Tool",
  summary:
    "Creates regular polygons with configurable side count and creation mode.",
  activation:
    "Click the **Polygon** button in the sketch toolbar.",
  shortcuts: [
    sc("Enter", "Draft state", "Commit polygon, exit to Select"),
    sc("Escape", "Draft state", "Cancel, exit to Select"),
  ],
  sections: [
    sec(
      "Modes",
      "**Circumscribed:** Center → radius to a vertex.\n" +
        "**Inscribed:** Center → radius to a face midpoint.\n" +
        "**Edge:** Click two points to define one edge.",
    ),
    sec(
      "Sides",
      "Use the side count control in the tool panel (default 6).",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Fillet tool
// ---------------------------------------------------------------------------

const filletEntry: HelpEntry = {
  title: "Fillet / Chamfer Tool",
  summary:
    "Rounds (fillet) or bevels (chamfer) sharp corners between sketch lines.",
  activation:
    "Click the **Fillet** button in the sketch toolbar.",
  shortcuts: [
    sc("Escape", "Fillet state", "Cancel"),
  ],
  sections: [
    sec(
      "Applying",
      "Click a corner point shared by exactly two non-construction lines to apply a fillet.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Trim tool
// ---------------------------------------------------------------------------

const trimEntry: HelpEntry = {
  title: "Trim Tool",
  summary:
    "Deletes sketch curve segments by cutting them at intersection points with other curves.",
  activation:
    "Click the **Trim** button in the sketch toolbar (Modify tab), or press `T`.",
  shortcuts: [
    sc("T", "Select mode", "Activate Trim tool"),
    sc("Escape", "Trim mode", "Exit to Select mode"),
  ],
  sections: [
    sec(
      "How It Works",
      "Hover a curve to preview the segment that will be deleted (highlighted in red). " +
        "Click to delete it. The entity shortens or splits at the nearest intersection points.\n\n" +
        "End segment → curve shortens. Middle segment → curve splits into two. No intersections → entity deleted.",
    ),
    sec(
      "Constraints",
      "Trim is destructive. All constraints, relations, dimensions, anchors, and fillets on the trimmed entity are deleted. " +
        "Shared endpoints are severed. Surviving entities get independent point IDs. " +
        "Re-add constraints manually after trimming if needed.",
    ),
    sec(
      "Multi-Click Repeat",
      "The tool stays active after each operation — trim multiple segments in sequence. Press Escape to exit.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Project tool
// ---------------------------------------------------------------------------

const projectEntry: HelpEntry = {
  title: "Project Tool",
  summary:
    "Projects 3D body geometry (faces, edges, vertices, profiles) into the active sketch plane.",
  activation:
    "Click the **Project** button in the sketch toolbar, or press `P`.",
  shortcuts: [
    sc("P", "Select mode", "Activate Project tool"),
    sc("P", "Project mode", "Deactivate (return to Select)"),
    sc("Escape", "Project mode", "Deactivate"),
  ],
  sections: [
    sec(
      "Projecting",
      "Click a **face** to project its outline.\n" +
        "Click an **edge** to project it as a line or circle.\n" +
        "Click a **vertex** to project it as a sketch point.\n" +
        "Click a **profile** boundary to project the entire closed loop.",
    ),
    sec(
      "Live Links",
      "Projected entities maintain a live link to their source. If the source body changes, projected geometry updates automatically on recompute.",
    ),
  ],
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

export const helpRegistry: Record<string, HelpEntry> = {
  select: selectEntry,
  line: lineEntry,
  dimension: dimensionEntry,
  rectangle: rectangleEntry,
  circle: circleEntry,
  arc: arcEntry,
  polygon: polygonEntry,
  fillet: filletEntry,
  trim: trimEntry,
  project: projectEntry,
  parameters: parametersEntry,
};

function allHelpEntries(): HelpEntry[] {
  return Object.values(helpRegistry);
}
