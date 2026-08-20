# Implementing a New Sketch Tool (Shape)

This guide covers adding a new drawable sketch shape (line, rectangle, circle, polygon, arc, etc.) to PolySmith. It documents every file that must be touched and the code paths involved, including pitfalls discovered during implementation.

> **Text tool note (2026-08):** the Text tool is a *different shape* of
> feature — it has no drag draft and no dimension session. Its flow is
> the fillet panel's (tool active → click places the entity → floating
> panel edits → Enter confirm / Escape delete), its geometry is
> *generated* (see `SketchText` / `refresh_sketch_texts` in
> [Text-Tool-Implementation-Plan](Text-Tool-Implementation-Plan)) rather
> than drawn, and it needs no per-entity viewport rendering (glyphs are
> ordinary sketch lines). Use the fillet/panel precedent for any future
> "place then edit" tools.

---

## Files to Touch (Checklist)

### C++ Core

| File | What to add |
|---|---|
| `native/cad-core/src/core/feature.h` | Entity struct (`SketchPolygon`, etc.) |
| `native/cad-core/src/core/sketch_feature.h` | `add_sketch_<tool>` declaration, auto-dimension creation |
| `native/cad-core/src/core/sketch_feature.cpp` | Implementation: geometry math, entity push, auto-dimension push, `refresh_sketch_derived_state` call, validation |
| `native/cad-core/src/core/document.h` | `DocumentManager::add_sketch_<tool>` declaration |
| `native/cad-core/src/core/document.cpp` | Wrapper: `require_document`, active-sketch validation, `push_undo/clear_redo`, call core op, `refresh_linked_extrudes`, `bump_geometry_revision` |
| `native/cad-core/src/app.cpp` | Command handler: `if (command.type == "add_sketch_<tool>")` |
| `native/cad-core/src/protocol/serialization.cpp` | **Serialize** (both feature params `to_payload` AND viewport primitive), **deserialize** (`sketch_parameters_from_payload`) |
| `native/cad-core/src/core/viewport.cpp` | Viewport primitive builder if the shape renders in 3D viewport |

### Protocol / IPC

| File | What to add |
|---|---|
| `protocol/schema/commands.schema.json` | `"add_sketch_<tool>"` in the command type enum |

### TypeScript Types

| File | What to add |
|---|---|
| `apps/desktop-ui/src/types/geometry/sketch.ts` | Entity entry interface (`SketchPolygonEntry`), add to `SketchFeatureParameters` |
| `apps/desktop-ui/src/types/ipc.ts` | Command interface, add to `CoreCommand` union |
| `apps/desktop-ui/src/types/viewport.ts` | Viewport primitive interface |
| `apps/desktop-ui/src/types/scene.ts` | Scene object interface |
| `apps/desktop-ui/src/types/index.ts` | Re-export new types |

### TypeScript IPC / Hooks

| File | What to add |
|---|---|
| `apps/desktop-ui/src/lib/ipcProtocol.ts` | `makeAddSketch<Tool>Command(...)` builder |
| `apps/desktop-ui/src/hooks/useCadCore.ts` | Import builder, add `addSketch<Tool>` async function to the returned hook object |
| `apps/desktop-ui/src/lib/schemas/ipcSchema.ts` | Zod validation for the new viewport primitive fields |

### UI — Viewport (three.js rendering + interaction)

| File | What to add |
|---|---|
| `apps/desktop-ui/src/utils/viewport.utils.ts` | `buildSketch<Tool>Object(...)` — three.js geometry generation |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Draft/click handling, preview rendering, dimension deletion scheduling |

### UI — Toolbar

| File | What to add |
|---|---|
| `apps/desktop-ui/src/layout/SketchToolbar.tsx` | Button/icon for the new tool |
| `apps/desktop-ui/src/layout/ToolBarIcons.tsx` | SVG icon component |
| `apps/desktop-ui/src/types/geometry/contraints.ts` | `SketchTool` union type extension |

---

## The Two Commit Paths

**This is the most important concept.** Every sketch shape can be committed through two different code paths in `ViewportPanel.tsx`:

### Path 1 - `commitDraftDimensionSession` (Enter / form-submit)

Called when the user presses **Enter** in a draft dimension input, or submits the dimension form. The `session` parameter carries `lockedFields` reflecting which fields the user typed into. **After commit, the line tool switches to `"select"` mode** (matching industry-standard CAD where Enter ends the active command).

### Path 2 - Snap handler in `handlePointerUp` (click-based commits)

Called on the **second click** of a click-click sequence (or third click for 3-point modes). The `draftDimensionSessionRef.current` carries the locked-fields state at that moment. **Click commits continue chaining** by default (the line's end becomes the next start).

**Every new tool MUST handle BOTH paths.** If you only add code to one path, the tool will work in one interaction mode but silently fail in the other.

### Double-click to break chain

Industry-standard CAD uses double-click at the last endpoint to end the chain while staying in the line tool — the next click starts a fresh independent line. Implemented via:

- `chainBreakRequestedRef` in `handlePointerDown`: tracks click timing (300ms) and position (6px). Two clicks at the same location set the flag.
- Zero-length guard in pointer-up: if committed start and end are the same point (<0.01 units), the draft is cleared without calling `addSketchLine`.
- When either mechanism fires, `lineDraftStartRef` is cleared and a fresh draft starts on the next click.

### Path 2 sub-branches

Inside `handlePointerUp`, there are tool-specific branches:
- Arc
- Rectangle (with 3-point vs corner-corner/center-point sub-branches)
- Circle (with 3-point vs center-radius/2-point sub-branches)
- Polygon
- **Line (the fallthrough)** - if none of the above match, the line tool code runs

---

## Dimension Deletion

PolySmith implements on-demand dimensions:

- **Drag-only (no typing):** the auto-dimension is deleted after commit
- **Typed value during preview:** the dimension is preserved

### How it works

1. C++ core creates an auto-dimension on every non-construction shape
2. After the shape is committed, the TypeScript useEffect checks `lockedFields`
3. If no relevant field was locked, call `delete_sketch_dimension` IPC

### Adding dimension deletion for a new tool

1. **Determine the relevant draft field(s):** call `draftSessionFields(tool)` to see which `DraftDimensionField`s the tool uses (`"diameter"` for circle, `"radius"` for polygon, `"length"` for line, `"width"`+`"length"` for rectangle)

2. **Add the decision flag** to `pendingDimensionDeletionRef` in `ViewportPanel.tsx`

3. **Set the flag** in `scheduleDimensionDeletion()`:
   ```typescript
   shouldDeleteNewTool:
     tool === "newtool" && !session?.lockedFields.<relevantField>,
   ```

4. **Handle deletion** in the useEffect:
   ```typescript
   if (pending.shouldDeleteNewTool && sketch.<entities>.length > 0) {
     const entity = sketch.<entities>[sketch.<entities>.length - 1];
     if (entity && !entity.is_construction) {
       void deleteSketchDimensionRef.current(`dim-<prefix>-${entity.<id_field>}`);
     }
   }
   ```
   **Always use the last entity** (`array[array.length - 1]`), not a baseline count.

5. **Add `scheduleDimensionDeletion("newtool")`** to every commit path (both Path 1 and Path 2)

### Dimension ID Format

Auto-dimension IDs follow the pattern `dim-<prefix>-<entity_id>`:

| Tool | Prefix | Example |
|---|---|---|
| Line | `dim-line` | `dim-line-line-1` |
| Circle | `dim-circle` | `dim-circle-circle-1` |
| Polygon | `dim-polygon` | `dim-polygon-polygon-1` |

---

## Pitfalls and Lessons Learned

### 1. Stale `fromLineCount` Race Condition

**Problem:** The baseline entity count was captured from `sketchFeature` during the event handler, which reflects the *current* React render. When the user clicks rapidly (e.g., chained lines), React may not have re-rendered from the previous commit. The baseline count would be stale, causing the deletion to target the wrong entity.

**Solution:** Use the **last entity** in the array (`array[array.length - 1]`). Since entities are always appended, the last one is always the just-committed one - immune to React timing.

### 2. Null Session for Click-Based Commits

**Problem:** For click-based creation (no drag), `draftDimensionSessionRef.current` can be `null`. A `dSession?.tool === "tool"` guard would fail (`null?.tool` is `undefined`), blocking the deletion.

**Solution:** Don't guard on the session's tool field. Use `!dSession?.lockedFields.<field>` directly - with a null session, this evaluates to `!undefined` = `true`, correctly triggering deletion.

### 3. Circle `lockedFields.radius` Doesn't Exist

**Problem:** `draftSessionFields("circle")` returns `["diameter"]` only - there is no radius field in the UI. Checking `!session.lockedFields.radius` was always true (radius is never locked), but the actual relevant field is `diameter`.

**Solution:** Check the actual fields returned by `draftSessionFields(tool)`, not assumed fields. For circle, only check `lockedFields.diameter`.

### 4. Old Session Replaced Before Capture (Line Chaining)

**Problem:** In the line chaining code, `draftDimensionSessionRef.current` was replaced with a new session BEFORE reading `lockedFields`. The new session always has `lockedFields: {}`, so deletion never triggered.

**Solution:** Capture `const oldSession = draftDimensionSessionRef.current` before creating the replacement session. Pass it as `preCapturedSession` to `scheduleDimensionDeletion`.

### 5. Construction Lines Have No Auto-Dimensions

**Problem:** The C++ core only creates auto-dimensions for non-construction entities. The TS side would try to delete a dimension that never existed.

**Solution:** Check `entity.is_construction` before calling `delete_sketch_dimension`. The C++ side also silently ignores missing dimensions as a safety net.

### 6. Split Commit Paths

**Problem:** Shape commits can go through `commitDraftDimensionSession` (Enter) OR the snap handler in `handlePointerUp`. Every path must be updated independently, and it's easy to miss one.

**Solution:** Use the centralized `scheduleDimensionDeletion(tool, preCapturedSession?)` helper. Call it from every commit path. Do NOT inline the pending ref setup - that leads to drift between paths.

### 7. `polygons` Missing from Serialization

**Problem:** When adding polygon support, the `polygons` field was added to the C++ struct but forgotten in `serialization.cpp`. The TS side accessed `params.polygons.length` which threw `TypeError` because `polygons` was `undefined`.

**Solution:** Always update BOTH the serialization (`to_payload`) and deserialization (`from_payload`) in `serialization.cpp` when adding a new field. The TS side should use optional chaining (`params?.polygons?.length ?? 0`).

---

## Manual Sketch Dimension Tool Completion (Shipped)

> Implemented 2026-05-24. All three single-entity dimension creation commands
> (`add_sketch_line_length_dimension`, `add_sketch_circle_radius_dimension`,
> `add_sketch_polygon_radius_dimension`) are wired end-to-end through C++ core,
> IPC, TypeScript types, and ViewportPanel.tsx click handlers. Polygon entities
> are now handled alongside lines and circles. The plan below is retained as
> reference documentation.

### Goal

The Dimension tool already exists in the sketch toolbar (icon, hotkey `D`,
floating info panel, two-click flows for angle/distance between entities,
dimension label dragging). What's missing is the ability to **create a
dimension on a single entity that doesn't already have one**.

When the user clicks a line or circle whose auto-dimension was deleted (by
the on-demand system), the tool currently just selects the
entity. It needs to **create** the missing dimension instead.

### Current State

**Already shipped:**
- ✅ Dimension tool entry in `SketchToolbar.tsx`
- ✅ `DimensionIcon` in `ToolBarIcons.tsx`
- ✅ Hotkey handler → `setSketchTool("dimension")`
- ✅ Floating info panel ("Click a line or circle...")
- ✅ `"dimension"` in `SketchTool` union type
- ✅ `active_sketch_tool: "dimension"` accepted by C++ core
- ✅ Two-click flow for angle (`addSketchAngleDimension`)
- ✅ Two-click flow for distance (`addSketchDistanceDimension`)
- ✅ Two-click flow for circle-pair distance
- ✅ `dimensionToolFirstLineRef` / `dimensionToolFirstLine` state for staging first pick
- ✅ `pendingDimensionPlacementRef` pattern for auto-opening the dimension editor
- ✅ Dimension label dragging on pointer down

**What's missing (the gap this plan fills):**

When the Dimension tool clicks a **single** entity:
- **Line click**: checks if `dim-line-{id}` exists. If not, just selects
  the entity → should instead **create** a `line_length` dimension
- **Circle click**: checks if `dim-circle-{id}` exists. If not, just
  selects the entity → should instead **create** a `circle_radius`
  dimension
- **Polygon click**: no handling at all → should create a
  `polygon_radius` dimension

### IPC Commands Needed

Three new IPC commands need to be added end-to-end:

| Command | Type | Payload |
|---|---|---|
| `add_sketch_line_length_dimension` | IPC | `{ line_id: string }` |
| `add_sketch_circle_radius_dimension` | IPC | `{ circle_id: string }` |
| `add_sketch_polygon_radius_dimension` | IPC | `{ polygon_id: string }` |

**Schema gap to fix:** `add_sketch_angle_dimension` and
`add_sketch_distance_dimension` exist in C++ + TS but are missing from
`protocol/schema/commands.schema.json` — the schema fix is included.

### C++ Implementation

Each single-entity dimension command follows the same pattern:

1. Validate the entity exists and is not construction
2. Compute current length / radius
3. Check for duplicate dimension on this entity (skip if exists)
4. Create `SketchDimension{ id: "dim-<prefix>-" + entity_id, kind: "<kind>", entity_id: entity_id, value: current_value }`
5. Append to `parameters.dimensions`

Dimension ID format: `dim-line-{id}`, `dim-circle-{id}`, `dim-polygon-{id}`.
Kind values: `line_length`, `circle_radius`, `polygon_radius`.

Files touched:
- `protocol/schema/commands.schema.json` — add 5 commands to the enum
- `native/cad-core/src/core/document.h` — declare 3 new methods
- `native/cad-core/src/core/document.cpp` — implement (find active sketch, call core, push undo, refresh)
- `native/cad-core/src/core/sketch_feature.h` — declare core helpers
- `native/cad-core/src/core/sketch_feature.cpp` — implement helpers (reuse auto-dimension creation logic)
- `native/cad-core/src/app.cpp` — register 5 command handlers

### TypeScript Changes

- `apps/desktop-ui/src/types/ipc.ts` — add 3 command interfaces + include all 5 in `CadCoreCommand` union
- `apps/desktop-ui/src/lib/ipcProtocol.ts` — add 3 command builders
- `apps/desktop-ui/src/hooks/useCadCore.ts` — add 3 hooks + refs

### ViewportPanel.tsx Wiring

**Line click (current → change):**

```ts
// Current: just selects the entity
void selectSketchEntityRef.current(hit.id, false);

// Change: create missing dimension and open editor
pendingDimensionPlacementRef.current = true;
void addSketchLineLengthDimensionRef.current(hit.id)
  .catch(() => { pendingDimensionPlacementRef.current = false; });
```

**Circle click:** Same pattern using `addSketchCircleRadiusDimensionRef`.

**Polygon click (new — not handled at all currently):**
```ts
if (hit.entityKind === "polygon") {
  const dimensionId = `dim-polygon-${hit.id}`;
  const dimensionExists = /* check */
  if (dimensionExists) {
    handleDimensionClick(dimensionId);
  } else {
    pendingDimensionPlacementRef.current = true;
    void addSketchPolygonRadiusDimensionRef.current(hit.id)
      .catch(() => { pendingDimensionPlacementRef.current = false; });
  }
  return;
}
```

### Transaction Flow (Line Click Example)

```
User clicks a line in Dimension tool
  → pendingDimensionPlacementRef = true
  → IPC: add_sketch_line_length_dimension(line_id)
    → C++: validate line, check dup, compute length, push SketchDimension
    → C++: refresh_sketch_derived_state + bump_geometry_revision
    → C++: return document_state (with new dimension)
  → TS: receive document_state with new dimension
    → pendingDimensionPlacementRef fires useEffect
    → beginDimensionPlacement(selectedSketchDimension)
    → dimension editor opens with the line length value
  → User types a new value → Enter
    → IPC: update_sketch_dimension(dim_id, new_value)
    → Line resizes (constraint behavior)
```

### Rendering

No viewport rendering changes needed — the viewport already renders all
`SketchDimension` entries via `ViewportSketchDimensionPrimitive`. Newly
created dimensions appear immediately.

### Files Changed (Full List)

| File | Change |
|---|---|
| `protocol/schema/commands.schema.json` | Add 5 commands to the enum |
| `native/cad-core/src/core/document.h` | Declare 3 new dimension creation methods |
| `native/cad-core/src/core/document.cpp` | Implement the 3 new methods |
| `native/cad-core/src/core/sketch_feature.h` | Declare core helpers |
| `native/cad-core/src/core/sketch_feature.cpp` | Implement helpers, extract auto-dimension creation |
| `native/cad-core/src/app.cpp` | Register 5 command handlers |
| `apps/desktop-ui/src/types/ipc.ts` | Add 3 command interfaces + add all 5 to union |
| `apps/desktop-ui/src/lib/ipcProtocol.ts` | Add 3 command builders |
| `apps/desktop-ui/src/hooks/useCadCore.ts` | Add 3 hooks + refs |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Replace `selectSketchEntity` with dimension creation + add polygon support |
| `AI-CAD-Command-Language` | Document new dimension commands |
| `IPC-Protocol` | Document new dimension commands |

---

## Line Dimension Tool Specification

The full behavioral specification for line dimension creation,
intent-based dimension detection during drag, and post-creation endpoint
drag is documented in [Implementation-Log](Implementation-Log) (2026-05-24, "Line
Dimension Tool — Specification"). Key items:

- **Preview-phase**: length/angle inputs become constraints only when
  explicitly edited; unedited values are driven and invisible after commit.
- **Post-creation**: clicking a line body in Dimension tool creates a
  single-entity dimension but should also permit a two-point distance
  dimension via drag-direction disambiguation.
- **Intent detection**: perpendicular drag → line length; parallel drag
  → endpoint-to-endpoint distance; ambiguous → default to line length.
- **Endpoint drag**: belongs to the select tool. Dragging an unconstrained
  line endpoint resizes it in real time. Dragging a constrained line
  endpoint converts the dimension to driven.

**Two root problems identified (2026-05-24):**

1. **Flickering**: auto-dimensions are created by the core during line
   preview, then deleted by `scheduleDimensionDeletion()` on commit.
   Wastes IPC round-trips and causes visual flash. Fix: suppress
   auto-dimension creation during preview (requires a preview-mode flag
   in the C++ core's entity push path).

2. ✅ **First-click steals two-point flow**: `dimCreateLine()` sends IPC
   immediately on first click without a regroup path. Fixed 2026-05-24:
   the regroup path now detects when user clicks a different entity after
   a just-created dimension, deletes the hasty dim, and creates the
   correct two-entity/point dimension instead. Point-snap → wait also
   implemented (stages point, doesn't create dim). Empty-space click
   accepts the current dimension.

Three concrete work items remain: post-creation endpoint drag (select
tool), right-click context-sensitive behavior (Enter/Escape/repeat-last),
and perpendicular snap for dimension tool disambiguation.

**Right-click specification** (see [Implementation-Log](Implementation-Log) 2026-05-24):
context-sensitive — Enter during dimension editing, Escape during active
tool, repeat-last-tool when idle. Must be implemented as a first-class
CAD input primitive, not a context menu.
