# PolySmith Implementation Log

This document tracks concrete implementation milestones as they land in the codebase.

## 2026-05-28

### Dimension Tool — Placement, Angle, and Drag Fixes

**Placement commit (click-to-accept):**
- `handlePointerUp` during placement now closes the editor immediately (`setIsDimensionEditorOpen(false)`) instead of focusing the input. The auto-detected value is accepted; edit later by double-clicking the label.
- Pointer capture (`setPointerCapture`) is set on the canvas during placement so the `pointerup` event always reaches the canvas handler, even when the cursor is over the editor input.
- The editor form and input now use `pointer-events-none` so clicks pass through to the canvas. The parameter suggestion dropdown retains `pointer-events-auto`.
- `handlePointerDown` sets `pointerDown` during placement so the entity click handler can run after placement commit (needed for two-pick angle/distance flows).

**Escape during placement:**
- The input's `onKeyDown` Escape handler now checks `dimensionLabelDragRef.current?.isPlacement`. During placement, Escape deletes the dimension entirely (matching the global handler). After placement, Escape restores the previous value and closes the editor.

**Point priority in dimension tool:**
- `intersectSceneTargets` now checks sketch points and entities BEFORE dimension labels when the dimension tool is active. This prevents existing dimension visuals (extension lines, arrows, sprites) from intercepting clicks meant for endpoints during point-to-point or two-entity workflows.

**Angle dimension — two-pick flow:**
- `dimCreateLine` now stages the entity via `dimensionToolFirstLineRef` so the second click can morph into an angle or distance dimension.
- The two-entity handler deletes the single-entity dimension (`dim-line-{id}`) before creating the two-entity dimension.
- After placement commit, `handlePointerUp` now falls through to entity handling instead of returning, so the second click in a two-pick flow is processed.

**Angle dimension — arc rendering (`viewport.utils.ts`):**
- Arc sweep normalized to (-π, π] and always uses `Math.abs()` — the (uAxis, vAxis) frame is already oriented so positive sweep = short path to endRay. Removed the `ccw` flip that was sending the arc the long way around.
- Arc always starts at angle 0 (= dimensionStart) instead of using the core's `startAngle` as an absolute offset in the (uAxis, vAxis) frame.

**Angle dimension — label dragging:**
- `angleDimensionFrame` fallback path now uses `dimension.arcCenter` (core-provided world-space pivot) instead of trying to compute the pivot from anchors — `anchorStart == dimensionStart` for angle dims, so the anchor-based computation always failed.
- Click-to-select: first click on a dimension label selects it (highlights); re-click on the already-selected dimension starts the drag.
- Minimum arc/label radius dropped from `anchorRadius + 1` (~5) to `2.0`.
- The stored drag position now represents the **label** position (not the arc control point). `displayedSketchDimensions` reverse-computes the arc radius from the label distance so the label tracks the cursor directly without cascading shrink.

**Known issue:** Angle dimension label drag doesn't perfectly follow the mouse cursor direction — the label moves in a slightly different direction than expected, reaching a limit and then going backwards. Needs further investigation.

**Help documentation:**
- Created `polysmith/help/dimension.md` — full reference for the Dimension tool (single-entity, two-entity, placement, editing, expressions).
- Updated `apps/desktop-ui/src/lib/help-index.ts` dimension entry with correct shortcuts and sections.

### Windows Build Fixes

The project was bootstrapped and built successfully on Windows (MSVC 14.44, NMake).
Several cross-platform issues were fixed:

**FreeType submodule** — FreeType 2.14.3 is now a first-class git submodule at
`third_party/freetype` (pinned to tag `VER-2-14-3` from `gitlab.freedesktop.org`).
The configure script (`scripts/configure-occt.mjs`) builds it into `third_party/freetype-build`
and installs to `third_party/freetype-install`, then links OCCT against it via
`-D3RDPARTY_FREETYPE_DIR`. The old manually-placed `third_party/occt/freetype-2.14.3/`
was removed. Run `pnpm deps:sync` to initialize the submodule.

**OCCT `const char*` → `const unsigned char*`** (`third_party/occt/src/StdPrs/StdPrs_BRepFont.cxx:460`) —
FreeType 2.14.3 declares `FT_Outline::tags` as `unsigned char*`, but OCCT 7.8 assigned it to
`const char*`. MSVC rejects the implicit conversion. Fixed to match the actual type.
Upstream OCCT fixed this in commit `7236e83d` (landed on `master`/`OCCT-7.9+` but not `OCCT-7.8`).

**`M_PI` undeclared** (`native/cad-core/CMakeLists.txt`) — MSVC doesn't define `M_PI` in `<cmath>`
without `_USE_MATH_DEFINES`. Added `add_compile_definitions(_USE_MATH_DEFINES)` for MSVC builds.

**Binary output path** (`native/cad-core/CMakeLists.txt`) — MSBuild places binaries in
`build/Debug/` by default. Added `CMAKE_RUNTIME_OUTPUT_DIRECTORY` to put them directly
in `build/`, and updated `build.rs` default path to `build/Debug/cad_core`.

**OCCT DLL PATH** (`apps/desktop-ui/src-tauri/src/cad_core.rs`) — At runtime, `cad_core.exe`
needs OCCT DLLs from `third_party/occt-install/win64/vc14/bin`. The Tauri spawn code now
prepends this directory to `PATH` for the child process on Windows.

**`windows_sys::BOOL`** (`apps/desktop-ui/src-tauri/src/orca_slicer.rs`) — In `windows-sys` 0.61.x,
`BOOL` moved from `Win32::Foundation` to `core::BOOL`. Updated the import.

## 2026-05-27

### 3D Move Timeline Feature

- added a semantic native `move` feature with `target_body_id`, local X/Y/Z
  translation, local X/Y/Z rotation, and `is_pending` parameters
- body replay now resolves the target compiled body, computes a pre-move
  bounding-box-center pivot, applies fixed-order local X/Y/Z rotations plus
  local translation, preserves the body id, and updates the emitted local frame
- added `create_move`, `update_move_parameters`, and `confirm_move` IPC commands
  plus save/load, TypeScript, Zod, and AI command validation/prompt coverage
- extended `viewport_state.bodies[]` with current body center, bounds size, and
  local-frame axes so the desktop UI can place a 3D transform manipulator
- enabled the Modify ribbon Move tool with the standard contextual flow:
  preselected body or pick-body pending panel, right-side numeric transform
  panel, live core preview, confirm/cancel, and timeline edit restore
- added themed viewport move helpers: local-axis translation handles, a free
  center handle, and local-axis rotation rings; drags are RAF-coalesced and go
  through `update_move_parameters`

### Snap Engine Completion & Wiring

**C++ snap engine (`snap_engine.cpp`):**
- added `collect_intersection_candidates` — line-line and line-arc intersection detection with segment clamping
- added `collect_quadrant_candidates` — 0°, 90°, 180°, 270° points on circles
- added `collect_perpendicular_candidates` — cursor-to-line foot projection, clamped to segment
- added `collect_tangent_candidates` — two tangent points per circle, cursor-outside-circle guard
- added `collect_grid_candidates` — nearest grid-intersection snap (1.0-unit grid spacing)
- wired all five new collectors into `resolve_snap()`, gated by the corresponding `SelectionFilter` flags

**C++ wiring (`viewport.h`, `viewport.cpp`, `serialization.cpp`):**
- added `std::vector<SnapCandidate> snap_candidates` to `ViewportState`
- `build_viewport_state()` now pre-computes all position-independent snap targets (endpoints, midpoints, centers) from the active sketch, gated by the document's `SelectionFilter`
- serialized `snap_candidates` as a JSON array in the viewport state payload, mirroring the `SnapCandidate` struct (kind, entity_id, point_id, local_x, local_y, label)

**TypeScript (`ViewportPanel.tsx`, `types/ipc.ts`, `ipcSchema.ts`):**
- added `SnapCandidateEntry` interface and updated `ViewportState` type
- added `snap_candidates` to the Zod schema for viewport state validation
- `sketchSnapCandidates` useMemo now prefers `viewport.snap_candidates` from the core when available; falls back to the legacy TS-side entity-walk builder when the core hasn't emitted candidates
- candidates carry `endpointHostLineId` (endpoints) and `hostLineId` + `tValue` (midpoints) from the core's `entity_id` field, preserving the existing perpendicular-foot and midpoint-anchor tooling

**Architecture impact:**
- snap target enumeration is now owned by the C++ core and gated by the document's `SelectionFilter`
- the TS panel's checkboxes reach the core through `update_selection_filter`; the core echoes the filter in `viewport_state.snap_candidates` and pre-filters candidates
- dynamic position-dependent snaps (tangent, perpendicular, axis lock) remain in TS, gated by the same filter read from `localStorage`

### Snap completeness — missing types added

**SelectionFilter extended (`feature.h`):**
- added `snap_grid_line` (lock to nearest grid line axis) and `snap_polar` (lock to polar angle increments)
- added `polar_angle_degrees` (default 15°) to the filter struct

**Pre-computed snap_candidates (`viewport.cpp`):**
- added `quadrant` — 4 fixed points per circle (0°, 90°, 180°, 270°)
- added `intersection` — all line-line and line-arc intersection points
- added `grid_line` — grid-axis anchors for axis-lock snapping

**Dynamic snap collectors (`snap_engine.cpp`):**
- added `collect_grid_line_candidates` — locks cursor to nearest vertical or horizontal grid line
- added `collect_polar_candidates` — locks cursor to nearest polar angle from a start point
- updated `resolve_snap()` signature with optional `start_x`/`start_y` for polar snap computation

**IPC + serialization (`app.cpp`, `serialization.cpp`):**
- wired `snap_grid_line`, `snap_polar`, `polar_angle_degrees` through `update_selection_filter` command handler
- serialized/deserialized new fields in document state and viewport state JSON

**UI (`SelectionFilterPanel.tsx`):**
- added Grid Line and Polar checkboxes with angle input (5°–90°, step 5°)
- updated `SelectionFilter` interface with new fields

### Dimension tool: point-to-point semantics

**TS (`ViewportPanel.tsx`):**
- clicking a line endpoint now stages the point and waits; clicking the
  *other* endpoint of the same line creates a line-length dimension
  (consistent with clicking the line body)
- clicking a point then a different entity creates a point-to-point
  distance: point→circle-center and point→polygon-center use the target's
  center point; point→other-point uses point_distance
- point→line-body still falls through to entity-to-entity (line_line_distance
  or angle), since line midpoints don't have persistent point IDs
- circle: first click stages the center point (no radius dimension created);
  re-click the same circle to create its radius dimension; click a different
  entity for two-entity distance
- same-line endpoint clicks now work even when the second click hits the
  line body rather than the endpoint sphere

### Trim arc point-ID continuity fix

**C++ (`sketch_feature.cpp`):**
- fixed circle trim: the arc created from a trimmed circle now calls
  `find_coincident_endpoint()` for both start and end positions, reusing
  existing point IDs from touching lines/arcs instead of always generating
  fresh IDs. This lets the profile loop detector chain arc edges with
  their neighbouring lines, so 2-lines-1-arc closed shapes (common after
  tangent-line + circle trim) are detected as extrudable profiles.

### Circle-nearest snap

**TS (`ViewportPanel.tsx`):**
- added circle-nearest snap in `resolveSnappedSketchPoint()`: projects the cursor
  radially onto the nearest circle's circumference. The nearest point is the
  intersection of the center→cursor ray with the circle edge. Gated by
  `snap_nearest` in the effective filter.

**C++ (`snap_engine.cpp`):**
- added `collect_circle_nearest_candidates` — same radial projection logic,
  wired into `resolve_snap()` under the `snap_nearest` gate

**i18n (`en.json`):**
- added `snap.onCircle` = "On circle" label

### Parallel snap + Alt-key object snap override

**Parallel snap (`ViewportPanel.tsx`):**
- added parallel-direction lock in `resolveSnappedSketchPoint()`: when
  `snap_parallel` is on and a line draft is in progress, finds the nearest
  existing line by angle and projects the cursor onto the parallel ray

**Object Snap Override (`ViewportPanel.tsx`):**
- Alt key inverts all snap toggles while held — disabled snap types become
  active and enabled ones become inactive

## 2026-05-26

### Sweep v1

- enhanced Sweep to resolve the selected path entity into a connected non-construction sketch line/arc chain, serialize ordered world-space path segments, and build curved/multi-segment sweeps with an OCCT pipe while preserving the existing straight-line prism path for legacy/simple sweeps
- added a core-owned `sweep` feature with `SweepFeatureParameters` storing the source profile, source path entity, cached path world coordinates, sampled profile loops, and source sketch ids
- implemented `sweep_profile`, `update_sweep_profile`, and `update_sweep_path` IPC commands for real OCCT preview geometry
- wired swept solids through the body compiler and viewport mesh path, plus serialization, IPC builders/hooks/types/schemas, AI command validation, and English UI strings
- added the Create > Sweep toolbar action, contextual Sweep panel, profile/path picking, timeline edit support, and dependency refresh that marks broken profile/path references with warnings instead of crashing
- updated `IPC-Protocol` and `AI-CAD-Command-Language` with the new sweep commands and serialized feature parameters

### Revolve v1

- added a core-owned `revolve` feature with `RevolveFeatureParameters` storing the source profile, source axis line, cached axis world coordinates, sampled profile loops, and angle
- implemented `revolve_profile`, `update_revolve_profile`, `update_revolve_axis`, and `update_revolve_angle` IPC commands for real OCCT preview geometry
- wired revolved solids through the body compiler and viewport mesh path so they behave like other timeline-created bodies
- added the Create > Revolve toolbar action, contextual Revolve panel, profile/axis picking, live angle preview, timeline edit support, IPC builders/hooks/types/schemas, AI command validation, and English UI strings
- extended dependency tracking so profile sketch or axis sketch edits re-resolve the revolve when possible and mark `dependency_broken` with a warning when the source profile or axis line can no longer be rebuilt
- updated `IPC-Protocol` and `AI-CAD-Command-Language` with the new revolve commands and serialized feature parameters

### Loft v1

- added a core-owned `loft` feature with `LoftFeatureParameters { sections[], ruled }`; sections store the source sketch/profile ids, plane frame, and sampled closed-loop profile points
- implemented `loft_profiles`, `update_loft_profiles`, and `update_loft_ruled` IPC commands; v1 supports two or more closed sketch profiles without holes, creates a new body, and uses smooth transitions by default with a ruled toggle
- wired loft solids through the native body compiler using OCCT `BRepOffsetAPI_ThruSections`, so viewport meshes and exports use the same compiled body path as other modeled solids
- extended sketch dependency refresh so dimension edits on source sketches re-resolve loft section profiles where possible; when a source profile disappears, gains unsupported holes, or cannot rebuild, the loft is marked `dependency_broken` with a warning instead of silently producing stale geometry
- added the Create > Loft toolbar action, floating contextual Loft panel, live smooth/ruled preview, timeline edit support, IPC builders/hooks/types/schemas, AI command validation, and English UI strings
- updated `IPC-Protocol` and `AI-CAD-Command-Language` with the new loft commands and serialized feature parameters

## 2026-05-20

### Parametric Parameters & Dimension Formulas

#### Goal
Add a document-scoped parameter table (name → formula expression → resolved value) and allow sketch dimensions to accept formula expressions referencing those parameters.

#### C++ Core
- `parameter.h`: new `ParameterEntry` struct (`name`, `expression`, `resolved_value`, `has_error`, `error_message`)
- `formula_eval.h/.cpp`: recursive-descent expression evaluator supporting `+`, `-`, `*`, `/`, parentheses, unary minus, and parameter name references. Cycle detection via a `resolving` set. `reify_parameters()` iterates parameters to fixpoint.
- `feature.h`: added `expression` field to `SketchDimension`. `DocumentState` gained `parameters` vector in `document.h`.
- `document.h/.cpp`: `add_parameter`, `update_parameter`, `delete_parameter` — CRUD with undo/redo, `reify_parameters`, `reify_dimension_expressions` per sketch, `refresh_history_dependencies`, and `bump_geometry_revision`.
- `sketch_feature.h/.cpp`: `update_sketch_dimension` now accepts optional `expression` parameter. `reify_dimension_expressions()` re-evaluates dimension expressions against the current parameter table.
- `app.cpp`: registered `add_parameter`, `update_parameter`, `delete_parameter` command handlers. Extended `update_sketch_dimension` to accept string expressions — evaluates them against current parameters, passes resolved value + expression to core.
- `serialization.cpp`: `expression` in sketch dimension serialization/deserialization (backward compat: absent → `""`). `parameters` array in `DocumentState` serialization/deserialization (backward compat: absent → `[]`).
- `CMakeLists.txt`: added `formula_eval.cpp`.

#### Protocol / Schema
- `protocol/schema/commands.schema.json`: `add_parameter`, `update_parameter`, `delete_parameter` in command enum.
- `IPC-Protocol`: documented parametric parameters section and dimension expressions.

#### TypeScript — Types & IPC
- `types/ipc.ts`: `ParameterEntry` interface, `AddParameterCommand`, `UpdateParameterCommand`, `DeleteParameterCommand`, extended `UpdateSketchDimensionCommand.value` to `number | string`. `DocumentState.parameters` field.
- `types/geometry/sketch.ts`: `expression` field on `SketchDimensionEntry`.
- `lib/ipcProtocol.ts`: `makeAddParameterCommand`, `makeUpdateParameterCommand`, `makeDeleteParameterCommand` builders. Updated `makeUpdateSketchDimensionCommand` to accept `number | string`.
- `hooks/useCadCore.ts`: `addParameter`, `updateParameter`, `deleteParameter` hooks. Updated `updateSketchDimension` to `number | string`.
- `lib/schemas/ipcSchema.ts`: `expression` on sketch dimension schema (default `""`). `parameters` array on document state schema (default `[]`).

#### Parameters Panel UI
- `layout/ParametersPanel.tsx`: self-contained floating panel with parameter table (Name / Expression / Value columns). Inline editing on click, commit on Enter/blur, escape to cancel. Delete button on hover. "Add Parameter" button creates inline row. Error state rendered in red with tooltip.
- `layout/header/AppHeader.tsx`: `f(x)` button to the right of workspace tabs (Create | Modify | Construct | Sketch). Toggles panel visibility via `parametersPanelOpen` prop.
- `App.tsx`: `parametersPanelOpen` state + `onToggleParametersPanel` wired through to `AppHeader`.
- `i18n/en.json`: `parameters.*` translation keys.

#### Dimension Formula Input
- `ViewportPanel.tsx`: `handleSubmitDimensionEdit` and `handleDimensionDraftChange` now accept both numeric and string values. Numeric values use the existing path; strings are sent as expressions through `updateSketchDimension` for core-side evaluation.

#### Bug Fixes & UX Polish (2026-05-21)

**Parameters panel — premature auto-commit.** The name field's `onBlur` fired when the user clicked into the expression field, sending `addParameter("name", "")` with an empty expression before the user could type a value. Fixed by removing `onBlur` from the name field. Additionally, a `useEffect` that auto-focused the name field was re-running on every `editing` keystroke, stealing focus from the expression field and triggering its `onBlur` → premature commit. Fixed to only focus when switching between rows or entering edit mode, not on every keystroke.

**Dimension editor — live preview flood.** `handleDimensionDraftChange` was sending every keystroke as an expression to the core. Typing a parameter name like "test" sent "t", "te", "tes", "test" in rapid succession, flooding the core with "Unknown parameter" errors for partial names. Originally fixed by only sending numeric values as live preview; expressions were held until Enter.

**Dimension expression live-preview keystroke — debounced.** The "hold until Enter" policy broke parameter-driven dimensioning during initial line/circle/arc/rectangle construction, because there is no Enter step during mouse-drag drafting — the user types a value and clicks to commit. The policy was relaxed to allow expression sends during typing, but partial parameter names flooded the core with "Unknown parameter" errors. Fixed by adding a 300ms debounce: each keystroke resets a timer, the expression is only sent when the user pauses for 300ms. Partial names like "my_" never reach the core; only the completed "my_angle" is sent. The timer is also cleared on cancel so a cancelled edit doesn't fire a stale send.

**Draft dimension parameter resolution.** The draft dimension system (`handleDraftDimensionChange` → `applyDraftDimensionFieldValue`) runs entirely client-side for live preview during line/circle/arc/rectangle construction. It only understood numeric input — typing a parameter name produced NaN, so the preview position never updated and stayed at the mouse-dragged location. Fixed by resolving parameter names against `document.parameters` in `handleDraftDimensionChange` before passing to `applyDraftDimensionField`: if the input matches a parameter name, its `resolved_value` is used for the numeric position computation and label display. The raw expression string is preserved for the persistent dimension editor path that takes over after commit.

**Draft dimension Enter exits line tool.** Pressing Enter in the draft dimension input commits the line, then switches the sketch tool to "select" mode — matching how industry-standard CAD programs end the line command when dimensions are explicitly typed. Mouse click (without Enter) still commits and keeps the line tool active for chained drafting.

**Double-click to break line chain.** Industry-standard CAD tools use double-click at the last endpoint to end the chain but stay in line tool (unchained) — the next click starts a fresh independent line. Two mechanisms: (1) zero-length line guard in the pointer-up handler — when the committed start and end are the same point (within 0.01 units), the line is dropped and the draft is cleared instead of calling `addSketchLine` with degenerate geometry. This prevents "Sketch lines must have non-zero length" errors. (2) `chainBreakRequestedRef` in `handlePointerDown` — tracks click timing and position; when two left-button clicks land within 300ms and 6px of each other while a line draft is active, the flag causes the pointer-up handler to clear the start instead of chaining. After either mechanism fires, the next click begins a fresh independent line.

**TODO: Make double-click timing configurable.** The 300ms threshold is hardcoded. It should be a user-facing setting so people with faster or slower click speeds can tune it to their preference.

**TODO: Right-click as "Enter" (configurable).** In industry-standard CAD, right-click often ends the active tool and returns to select mode. PolySmith should allow configuring right-click behavior (end tool / context menu / both) per tool context. Currently right-click only opens the context menu.

### Help System Foundation

**Help documentation per tool.** Created `help/line.md`, `help/circle.md`, `help/rectangle.md`, `help/parameters.md` — comprehensive user-facing documentation covering activation, interaction modes, dimension fields, keyboard shortcuts, parameter expressions, constraints, and internal implementation notes.

**Help index and popover component.** `apps/desktop-ui/src/lib/help-index.ts` exports a `helpRegistry` mapping tool IDs to structured `HelpEntry` objects (title, summary, sections, shortcuts, activation). `apps/desktop-ui/src/layout/HelpPopover.tsx` renders a floating popover with collapsible sections, a keyboard shortcut table with `<kbd>` styling, and minimal markdown-like rendering (**bold**, `code`). The popover auto-positions relative to an anchor element and flips if near viewport edges. Closes on outside click. Wired through a `?` hotkey.

**TODO: Wire HelpPopover into toolbar and status bar.** The component is built but not yet wired. Each toolbar button should show a small tooltip on hover (using `entry.summary` + `entry.activation`) and open the full popover on long-press or `?` key. The status bar should show the current tool name and be clickable to open help. Future workspace contexts (Slicer, CAM, Drawing) will need their own help entries registered in the helpRegistry.

**TODO: Workspace-aware help.** The top-left workspace dropdown ("Slicer" → future: "CAM", "2D Drawing", etc.) switches toolbars and the main viewport. Help entries should be contextualized per workspace so the help system shows relevant content for the active workspace.

**Dimension editor — `inputMode`.** Changed the persistent dimension editor's `inputMode` from `"decimal"` to `"text"` so `*` and letter characters can be typed for formula expressions.

**Dimension interaction model.** Changed from single-click-immediately-edits to a proper CAD convention:
- First click selects the dimension (highlights it), no editor
- Second click on the same dimension opens the inline value editor
- Right-click on any dimension shows a context menu with "Delete"
- Added `lastClickedDimensionRef` so rapid re-clicks work before the IPC selection round-trip completes
- Added `dimensionId` field to `ViewportContextMenuState` type
- Added `handleDeleteDimensionFromContextMenu` handler
- Added `handleDimensionClick` (renamed from `selectSketchDimensionForEdit`) with select-then-edit logic

### On-Demand Sketch Dimensions

#### Goal
Shapes created by mouse-only drag get **no automatic dimension**. Shapes created with a **typed value** (draft dimension input during preview) keep the dimension. Prevents sketch bloat and over-constraining.

#### Approach
Keep C++ auto-dimension creation, then delete from TypeScript when the user didn't type.

#### C++ Core
- `sketch_feature.h/cpp`: added `delete_sketch_dimension(FeatureEntry&, const std::string&)` — finds dimension by ID, erases from vector, refreshes derived state. Silently ignores missing dimensions (construction lines don't get auto-dims).
- `document.h/cpp`: added `DocumentManager::delete_sketch_dimension` wrapper with undo push, redo clear, core call, geometry revision bump.
- `app.cpp`: registered `delete_sketch_dimension` command handler reading `dimension_id` from payload.
- `serialization.cpp`: added `polygons` array to sketch parameters serialization and deserialization (was missing, causing `TypeError` on TS side).

#### TypeScript — IPC & Types
- `protocol/schema/commands.schema.json`: registered `delete_sketch_dimension` command.
- `types/ipc.ts`: added `DeleteSketchDimensionCommand` interface and `CoreCommand` union entry.
- `types/geometry/sketch.ts`: added `SketchPolygonEntry` type, `polygons` field to `SketchFeatureParameters`, `"polygon_radius"` to `SketchDimensionEntry.kind` union.
- `lib/ipcProtocol.ts`: added `makeDeleteSketchDimensionCommand(dimensionId)`.
- `hooks/useCadCore.ts`: added `deleteSketchDimension` hook.

#### TypeScript — Viewport Logic
- `App.tsx`: passed `onDeleteSketchDimension` callback to `ViewportPanel`.
- `ViewportPanel.tsx`:
  - Added `pendingDimensionDeletionRef`, `deleteSketchDimensionRef`, and `onDeleteSketchDimension` prop.
  - Created centralized `scheduleDimensionDeletion(tool, preCapturedSession?)` helper — single source of truth for the dimension deletion decision. Checks `lockedFields` on the draft session; uses `diameter` for circle (not `radius`, since the circle draft UI only has a diameter field).
  - Post-commit `useEffect` watches `sketchFeature` and deletes auto-dimensions for lines, circles, polygons, and rectangles using the **last entity** in the array (avoids stale-baseline race condition).
  - Set `scheduleDimensionDeletion` in ALL commit paths: `commitDraftDimensionSession`, line chaining snap path, all rectangle/circle/polygon snap variants, and the rectangle corner-corner/center-point snap path that was previously missing.
  - Added `!is_construction` guards so construction geometry (which has no auto-dim) isn't targeted.

#### Dimension ID Format
Auto-dimensions use `dim-<prefix>-<entity_id>`: `dim-line-line-1`, `dim-circle-circle-1`, `dim-polygon-polygon-1`.

#### Bugs Encountered and Fixed

1. **`polygons` missing from C++ serialization** — the `polygons` field was in the C++ struct but never serialized to JSON. TS access to `params.polygons.length` threw `TypeError`, crashing the commit function before the IPC was sent. Fixed by adding `polygons` to both `to_payload` and `from_payload` in `serialization.cpp`.

2. **Circle `lockedFields.radius` doesn't exist** — `draftSessionFields("circle")` returns only `["diameter"]`. Checking `!session.lockedFields.radius` was always true (radius is never locked), but the actual relevant field is `diameter`. Fixed by checking only `lockedFields.diameter` for circles.

3. **Null session for click-based commits** — for click-based creation (2-point circle, etc.), `draftDimensionSessionRef.current` can be null. The guard `dSession?.tool === "circle"` failed when `dSession` was null. Fixed by removing the tool check and using `!dSession?.lockedFields.<field>` directly — with null session this evaluates to `!undefined` = `true`.

4. **Line chaining used wrong session** — in the line chaining snap path, `draftDimensionSessionRef.current` was replaced with a fresh session BEFORE reading `lockedFields`. The new session always has `lockedFields: {}`, so deletion never triggered. Fixed by capturing `oldSession` before creating the replacement session and passing it as `preCapturedSession` to `scheduleDimensionDeletion`.

5. **Stale `fromLineCount` race condition** — the baseline entity count was captured from `sketchFeature` during the event handler, which reflects the CURRENT React render. When clicking rapidly (chained lines), React hadn't re-rendered from the previous commit, so `fromLineCount` was stale, targeting the wrong entity. Fixed by using the **last entity** in the array (`array[array.length - 1]`) instead of a baseline count.

6. **Split commit paths** — shape commits can go through `commitDraftDimensionSession` (Enter/submit) OR the snap handler in `handlePointerUp` (click-based). Initially only `commitDraftDimensionSession` was handled, causing clicks to silently bypass deletion. Fixed by calling `scheduleDimensionDeletion` from EVERY commit path. The centralized helper prevents drift between paths.

#### Documentation
- `Sketch-Tool-Implementation`: comprehensive guide for implementing new sketch tools, covering all files to touch, the two commit paths, dimension deletion integration, and all pitfalls discovered.

## 2026-05-19

### Sketcher UI Overhaul — Split Buttons, Rectangle/Circle/Polygon Variants

#### UI Framework
- created reusable `SplitToolButton` component with icon + chevron dropdown for tool variant selection
- redesigned SketchToolbar: rectangle (2-point / center-point / 3-point), circle (center+radius / 2-point / 3-point / tangent stubs), arc (3-point / center+start+end), polygon (circumscribed / inscribed / edge)
- added `PolygonIcon` SVG to ToolBarIcons

#### Polygon Tool (full-stack)
- C++ core: `SketchPolygon` struct in `feature.h`, `add_sketch_polygon` in `sketch_feature.cpp` with inscribed / circumscribed / edge math, `DocumentManager` wrapper, viewport primitive builder `make_sketch_polygon_primitive` computing world-space corners
- IPC: `add_sketch_polygon` command registered in `commands.schema.json`, dispatched in `app.cpp`
- TS types: `PolygonToolMode`, `ViewportSketchPolygon`, `SketchPolygonScene`
- Three.js rendering: `buildSketchPolygonObject` in `viewport.utils.ts`, closed `THREE.Line` loop from corner arrays
- Serialization: `to_payload` for `ViewportSketchPolygonPrimitive` + viewport state emission in `serialization.cpp`
- Zod schema: `sketch_polygons` field with `.default([])`, `active_sketch_tool` and `active_tool` enums extended with `"polygon"`
- Polygon radius dimension: `polygon_radius` kind created in `add_sketch_polygon`, emitted by viewport builder
- Polygon orientation: click-aligned for all three modes
- Viewport event handling: polygon tool added to `handlePointerDown` (sets lineDraftStartRef) and `handlePointerUp` (draft commit), also handled in drag session fallback
- Tool validation: `is_supported_sketch_tool` and `validate_tool` extended with `"polygon"`

#### Circle Tool
- 2-point circle: center = midpoint, radius = half distance between two diameter endpoints
- 3-point circle: circumcenter computed from three points via perpendicular bisectors
- tangent circles: dropdown entries reserved for future core support

#### Rectangle Tool
- 3-point rectangle: three-click flow — corner, second corner of first edge, perpendicular offset point

#### Bug Fixes
- `document.cpp` polygon handler was setting `active_sketch_tool = "circle"` (copy-paste error) → corrected to `"polygon"`
- `serialization.cpp` was missing `sketch_polygons` JSON emission → polygons invisible in viewport

#### Documentation
- `ai-cad-command-language.md`: documented `add_sketch_polygon` with all three modes
- `ipc-protocol.md`: added polygon command reference + viewport_state entry
- updated `sketcher-improvements.md` and `sketcher-progress-report.md` (working notes)

## 2026-04-16

### Architecture and Protocol

- established a strict JSON IPC path between UI and native core
- moved native CAD startup, document handling, and protocol logic out of `main.cpp`
- moved Tauri process management and protocol forwarding out of `main.rs`
- documented roadmap, protocol direction, and onboarding updates in `wiki/`

### Core Document Foundation

- added document creation and document state queries
- added session state queries
- made the native core the source of truth for document and feature history state

### First Modeling and Viewport Slices

- added `add_box_feature`
- added `update_box_feature`
- added viewport snapshots derived from core-owned feature data
- rendered viewport state in the UI as a lightweight SVG wireframe preview

### Selection and Editing Loop

- added core-owned feature selection
- synchronized selection between feature browser, timeline, and viewport
- added rename and delete behavior for features
- added undo and redo in the native core through snapshot history
- added a second primitive, `cylinder`, through the core, protocol, viewport, and UI forms

### UI Rebuild

- adopted the `Midnight Carbon` design direction from [Design-System](Design-System)
- added Tailwind-based styling and font setup
- rebuilt the desktop shell into a more CAD-like workspace layout
- introduced a top mode header, floating command bar, side panels, and bottom feature timeline

### Current Focus

- replaced the SVG debug viewport with a focused `three`-based 3D renderer spike in the UI
- added a renderer-facing scene adapter so the viewport still renders strictly from core-owned snapshot data
- kept viewport selection on the existing IPC feature-selection flow while adding orbit, pan, and zoom controls
- extended the viewport snapshot with core-owned primitive centers and scene bounds for renderer-facing camera framing
- added fit-view and hover feedback to make the 3D viewport feel more usable without moving behavior into React
- added core-owned origin reference planes and axes so the viewport can render CAD-style construction geometry
- added reference-plane selection and a first `start_sketch_on_plane` command stub to establish the sketch entry flow
- turned sketching into a real first-pass workflow by creating core-owned sketch features on planes
- added a first sketch entity, `line`, created from two viewport clicks and rendered back from the core snapshot
- extended sketching with core-owned tool state, viewport snapping, rectangle and circle creation, and selectable sketch entities
- added minimal inferred horizontal and vertical line hints in the core as a lightweight first step toward sketch constraints
- split sketch mode into explicit drawing and selection behavior, including chained line creation and `Escape` to return to selection mode
- added editable sketch lines and circles in the inspector, visible sketch points in the viewport, and first explicit horizontal/vertical line constraints
- added derived closed sketch profile detection in the core plus selectable profile overlays in the viewport
- added a first `extrude_profile` command that turns rectangular profiles into box-like extrudes and XY circular profiles into cylinders
- widened closed-profile detection from rectangle-only cases to arbitrary clean closed line loops and rendered those extrudes as polygonal prisms in the viewport
- added core-derived sketch dimensions to the viewport so active sketches now show line lengths and circle radii directly in the 3D sketch view
- promoted sketch dimensions into core-owned sketch data and added a first `update_sketch_dimension` IPC command for driving line length and circle radius
- kept the viewport on core-owned dimension snapshots while adding a minimal inspector path to edit those driving values from the UI
- added explicit core-owned sketch dimension selection plus a `select_sketch_dimension` IPC path so viewport overlays can be selected directly and highlighted
- added a first in-viewport dimension editor with autofocus and keyboard-friendly editing while reducing duplicate inspector dimension controls during viewport editing
- added a first solver-lite propagation pass for connected sketch lines so moving or redimensioning a shared endpoint also updates coincident neighbors and re-applies horizontal/vertical line constraints through that chain
- stabilized shared sketch corners further by snapping added, edited, constrained, and dimension-driven line endpoints onto existing coincident endpoints in the core before propagation runs
- promoted sketch line endpoint connectivity from coordinate-only inference to explicit internal point ids so connected redimensioning and constraint propagation can follow stable shared topology
- added a first cross-line `equal_length` sketch relation in the core and re-applied it through the existing line edit, constraint, and dimension-driving flows
- moved sketch line constraints into the top ribbon with a dedicated sketch constraints section, keeping the UI as intent-only while the core owns constraint and relation state
- shifted sketch constraints to an armed click flow in the UI so horizontal, vertical, clear, and equal-length constraints are chosen from the ribbon and then applied by clicking one or two sketch lines in the viewport
- added core-owned viewport constraint markers for sketch lines and equal-length relations, including direct badge-click clearing through the existing constraint commands
- tightened equal-length propagation so relation-driven updates also re-propagate shared start-point changes caused by snapping or connected geometry
- added a first solver-lite `perpendicular` sketch relation with the same armed two-line workflow as equal length and core-owned perpendicular viewport markers
- kept perpendicular behavior intentionally narrow by preserving the driven line start point and length while rejecting setups that conflict with direct horizontal or vertical axis constraints
- added a first solver-lite `parallel` sketch relation with the same armed two-line workflow and core-owned parallel viewport markers
- kept parallel behavior intentionally narrow by preserving the driven line start point and length while rejecting setups that conflict with direct horizontal or vertical axis constraints
- added an explicit endpoint-only `coincident` sketch tool so the UI can pick real core-owned endpoint ids and ask the native sketch core to merge those points through IPC
- kept coincidence solver-lite by making it a direct point-id merge in the native core, which now preserves connected redimensioning behavior without introducing a separate removable point-constraint model yet
- added a focused `export_document` IPC spike that writes a real STEP file from core-owned feature history
- kept export in the native core by rebuilding solid-producing OCCT shapes there and returning a `document_exported` event to the UI
- generalize the selected-feature inspector beyond box-only editing
- keep viewport data derived from the native core
- continue expanding the design system while preserving architecture boundaries
- added core-owned planar solid-face snapshots for primitive picking in the viewport
- added `select_face` and `start_sketch_on_face` IPC commands so the UI can select a face and start a sketch from core-owned face identity
- kept face-to-sketch placement in the native core by deriving a sketch plane frame from the selected face before creating the sketch feature
- fixed the face-sketch relay so the UI now passes the core-emitted face plane frame back into `start_sketch_on_face`, which keeps sketch clicks and dimension labels on the chosen face
- preserved the sketch plane frame through closed-profile detection and extrusion so face-based profile overlays and extruded bodies stay on the selected face instead of snapping to a perpendicular origin plane
- promoted polygon profile detection to use core-owned shared sketch point ids as the primary topology source, which makes closed-loop detection more robust when coincident endpoints stay connected through redimensioning and point merges
- promoted sketch points into explicit core-owned records and added `select_sketch_point` so the viewport can select stable point ids through IPC instead of fabricating point state in React
- promoted sketch profiles into stored core-owned region data on the sketch feature so selection, viewport rendering, and extrusion read cached regions instead of re-deriving them ad hoc
- kept redimensioning and coincident merges on the native side while rebuilding stored sketch points and profiles after sketch edits, which makes closed loops more resilient under parametric edits
- added a minimal core-owned fixed-point relation with `set_sketch_point_fixed`, fixed-point viewport badges, and point-aware inspector actions so point selection now drives a real parametric edit flow
- kept fixed-point behavior solver-lite in the native core by preserving fixed flags through point rebuilds and coincident merges while driving line-length edits from the unfixed endpoint whenever possible
- added point-driven sketch editing through `update_sketch_point`, tightened fixed-point preservation across line edits and solver-lite relations, and refreshed profile-linked extrude parameters when source sketch profiles change
- added editable extrude depth through a core-owned `update_extrude_depth` IPC command and an inspector form so a selected extrude feature can be redimensioned without rebuilding from the source sketch
- adopted a contextual modeling select-action-preview-confirm pattern across the UI: hoverable solid faces with face hover highlighting, double-click on any face to start a sketch, world-aligned camera framing on sketch entry, a subtle frameless inline dimension input that auto-dismisses on Enter, and an `E` hotkey that triggers a floating extrude preview panel driving live `update_extrude_depth` previews with confirm/cancel; documented the pattern as binding in `Contextual-Modeling-Workflow` so future contributions follow the same flow
- stabilized sketch UX further: the camera now reframes only on sketch plane transitions instead of every viewport snapshot, the extrude preview panel debounces depth previews so the inspector no longer needs its own depth form, profiles can be selected and extruded outside an active sketch via relaxed `select_sketch_profile` and `extrude_profile`, finished sketches can be reopened through a new `reenter_sketch` IPC command, and the document browser was rebuilt as a CAD-style collapsible hierarchy with Origin / Sketches / Bodies categories, compact icon-and-name rows, and per-row plus per-category eye toggles that drive UI-side viewport visibility filtering
- fixed the live extrude depth bug at its root: the Tauri IPC bridge is fire-and-forget, so reading the document store immediately after `await sendCoreCommand(...)` returned stale state and the floating Extrude panel was driving `update_extrude_depth` on the wrong feature id; introduced an `awaitDocumentChange` helper on the store and used it to capture the freshly-created extrude feature id before opening the action panel
- shipped binary STL export through a new core `export_document_as_stl` (BRepMesh_IncrementalMesh + StlAPI_Writer), an `export_document_stl` IPC command, and an Export STL button in the header; the existing `document_exported` reply distinguishes formats via its `format` field
- added Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z, and Cmd/Ctrl+Y hotkeys for undo/redo at the App level, gated by typing-target detection and core-reported `can_undo`/`can_redo` so users can iterate without reaching for the header buttons
- refreshed `V1-Roadmap` to reflect actual project state and a tiered next-feature plan (cut extrude, save/load, fillet/chamfer with edge selection, etc.)
- shipped native document persistence: a new `document_from_payload` deserializer mirrors the existing `to_payload`, `DocumentManager::save_document_to_path` / `load_document_from_path` write and reload `.polysmith` JSON files, ID counters are restored from the loaded feature/sketch ids, undo/redo stacks are cleared on load, and new `save_document` / `load_document` IPC commands plus an Open and Save header pair complete the flow end-to-end
- added a contextual modeling Project sketch tool: a new core `face_geometry` module computes world-space face outlines for extrude features that carry a `plane_frame` (rectangle and circle profiles, base/top and four side rectangles), `DocumentManager::project_face_into_sketch` projects the outline onto the active sketch plane and inserts fixed-endpoint sketch lines (or a sketch circle for circular caps), and the SketchToolPanel exposes a "Project selected face" button gated by face selection. Polygon-extrude side faces and legacy box/cylinder features are explicitly rejected for v1.
- compacted the AppHeader: File and Edit buttons collapsed into dropdown menus while Start Core and the session indicator stayed inline, freeing horizontal space for upcoming feature buttons.
- introduced cut/join extrude and a true mesh primitive end-to-end. New core modules `feature_shape` (shared OCCT solid builders) and `body_compiler` walk `feature_history`, compose bodies via `BRepAlgoAPI_Fuse` / `BRepAlgoAPI_Cut` based on each extrude's new `mode` field (`new_body | join | cut`), and tessellate the resulting bodies with `BRepMesh_IncrementalMesh`. `viewport.cpp` emits a new `ViewportMeshPrimitive` array and suppresses legacy primitives whose feature ids were consumed by booleans, so we never double-render. `export.cpp` now delegates shape collection to the same body compiler so STEP and STL exports honor cut/join. The IPC schema gained a new `update_extrude_mode` command and the `extrude_profile` command accepts an optional `mode` payload field; `ExtrudePreviewPanel` exposes a New body / Join / Cut segmented control wired through `useCadCore.updateExtrudeMode`. The three.js renderer ships a `MeshScenePrimitive` variant that uploads the core's positions/normals/indices straight into a `BufferGeometry`. v1 always targets the most recent body for booleans; explicit body picking remains a follow-up.
- shipped edge selection plumbing as the first slice of Tier 2 (prerequisite for fillet/chamfer/measure). The body compiler's per-body OCCT shapes are walked with `TopExp::MapShapes(TopAbs_EDGE)` and each unique edge is sampled into a flat world-space polyline (`BRepAdaptor_Curve` + `GCPnts_QuasiUniformDeflection`, with straight segments staying as 2-point lines). The viewport state grew a new `edges: [{id, owner_body_id, kind, points[], is_selected}]` array with stable ids of the form `<owner_body_id>:edge:<index>`. `DocumentState.selected_edge_id` and `DocumentManager::select_edge` round-trip through serializer + IPC schema + `select_edge` command + TS types/zod/builders/`useCadCore.selectEdge`. The viewport renderer materializes each edge as a `THREE.Line` with `userData.edgeId`, picks them with a tightened `Raycaster.params.Line.threshold` ahead of face raycasts (so edges that sit on faces stay grabbable), and dispatches `selectEdge` on click. Selection clears alongside face selection on every other selection-mutation path so nothing else accidentally lights up. Vertex selection and fillet/chamfer remain follow-ups.
- shipped vertex selection plumbing alongside fillet & chamfer features, closing Tier 2's selection layer and landing the first real body-modifying ops on top of edges. Vertex selection mirrors edge selection: the body compiler's per-body OCCT shapes are walked with `TopExp::MapShapes(TopAbs_VERTEX)` and each unique vertex is emitted as `viewport_state.vertices: [{id, owner_body_id, position, is_selected}]` with ids of the form `<owner_body_id>:vertex:<index>`; `DocumentState.selected_vertex_id` and `DocumentManager::select_vertex` round-trip through serializer + IPC schema + `select_vertex` command + TS types/zod/builders/`useCadCore.selectVertex`; the renderer materializes each vertex as a small `THREE.SphereGeometry` mesh (`renderOrder = 2`, `depthTest = false` so it always reads through edges and faces) and the raycaster picks vertices first, then edges, then faces. Fillet and chamfer arrived as new body-modifying feature kinds: `FilletFeatureParameters { target_body_id, edge_ids[], radius }` and `ChamferFeatureParameters { target_body_id, edge_ids[], distance }` round-trip through `to_payload` / `feature_entry_from_payload` and are applied during body compilation via `BRepFilletAPI_MakeFillet` / `BRepFilletAPI_MakeChamfer` against the target body's edges (re-resolved via `TopExp::MapShapes(TopAbs_EDGE)` on the live body shape; failures degrade to the unmodified body so the document keeps rendering). New `create_fillet` / `update_fillet_radius` / `create_chamfer` / `update_chamfer_distance` IPC commands feed `DocumentManager::create_fillet` / `update_fillet_radius` / `create_chamfer` / `update_chamfer_distance`. The UI exposes contextual modeling hotkey `F` (fillet) and `C` (chamfer) on a selected edge: the core synchronously creates the feature with a 1mm default and the `EdgeOpPreviewPanel` (a single shared component parameterized by title and value label) drives live preview via the update commands and Confirm / Cancel (cancel = undo). Edge ids inside fillet/chamfer features are preserved from the moment the user picked them; the body compiler resolves them on-demand against the body's current shape, which keeps later parametric edits (e.g. depth changes upstream) from invalidating the feature unless the topology genuinely shifts.
- shipped explicit target-body selection for boolean extrudes. `ExtrudeFeatureParameters` gained an optional `target_body_id`; the body compiler honors it when the requested body still exists in the current accumulator and otherwise falls back to the most recent body, and the targeted body is bumped to the front of the "most recent" queue so subsequent default-target booleans build on top of it. New `update_extrude_target_body` IPC command (with `extrude_profile` also accepting an optional `target_body_id`) and matching builder/hook entries thread the choice end-to-end. `viewport_state.bodies: [{id, label}]` exposes the available bodies (root feature ids, human-readable labels) so the `ExtrudePreviewPanel` can render a "Target body" dropdown when there's more than one candidate; the in-progress extrude itself is filtered out of the option list. Single-body workflows still see no picker and keep working unchanged.

## 2026-05-06

### Sketch Arcs (v1)

- shipped a first-class sketch arc primitive end-to-end. New core `SketchArc` struct stores `start_point_id` / `end_point_id` plus cached `(center, radius, ccw)` and the world-space endpoint coords; arc endpoints participate in the shared SketchPoint graph (rebuilt with `kind="endpoint"` and `is_fixed=true` for v1). `add_sketch_arc` constructor reuses `find_coincident_endpoint` so arcs and lines share endpoint ids when their endpoints meet — required for closed-loop profile detection.
- arc creation supports two contextual modeling modes via a single `add_sketch_arc { start, end, anchor, mode }` IPC command:
  - `three_point`: anchor lies on the arc; center is the circumcenter of (start, anchor, end). Default mode.
  - `center_start_end`: anchor is the center; end is snapped onto the resulting circle.
    Mode validation and degenerate-input rejection (colinear / zero radius) live in `DocumentManager::add_sketch_arc`.
- `sketch_profile.cpp` was generalized from a line-only walker to a `ProfileEdge` graph that walks lines and arcs uniformly. Arc edges sample interior points into `profile.points` so OCCT extrudes the resulting polygon as a clean curved boundary; sample order inverts when an arc is traversed end→start. New regression test `test_detects_polygon_loop_with_line_and_arc_edges` in `cad_core_sketch_profile_test` covers a stadium-half closed loop (3 lines + 1 semicircle).
- viewport emission added: `make_sketch_arc_primitive` ships a `ViewportSketchArcPrimitive` with world-space start/end/center, radius, ccw, plus the standard `is_selected` / `is_construction` / `is_preview` flags. The UI's `buildSketchArcObject` projects those world coords back into the sketch plane's local 2D frame to recover start/end angles, then samples 64 segments along the swept direction (matching `buildSketchCircleObject`'s resolution). Same plane-frame resolution as circles — face-based sketches use the live frame, ref-plane sketches fall back to the legacy axis mapping.
- UI tool flow: arc placement is three clicks. First click reuses `lineDraftStartRef` (so the start-snap pipeline is shared with line/rect/circle); second click stores the end in `arcSecondPointRef`; third click resolves the anchor and dispatches `addSketchArc`. State resets on tool switch and Escape. `SketchToolbar` enables the Arc button and renders a 3-point | Center segmented control (visible only while the arc tool is active); `arcToolMode` is owned by `App.tsx` and threaded through `AppHeader`.
- v1 limitations (deliberate): arc endpoints are `is_fixed=true` so they can't be dragged after creation — delete + redraw to reshape; no tangent / coincident / equal-radius constraints involving arcs; no construction-arc toggle in the UI yet (the kernel honors the field); Mirror tool ignores arcs; profile sampling uses a fixed `kArcSampleSegments=16`. Reshape, constraints, and dimension drive are natural follow-ups.

### Lessons for Future Agents — Schema Drift

While shipping arcs, three classes of bug surfaced in the Zod schemas at the IPC boundary; worth flagging because they will recur for every new entity / tool added in the future.

- `apps/desktop-ui/src/lib/schemas/ipcSchema.ts` is the source of truth on the UI side. **Zod silently strips object keys that aren't in the schema**, even when the underlying TS interface declares them, and the React tree will then crash on a `.filter()` over the resulting `undefined`. New viewport / document fields must be added to the schema _and_ the TS interface together. Always default new array fields to `[]` so older cores parse cleanly.
- Zod **rejects enum values not in the union** rather than passing them through. New `SketchTool` values, profile kinds, dimension kinds, etc. must be added to every relevant `z.enum([...])` site (often two: document-level and per-feature). When the enum reject happens on a _response_ (not a command), the UI silently keeps its previous state — which manifests as "the button click did nothing" rather than as a visible error.
- New sketch tool strings must also be added to the C++ allowlist in **two places**: `is_supported_sketch_tool` in `core/document.cpp` and `validate_tool` in `core/sketch_feature.cpp`. The first guards the IPC command boundary; the second guards the feature-level state mutation.

When adding a new entity, grep `ipcSchema.ts` for the union literally and walk every match. A future cross-checker test that diffs C++ `to_payload(...)` output keys against the Zod schema would prevent the strip-on-validate failure mode entirely; out of scope for now but flagged.

### Parametric Sketch Fillets (v1)

- shipped a parametric corner fillet for 2D sketches end-to-end. New core `SketchFillet` struct on `SketchFeatureParameters` carries `corner_point_id`, cached `corner_x`/`corner_y`, the two filleted line ids, the generated `trim_a_point_id` / `trim_b_point_id` / `arc_id`, and the user-controlled `radius`. Each fillet "owns" its generated arc and trim points but its inputs are still real `SketchLine`s — the recompute pass keeps them in sync.
- recompute is hooked into the existing post-edit pipeline: `enforce_sketch_fillets` runs inside `refresh_sketch_derived_state`, after the anchor / tangent passes (so it sees the latest line endpoints) and _before_ `rebuild_sketch_points` (so the rebuilt points table reads the up-to-date arc / trim coords). For each fillet it solves the virtual corner = intersection of the two filleted lines extended through their trim endpoints, recomputes trim distance `d = r / tan(theta/2)` and arc center along the angle bisector at `r / sin(theta/2)`, and writes the new geometry back onto the lines / arc. If the configuration becomes invalid (lines parallel, radius too large, near-180° angle) the pass _skips_ that fillet rather than corrupting partial state — the previous frame's geometry survives and the user can drag the lines back into validity. The fillet is preserved either way.
- `add_sketch_fillet` validates strictly (corner shared by both lines, lines distinct + non-parallel, radius fits, no other fillet already at this corner), allocates two new fixed `SketchPoint`s + a generated `SketchArc`, mutates each line's filleted endpoint to reference the new trim point, and records the fillet. `update_sketch_fillet_radius` is a one-liner that rewrites `radius` and re-runs the recompute. `delete_sketch_fillet` reads the fillet's cached corner coords and restores each line's filleted endpoint back to `corner_point_id` before removing the generated arc + the fillet record (the trim points fall off via the next `rebuild_sketch_points` pass).
- `corner_x`/`corner_y` are denormalized onto the fillet specifically so `rebuild_sketch_points` can re-emit the corner point even after `add_sketch_fillet` has mutated both lines to reference trim points instead. Without this cache the corner would silently drop out of the points table the moment no other entity referenced it (e.g. a 4-corner rectangle), and `delete_sketch_fillet` would have nowhere to read the restore coords from. The cache is refreshed by `enforce_sketch_fillets` on every recompute so the round-trip works under arbitrary line edits.
- IPC: `add_sketch_fillet`, `update_sketch_fillet_radius`, `delete_sketch_fillet` route through `app.cpp` to matching `DocumentManager` methods. `is_supported_sketch_tool` and `validate_tool` both gained `"fillet"`. Document state round-trips `feature_history[].sketch_parameters.fillets[]` (with all eight fields including the cached corner coords) so save / load preserves the parametric model. Tests in `cad_core_sketch_profile_test` cover create+trim, radius update re-deriving geometry, delete restoring the corner, and the oversized-radius rejection path.
- UI: new `SketchFilletEntry` interface + `fillets` field on the document state TS interface and Zod schema (`.default([])` so older cores parse cleanly). New `makeAddSketchFilletCommand` / `makeUpdateSketchFilletRadiusCommand` / `makeDeleteSketchFilletCommand` builders + `addSketchFillet` / `updateSketchFilletRadius` / `deleteSketchFillet` thin wrappers in `useCadCore`. `"fillet"` added to the `Shape2D` union (and therefore the `SketchTool` union) plus both `active_sketch_tool` enums in the Zod schema and both C++ allowlists. The Fillet button in `SketchToolbar` is enabled and shares the existing `FilletIcon` with the 3D fillet feature — the visual is identical and the tools are mutually exclusive (3D fillet requires an edge selection; 2D fillet requires an active sketch).
- Floating panel: new `SketchFilletPanel` mirrors `EdgeOpPreviewPanel`'s debounced-numeric-input pattern but is a separate component because the 2D fillet's input model is single-corner, not multi-edge. The `ViewportPanel`'s pointermove / click handler grew a `fillet`-tool branch that, on click, snaps to the nearest sketch point, finds incident non-construction lines, and rejects (silently) anything that isn't an exactly-two-line corner. Eligible clicks fire `addSketchFillet` with a 5mm default radius; `App.tsx` watches the next document update for a freshly added fillet and opens the panel pointing at its real id. Confirm closes; Cancel calls `deleteSketchFillet` to undo the create cleanly.
- v1 limitations (deliberate): only line-line corners (no line-arc, arc-arc); no live dashed pre-click preview while hovering corners (the create-then-edit flow gives a real-geometry preview as soon as the user clicks); no UI to edit an existing committed fillet (the panel only opens during the create flow — re-editing requires undo + redo for now); the default 5mm radius isn't smart-fit, so for tiny sketches the user must dial down before the core accepts the create. All natural follow-ups.

### Parametric Offset Construction Planes (v1)

- shipped a contextual modeling parametric offset construction plane end-to-end. New core `ConstructionPlaneFeatureParameters` struct on `FeatureEntry` carries `source_plane_id`, signed `offset` (mm), and a cached world-space `plane_frame`. New `core/construction_plane_feature.{h,cpp}` owns the creation / update / frame-derivation logic; the offset slides the source frame along its own normal, leaving the basis vectors aligned with the source.
- accepted source kinds: origin reference planes (`ref-plane-xy/yz/xz`), other construction-plane feature ids (chained offsets), and planar body face ids of the form `<body_id>:face:<index>`. Resolution lives in a new public `resolve_plane_source_frame(document, source_id)` helper exposed from `core/refresh_dependents.h`. The walker in `refresh_history_dependencies` now re-runs every construction plane's frame in topological order before the sketch pass, so chained planes / face-source planes update cleanly under any upstream geometry edit.
- new IPC commands: `create_offset_plane { source_plane_id, offset }` validates the source via the resolver and pushes a fresh `construction_plane` feature; `update_offset_plane { feature_id, offset }` rewrites the offset and re-derives the frame from the current source. `update_offset_plane` deliberately does _not_ push an undo step (mirrors `update_fillet_radius`), so a single `undo()` from the panel's Cancel rolls back the whole session.
- viewport: extended `ViewportReferencePlane` with an optional `plane_frame` and an `orientation = "custom"` value. Construction planes get emitted alongside the three origin planes, keeping selection / active-sketch-plane highlighting plumbing unchanged. The renderer's `buildReferencePlaneObject` builds a 4x4 basis matrix from the frame and applies it to the `PlaneGeometry`, so any orientation lands at the right world position without per-axis special cases.
- sketch integration: widened `is_origin_plane_reference` callsites (`select_reference`, `start_sketch_on_plane`) to a new `is_selectable_plane_reference(document, id)` predicate. When the user starts a sketch on a construction plane the core resolves the plane's frame and threads it into the sketch as `plane_frame`, so existing face-based-sketch machinery (profile detection on the frame, face-based extrudes, dimension placement) handles construction-plane sketches with no further changes.
- parametric chain: `refresh_history_dependencies` now walks `construction_plane → sketch → extrude` in one pass. Editing the offset on a construction plane re-derives every downstream sketch's plane frame, and that propagates into the consuming extrude's `extrude_parameters.plane_frame` via the existing extrude-from-sketch step. End result: editing the construction plane's offset moves its sketches and their extruded bodies in lockstep, matching the request "if I make a plane, then I make sketches and then I go back to move the plane again, my sketch should move with the plane".
- UI: new `Offset Plane` button in the Construct ribbon (replacing the disabled placeholder); new `OffsetPlanePanel` component (contextual modeling two-phase pending → active flow, debounced numeric input, Enter / Esc handlers). App-level state mirrors the fillet/chamfer pattern: clicking the button with no plane / face selected opens the panel in pending phase and the next plane / planar-face click in the viewport calls `create_offset_plane` with the currently-typed offset; an already-selected plane / face short-circuits straight to active phase. The panel's `sourceSummary` uses friendly names ("XY plane", "Top face", or the construction plane's user-facing label) per the AGENTS.md UI Copy Rules — internal ids never reach the user.
- extended offset construction planes to accept sketch profiles as sources. `resolve_plane_source_frame` now resolves a profile id to its owning sketch plane, centered on the profile region, so both select-then-invoke and pending-panel profile clicks create parametric offset planes without adding a separate IPC command.
- added Midplane and Tangent Plane construction tools. `ConstructionPlaneFeatureParameters` now carries a `plane_type` plus source id list so offset, midplane, and tangent planes share the same feature kind and viewport rendering path. New IPC commands `create_midplane` and `create_tangent_plane` create planes from two parallel plane-like sources or a body face tangent frame respectively; dependency refresh re-resolves their sources on recompute.
- added Plane at Angle as another `construction_plane` variant. The feature stores a plane-like source, a linear axis source, and `angle_degrees`; the core re-resolves sketch-line axes and body line-edge axes during dependency refresh, then derives the cached frame by rotating the source normal around the axis. New IPC commands `create_angle_plane` and `update_angle_plane` drive the contextual Construct toolbar flow.
- added the first Shell body-modifying feature. `ShellFeatureParameters` stores the target body, removed face ids, thickness, and pending flag; body compilation applies OCCT `BRepOffsetAPI_MakeThickSolid` against the current target shape and falls back to the unchanged body on offset failure. The Modify toolbar now opens a contextual Shell panel with pending face pick, live `update_shell_thickness`, `confirm_shell`, and cancel-through-undo.
- hierarchy: `DocumentHierarchyPanel` gained a new `Construction` category between `Origin` and `Sketches`, listing every `construction_plane` feature with a parallelogram glyph (new `ConstructionPlaneIcon` in `header/ToolBarIcons.tsx`). Clicking a construction plane in the hierarchy dispatches `select_reference` (not `select_feature`) so it lights up in the viewport and the Sketch button enables. The Construction category participates in per-category visibility hides; the per-feature visibility toggle hides the construction plane in the viewport and any sketches anchored on it follow via the existing `hiddenSketchPlaneIds` grouping.
- protocol / schema / docs: added the two new commands to `protocol/schema/commands.schema.json`, documented them and the new `viewport_state.reference_planes[].plane_frame` field in `IPC-Protocol`, and updated `findDependents` so deleting a construction plane (or any plane it sources from) surfaces a "this will break N downstream features" prompt. Round-trip serialization for `construction_plane_parameters` is symmetric with the other feature kinds; older `.polysmith` saves load cleanly because the field defaults to null.
- v1 limitations (deliberate): no in-panel "Edit" entry from the timeline yet (the user undoes + redoes to revisit the offset for now); no Mid-plane / Axis / Point construct features (still placeholders); the panel's Cancel collapses to a single `undo()` which won't separately revert older live offset edits if the panel is reopened — acceptable because v1 only opens the panel during creation. All natural follow-ups.

### Construction Axes and Points (v1)

- added `construction_axis` and `construction_point` feature kinds. Axes are created from sketch lines or straight body edges; points are created from sketch points or body vertices. Both feature kinds store their source id plus cached world-space geometry for rendering.
- dependency refresh re-resolves construction-axis and construction-point sources against the live prefix history. Missing / changed sources mark the feature `dependency_broken` and keep the last cached geometry instead of crashing or trusting stale topology.
- viewport protocol now emits construction axes through `reference_axes[]` with `axis: "custom"` and emits construction points through `reference_points[]`. The hierarchy lists planes, axes, and points together under Construction, with per-feature visibility preserved.
- UI: the Construct ribbon Axis and Point placeholders are now active contextual pick tools. Axis accepts body edges and sketch lines; Point accepts body vertices and sketch points.

### Modal Project Tool (face / edge / vertex)

- reshaped Project from a one-shot face-projection action into a contextual modeling modal sketch tool. While active (toggled via the `P` button in `SketchToolbar` or the `P` hotkey), every viewport face / edge / vertex click is routed to `project_*_into_sketch` instead of the normal selection. The tool stays armed across clicks; toggling Project again, picking a different sketch tool, or pressing `Esc` returns to Select.
- new core helpers in `native/cad-core/src/core/edge_geometry.{h,cpp}`: `compute_edge_geometry(document, edge_id)` parses `<body>:edge:<index>`, recompiles the OCCT body, walks `TopAbs_EDGE`, and classifies the edge with `BRepAdaptor_Curve` — returns `{ kind: "line"|"circle"|"arc"|"unsupported", start, end, center, axis, radius }`. `compute_vertex_position(document, vertex_id)` does the same shape for `<body>:vertex:<index>`. Both share a `parse_topology_id` helper.
- `DocumentManager::project_edge_into_sketch` and `DocumentManager::project_vertex_into_sketch` join the existing `project_face_into_sketch` next to a new shared `require_projection_target` validator. Linear edges become locked sketch lines; circular edges that lie in a plane parallel to the sketch normal become sketch circles or arcs (using the body-axis vs. sketch-normal dot product to determine winding). Edges that would project to ellipses or whose curve type is unsupported (B-splines, etc.) are rejected with a structured error so the UI can surface a transient toast — matches the user's "skip + show message" choice.
- vertex projection introduces a new `SketchProjectedPoint` struct on `SketchFeatureParameters`. The Project tool appends an entry per click; `rebuild_sketch_points` re-emits each as a `SketchPoint` of kind `"projected"`, and a new `enforce_projected_points_fixed` pass forces them locked on every recompute (so the user can't drag derived geometry off its source). `refresh_sketch_derived_state` was promoted out of the anonymous namespace so the new code path can re-run the recompute pipeline after appending to the projected-points vector.
- idempotency: a single `projected_sources: vector<string>` records every face / edge id already projected onto the sketch so a second click on the same source is a no-op. Vertex projections check `projected_points[*].source_id` directly. All three project paths share the same id-tracking convention.
- IPC: two new commands `project_edge_into_sketch { edge_id }` and `project_vertex_into_sketch { vertex_id }` in `commands.schema.json`, with matching builders, hooks, and `app.cpp` dispatch entries. `IPC-Protocol` documents the new commands, the supported-curve constraints, and the idempotency guarantees.
- UI: `SketchToolbar` lost its standalone "Project Face" button — Project now lives in the main tool row alongside Select/Line/Rectangle/Circle/Arc/Fillet, toggleable like any other modal tool. The `SketchTool` union grew a `"project"` member; the C++ `validate_tool` and `is_supported_sketch_tool` allowlists were widened in lockstep. The `P` hotkey toggles the tool instead of running a one-shot project. App.tsx intercepts the viewport's `onSelectFace` / `onSelectEdge` / `onSelectVertex` callbacks while the tool is active and dispatches the matching `project_*_into_sketch` IPC, with structured error messages routed to the existing message log. The `findDependents` walker still treats Project's outputs as derived geometry; nothing else needed updating because projected entities are stored as ordinary sketch lines / circles / arcs / points.
- renderer: standalone projected points get a slightly larger sphere in the Z-axis cyan so they read as derived geometry vs. user-drawn endpoints. The Zod schema and TS interfaces (`SketchPointScene`, `ViewportSketchPoint`, `SketchPointEntry`) all gained `"projected"` on their `kind` union; the schema for `sketch_parameters` ships `projected_points` and `projected_sources` fields with `default([])` so older saves keep loading cleanly.
- v1 limitations (deliberate): no live highlight differentiation while hovering with the Project tool active (the existing hover plumbing already lights up edges / faces / vertices, just not Project-specifically); curved edges whose plane isn't parallel to the sketch are rejected rather than projected as ellipses; projected geometry is "static" — moving the source body does not currently update the projection (matches the existing face-project semantics; live parametric link is a natural follow-up). Existing `project_face_into_sketch` polygon-side handling was unchanged; only the duplicate-click path it shares with the new methods was extended via `projected_sources`.

### Live-Linked Project Geometry (Contextual Modeling Re-Derivation)

- promoted Project from a "frozen at click time" tool to a live parametric link: every `project_face_into_sketch` / `project_edge_into_sketch` / `project_vertex_into_sketch` call now records a `SketchProjection` (new struct in `native/cad-core/src/core/feature.h`) carrying the body source id (`source_id`), its kind (`"face"|"edge"|"vertex"`), and the ids of every sketch entity it generated (`generated_line_ids`, `generated_circle_ids`, `generated_arc_ids`, `generated_point_id`). `SketchFeatureParameters` gained a `projections` vector to hold them; the legacy `projected_sources` ids-only field is kept for backwards-compatible deserialization but is no longer read at runtime.
- `bump_geometry_revision` (which fires after every mutator) drives the live-link path through `refresh_history_dependencies`. The sketch branch of the walker — right after the plane-frame resolution finishes — now compiles a fresh `prefix = document; prefix.feature_history.resize(i)` and calls a new `refresh_sketch_projections` pass. For each projection the pass re-resolves the source via `compute_face_outline` / `compute_edge_geometry` / `compute_vertex_position`, projects the resulting world-space geometry through the sketch's `plane_frame`, and patches the matching `lines[]` / `circles[]` / `arcs[]` / `projected_points[]` entries in place by id. After patching, `refresh_sketch_derived_state` runs so points, profiles, and dimensions pick up the new coords.
- end-to-end behaviour: editing a sketched line that was extruded into body B causes `refresh_history_dependencies` to recompile B in the same revision; when the walker reaches a _downstream_ sketch that projected one of B's faces / edges / vertices, the projection re-resolves against the freshly compiled B and the cached coords on its generated lines / circles / arcs / projected points are rewritten, so the projection follows the source through the chain. Same shape for chained construction-plane edits or any other upstream change that affects body topology.
- broken-source handling matches the rest of the dependency-broken machinery. `SketchProjection` carries its own `dependency_broken` + `dependency_warning`; if a source can't be re-resolved (deleted body, curve type that's no longer projectable, or the upstream face's vertex count changed and the captured `generated_line_ids` no longer cover the new outline) the per-projection flag is set, the generated geometry stays frozen on its last-known coords, and the parent sketch's feature-level `dependency_broken` / `dependency_warning` is set ("One or more projected entities reference a body source that no longer exists or changed shape. Re-project to restore the live link.") so the existing timeline warning button surfaces it. We deliberately don't auto-delete the generated entities — the user picks up the broken state in the timeline and either re-projects or removes the sketch entries manually.
- idempotency now walks `projections[*].source_id` instead of `projected_sources`. All three project methods share the same lookup, so face / edge / vertex sources can never collide on a duplicate id even though they share the same dedup vector. Vertex projections still carry their canonical record on `projected_points[]` (for the cached x, y) but their live-link record sits on `projections` like the other two; `refresh_sketch_projections` patches `projected_points[]` by `generated_point_id`.
- generated-id capture pattern: each project path snapshots `lines.size()` / `circles.size()` / `arcs.size()` immediately before its `add_sketch_*` calls and walks the trailing range to collect the new ids. That way the projection record only ever points at entities the same call actually emitted, and the standard `add_sketch_*` mutators don't need awareness of projections.
- serialization round-trips the new struct. `protocol/serialization.cpp` writes `projection_id`, `source_id`, `source_kind`, `generated_line_ids`, `generated_circle_ids`, `generated_arc_ids`, `generated_point_id`, `dependency_broken`, and `dependency_warning` under a new `projections` array on each sketch's `sketch_parameters` payload; the reader is gated behind a `payload.contains("projections")` guard so older `.polysmith` saves load cleanly (those documents lose the live link until the user re-projects, which is the previous behaviour). `load_document_from_path` extends the id-counter restoration loop to bump `next_sketch_projection_id_` past every loaded projection id.
- UI plumbing: `SketchFeatureParameters` (the TS interface) gained a `projections: SketchProjectionEntry[]` field with the same shape as the core struct; `lib/schemas/ipcSchema.ts` added a matching Zod object inside `sketch_parameters` with `default([])` so messages from a pre-live-link core also parse. The UI doesn't dispatch projections directly — the records ride along in the document state for round-trip + future "fix broken projection" UX. `IPC-Protocol` documents the new field, the live-link semantics, the `dependency_broken` flag, and the implicit migration story.
- v1 limitations (deliberate): polygon faces with a _different_ vertex count after upstream edits surface as broken rather than rebuilding the entity set (re-projecting solves it); circle / arc edges that flip their plane orientation (axis dot product changes sign) update CCW correctly but won't try to recover from "circle became a B-spline" — that path also flags broken; projection-broken sketches still render their last-known geometry so the user has something visible to grab. Wiring the live link through `findDependents` (so deleting an upstream feature warns about projection-broken downstream sketches) is a natural next step but not strictly required since the core already surfaces the broken state on the next recompute.

### Split Sketch Profiles, Hole Regions, And Multi-Profile Extrude

- started the multiple-face extrusion milestone by replacing the line-only profile detector's connected-component assumption with a planar line arrangement pass. Straight sketch lines are split at intersections and traced into bounded faces, so a rectangle cut by a crossing line now emits the two resulting selectable profile regions instead of rejecting the high-degree graph.
- added first hole profile semantics. When a sketch circle or nested closed polygon lies inside another polygon profile, the containing region now carries an `inner_loops[]` hole and the inner shape still emits as its own selectable profile. Selecting the outer region extrudes the outer area minus the inner shape; selecting the inner region extrudes only that inner shape; selecting both explicitly extrudes the full filled area.
- widened sketch profile selection to an ordered list. `select_sketch_profile` now accepts `additive=true` for Ctrl/Cmd/Shift-click toggling, document state emits `selected_sketch_profile_ids[]`, and the viewport highlights every selected profile while keeping the legacy `selected_sketch_profile_id` as the most recent selection for compatibility.
- widened `extrude_profile` to accept `profile_ids[]` while keeping the existing single `profile_id` payload working. The core validates all selected profiles come from the same sketch plane and creates one extrude feature with all selected regions. Multiple circular profiles are approximated as polygon loops for this first slice; single-circle extrudes still use the existing cylinder path.
- made the Create ribbon Extrude command invokable without a preselected profile or face. The UI now opens the Extrude panel immediately, shows the selected face count, and keeps profile clicks in toggle-selection mode while the panel is pending so the user can pick as many faces as needed before confirming. Confirm creates the native extrude through the new `profile_ids[]` payload.
- protocol/schema/UI types now round-trip `inner_loops[]`, `profile_ids[]`, and `selected_sketch_profile_ids[]`. The viewport renderer applies profile holes to the translucent selectable profile meshes, so the pick surface matches the core-owned region data.
- tightened the Extrude action flow so clicking Extrude / pressing E opens the panel in a pending state, then the first profile selection creates the native preview immediately. While the preview is active, additional profile selections dispatch `update_extrude_profiles { feature_id, profile_ids[] }`, preserving depth / mode / target and keeping the face count panel in sync with core-owned selection.
- fixed the legacy `polygon_extrudes` preview path to carry `inner_loops[]` into the scene object, geometry cache key, and `THREE.ExtrudeGeometry` holes. This keeps nested-profile previews from rendering the removed filled outer profile when the core emits an outer region with a hole.
- moved hole and multi-profile extrude previews onto the native tessellated mesh path instead of the legacy polygon primitive path, and keyed mesh rebuilds by buffer content rather than only vertex/index counts. This keeps ring walls / top faces visible and makes live preview updates invalidate correctly when depth or selected profiles change without changing topology size.
- made the viewport scene merge sketch profile regions from the core-owned document snapshot when they are missing from the viewport snapshot, so disconnected nested loops become selectable immediately after the sketch edit that created them instead of waiting for a later intersecting edit to refresh the viewport profile list.
- restyled the orbit view cube to use the Midnight Carbon palette instead of axis-colored faces, and added sketch-only 2D rotation arrows in the cube overlay. The arrows appear only when the camera is aligned to the active sketch plane and rotate the camera roll around that plane normal by 90 degrees without changing the sketch target.
- replaced the finite `THREE.GridHelper` floor with renderer-owned dynamic line grids that recenter around the camera target and step through millimetric spacing thresholds as the camera zooms. Sketch mode now also renders a matching back grid on the active sketch plane, giving the upcoming grid-snapping work a visible threshold ladder without moving any snapping/modeling state into React.
- extended the dynamic grid coverage from camera distance so shallow views keep the back grid filled across the viewport, and made sketch dimension labels / constraint badges screen-scaled sprites so they no longer balloon when the camera zooms close to the sketch.
- switched the main viewport camera to orthographic projection so cube face snaps and sketch-plane views read as true 2D CAD views. Cardinal view snaps now render the grid on the viewed plane instead of showing a receding floor grid, and active sketch mode suppresses the world floor grid so only the sketch-plane grid remains.
- reduced the screen footprint of sketch dimension labels and constraint badges in orthographic views, and added a renderer-only collision nudge for constraint badges that would otherwise overlap dimension labels.
- stabilized orbit-cube cardinal snaps by quantizing camera roll to the nearest 90-degree orientation for the clicked cube direction, and made the cube roll arrows available on all six cardinal face views.
- moved the desktop app's `cad_core` executable lookup behind generated Tauri build metadata, keeping dev builds pointed at the workspace core while release builds resolve the bundled resource path through Tauri.
- fixed circular-profile extrudes by forcing them onto the native OCCT mesh viewport path instead of the legacy world-Y cylinder primitive, so cylinders from face sketches render on arbitrary sketch planes.
- bound Escape in non-sketch 3D mode to the existing core-owned `clear_selection` IPC command so feature, face, edge, vertex, and reference selections can be dismissed from the keyboard.
- blocked deleting the sketch that is currently being edited: the UI now gives immediate feedback from hierarchy/timeline delete commands, and the native document mutator rejects the same `delete_feature` intent before state changes.
- changed default naming for extrusion-created standalone bodies so `new_body` extrudes appear as "Body", while join/cut extrusion features keep the "Extrude" action name.
- made face-based extrusion workflows default to Join by deriving the source body from the selected body face or from a sketch profile whose sketch plane is a body face; other sketches still default to New body.
- tuned viewport rendering by depth-biasing face overlays, drawing sketch geometry as an overlay so face-based drafts do not sink into body faces, and suppressing generated cylinder / native-mesh facet wireframes while recomputing mesh normals for smoother circular bodies.
- added draft dimension boxes for line, rectangle, and circle creation. While dragging, the viewport shows live editable dimensions (line length, rectangle width/length, circle diameter), supports Tab between rectangle fields, and Enter commits through the existing sketch IPC commands. Draft points now snap to the currently visible grid spacing when no stronger sketch-entity snap wins.
- tuned draft grid snapping to engage only within a small screen-space distance of the visible grid lines, and return focus to the canvas after committing a draft shape so the transient dimension field does not stay active.
- changed circle dimensions to present diameter in the viewport and editor (`D ... mm`) while preserving the core's existing radius-backed dimension storage and update path.
- moved sketch dimension presentation closer to contextual modeling drafting: default labels now render borderless until selected for editing, line dimensions draw arrowheads with slight extension-line overruns, rectangle sketches only display one width and one height dimension by default, and dimension label positions can be dragged closer/farther along the dimension normal as renderer-local presentation offsets.
- aligned sketch dimension labels to their dimension arrow line in screen space, and removed the boxed borders from sketch constraint glyphs.
- offset dimension labels a few pixels away from their dimension line, removed the selected-dimension label border state, and gave relation constraints display priority over basic horizontal / vertical badges.
- exposed the Dimension sketch tool in the toolbar next to Line, kept `D` as its hotkey, and expanded the flow so line / circle picks open focused dimension editing, second-line picks create angle dimensions, and circle-center to line / circle-center distance dimensions are created through a new core-owned `add_sketch_distance_dimension` IPC command.
- fixed the second-line Dimension tool path by routing connected line pairs to angle dimensions and separate parallel line pairs to core-owned line-to-line distance dimensions instead of sending every pair through the angle-only command.
- refined the Dimension tool placement/edit loop: after a two-pick dimension is created, the label stays attached to the cursor along the same perpendicular placement-axis behavior used by automatic line / rectangle dimensions until the edit is confirmed or cancelled, including distance dimensions whose initial extension offset is zero. Focused dimension edits now send live `update_sketch_dimension` previews while preserving Escape restore to the original core-owned value.
- fixed line-to-line distance edits so the driven line translates rigidly along the measured normal instead of skewing under endpoint-by-endpoint propagation, and cleared stale renderer-local dimension label offsets during live value edits so arrows re-center on the updated core geometry.
- adjusted Dimension tool placement so the second canvas click drops the dimension label but leaves the numeric editor focused, and live value edits preserve the placed dimension position instead of snapping the line back to its default location.
- corrected placed line-to-line dimension rendering so stored label placement preserves only the valid sideways offset; after a distance value changes, the dimension line and anchors are rebuilt between the updated sketch lines instead of keeping stale diagonal extension geometry.
- restored line-to-line distance extension lines by keeping anchors on the measured sketch lines while offsetting only the dimension bar/label, and changed angle dimensions to render as an arc between the two rays instead of a straight chord.
- refined angle dimension presentation so dragging changes the arc radius around the shared corner, live angle edits redraw that arc from the updated rays, and the degree label no longer inherits the linear-dimension chord rotation.
- fixed dragged angle dimension arcs by deriving the shared corner from the intersection of the two emitted angle rays instead of assuming the core's default arc radius, so resized arcs stay attached to the real sketch corner.
- made angle-dimension dragging size the arc from the cursor's radial distance to the shared corner, so the arc follows the mouse closely while the degree label remains cleanly placed on the angle bisector.
- adjusted angle-dimension dragging so the stored placement follows the cursor's actual radial direction from the corner, putting the outer arc under the mouse instead of forcing placement back onto the angle bisector.
- separated angle-dimension arc control from label placement: dragging now stores a cursor-following point on the outer arc perimeter while the degree label remains on the angle bisector closer to the corner.
- constrained stored linear-dimension label offsets to each dimension's valid perpendicular placement axis, preventing stale or off-axis offsets from skewing extension lines after edits or redraws.
- fixed nested circle profile detection by treating a circle that contains smaller circles as a polygon-style ring profile with sampled circular inner loops, while still emitting each inner circle as its own selectable profile. Plain circles without holes remain circle profiles.
- changed circular sampled loops back to the previous lightweight sampling budget and taught native extrusion shape building to recognize those loops as circles, emitting analytic circular OCCT wires for ring / circular-hole profiles instead of segmented topology.
- carried sketch plane frames on circle / arc viewport primitives so projected or face-based sketch circles render in the correct 3D plane even when their sketch is not the active sketch, and blocked dimension edits for projected circles because their diameter is driven by source geometry.
- added the first bottom-center viewport mini toolbar with a compact two-state grid button; it toggles both the dynamic three.js grid and the viewport grid-stage overlay without touching core CAD state. The grid control now uses the shared toolbar tooltip styling, theme-token hover colors, and a configurable `G` hotkey.
- fixed chained line drafting after the draft-dimension overlay landed: after a line segment commits, the transient preview session now resets its start to the segment's endpoint so the next preview matches the line that will actually be created.
- stopped click-to-finish sketch creation from leaving dimension inputs focused. Active line / rectangle / circle drafting still auto-focuses the transient draft field for immediate typed input, chained line drafting moves focus to each next segment's draft field, and commits suppress the persistent dimension editor's one-shot auto-open for generated dimensions; explicit dimension clicks still open the editor.
- routed Escape from the focused draft dimension input through the same sketch cancel path as the global Escape shortcut, so active line / rectangle / circle creation can be cancelled immediately without first blurring the input.
- replaced the old top-left Line options panel with a right-side sketch tool panel styled like the action panels. Line, Rectangle, Circle, and Arc now expose a construction toggle while active; Arc also exposes its creation mode there. The construction flag is sent through JSON IPC to the native core for line / rectangle / circle / arc creation, with construction circles and arcs owned by the core and excluded from profile detection.
- removed post-create construction conversion from the sketch tool panel so already-drawn geometry stays stable for downstream profiles / extrudes, and pinned the circle draft dimension box near the circle center instead of tracking the cursor.
- added sketch selection quality-of-life behavior: lines / circles / arcs and sketch vertices now highlight on hover, sketch vertices are directly selectable, and Ctrl/Cmd/Shift selection toggles multiple sketch entities or points through core-owned `selected_sketch_entity_ids[]` / `selected_sketch_point_ids[]` arrays.
- added sketch deletion through a new core-owned `delete_sketch_selection` IPC command. Delete / Backspace in sketch mode now removes selected sketch profiles, edges, or vertices, with profile / point deletion resolving to owned boundary or connected geometry in the native core; the core cleans related dimensions, constraints, anchors, projected links, and fillet-generated geometry, then recomputes profiles and linked extrudes. The UI shows the existing downstream-feature warning when the active sketch feeds later geometry before sending the delete intent.
- tightened dependent-geometry handling for sketch deletion: the confirmation prompt now appears only when the selected sketch profile / edge / vertex feeds an existing extrude, and deleting that source profile leaves the downstream extrude in history with `dependency_broken` warning state so both the feature timeline and hierarchy surface a warning instead of showing a healthy stale body.

### Pre-existing Test Failure (Not Introduced By This Work)

`cad_core_sketch_profile_test :: test_fixed_endpoint_stays_put_when_redimensioning` was failing on `main` before any fillet work landed (verified by `git stash` + rebuild). The test references the point id `point-line-1-end` which the current `add_sketch_rectangle` code path no longer emits — likely a casualty of an earlier refactor of how rectangle endpoints share point ids with the unified line counter. The fillet PR's new tests still pass; they were verified by temporarily reordering them ahead of the broken test in `main()`. Worth chasing as a separate cleanup.

## 2026-05-23

### Dimension Tool — Driven Dimensions & Radius/Diameter Toggle

#### Goal
Eliminate the global radius/diameter display hack (where `dimensionToolMode` multiplied/divided all circle dimensions by 2). Make radius vs diameter a per-dimension property, add the driven/reference dimension concept, and fix constrained-line angle dimensions to be driven instead of suppressed.

#### Driven Dimensions

**C++ Core:**
- `feature.h`: `SketchDimension` gained `bool driven = false` and `std::string display_as` fields.
- `sketch_feature.cpp`: `update_sketch_dimension` returns early for driven dimensions (silent no-op — they don't drive geometry).
- `sketch_feature.cpp`: new `sync_driven_dimensions()` runs during `refresh_sketch_derived_state`, re-measuring driven dimension values from current geometry for all 8 dimension kinds (`line_length`, `circle_radius`, `polygon_radius`, `angle`, `line_angle`, `line_line_distance`, `circle_center_distance`, `circle_line_distance`).
- `serialization.cpp`: `driven` and `display_as` serialized/deserialized with backward-compat defaults (`false` / `""`).

**line_angle Fix:**
- Previously: `line_angle` auto-dimensions were *skipped* for axis-constrained lines (horizontal/vertical) — a workaround to avoid driving a fixed constraint.
- Now: `line_angle` dimensions are *always* created for non-construction lines. Axis-constrained lines get `driven = true` so the angle is displayed but cannot be edited. The constraint still governs; the dimension just reflects it.
- Changed in both `add_sketch_line` and `set_sketch_line_construction`.

#### Per-Dimension Radius/Diameter Toggle

**The Problem:** Previously, `dimensionToolMode` was a global switch that multiplied/divided *all* circle dimensions. Switching modes changed every circle dimension on screen. This was a display hack, not a real feature.

**C++ Core:**
- `add_sketch_circle_radius_dimension` now accepts optional `display_as` parameter (`""` = diameter, `"radius"` = raw radius). Defaults to `""` for backward compat.
- `document.cpp`: new `update_sketch_dimension_display(dimension_id, display_as)` mutation with undo/redo support. Guards to `circle_radius` kind only.
- `app.cpp`: new `update_sketch_dimension_display` IPC command handler reads `dimension_id` + `display_as` from payload.

**TypeScript:**
- `types/geometry/sketch.ts`: `SketchDimensionEntry` gained optional `driven?: boolean` and `display_as?: string`.
- `lib/schemas/ipcSchema.ts`: both fields added to Zod schema with `.default(false)` / `.default("")`.
- `lib/ipcProtocol.ts`: `makeAddSketchCircleRadiusDimensionCommand` accepts optional `displayAs`; new `makeUpdateSketchDimensionDisplayCommand(dimensionId, displayAs)`.
- `hooks/useCadCore.ts`: `addSketchCircleRadiusDimension` accepts optional `displayAs`; new `updateSketchDimensionDisplay` hook. Both imported.

**UI — ViewportPanel:**
- `resolveDimensionDisplayAs(dimensionId)` looks up per-dimension `display_as` from the document state. Falls back to `""` (diameter) for missing/absent fields.
- `dimensionDisplayValue` / `dimensionCoreValue` now use per-dimension `display_as` instead of the global `dimensionToolMode`.
- Circle dimension creation passes the current tool mode as initial `display_as`: `"radius"` mode creates with `display_as = "radius"`, everything else defaults to diameter.
- Right-click context menu on circle dimensions: shows "Show Radius" or "Show Diameter" toggle button (reads current `display_as` from document state, calls `updateSketchDimensionDisplay`).

**Protocol:**
- `commands.schema.json`: `update_sketch_dimension_display` added to command enum.

#### Design Notes
- `display_as` is a pure presentation hint. It never affects geometry — only how the value is shown and parsed in the dimension editor.
- Circle dimensions default to diameter display (`display_as = ""`). Right-click → context menu → "Show Radius" / "Show Diameter" toggles the preference per-dimension.
- The `dimensionToolMode` dropdown was removed. The dimension tool auto-detects the appropriate dimension kind from the clicked geometry.
- Driven dimensions are re-evaluated every `refresh_sketch_derived_state` call, keeping their displayed values current without any driving side effects.

#### Known Issues & Follow-up
- **Raycasting order affects click reliability.** Sketch points are intersected before sketch entities. Clicks near shared line endpoints can resolve to a point instead of the line body. Improving the select/snap tool's point resolution and hover differentiation is a prerequisite for fully reliable dimension tool auto-detection.
- **Escape timing is brittle.** Relies on `pendingDimensionIdRef` being set pre-IPC and not cleared by React effects. A proper state machine for the dimension tool lifecycle would eliminate remaining edge cases.
- **Point-to-point distance is driven-only** (reference-only, cannot be edited to drive geometry).
- **Polygon deletion error** (`Sketch line not found: polygon-1`) — pre-existing bug in the sketch deletion handler, unrelated to the dimension tool.
- **i18n: translation files are incomplete.** Only `en.json` has full coverage. `es.json`, `ja.json`, `zh.json` only cover settings/header keys. Toolbar, viewport panels, sketch tool labels, and help tooltips fall back to English. Header is pinned to English via `{ lng: "en" }` in `AppHeader`. Language dropdown labels are hardcoded in English so users can always navigate back.

---

## 2026-05-23 / 2026-05-24

### Unified Sketch Interaction — Selection Filter, Snap Gating, DOF Coloring, Constraint Deletion

#### Goal
Build a unified sketch interaction system where constraints, snapping, and selection are controlled by a single user-facing checkbox panel. Add DOF counting and entity coloring for constraint status visualization. Add constraint badge selection and deletion via Delete key.

#### C++ Core — New Modules
- `snap_engine.h/.cpp` — snap resolution engine (endpoint, midpoint, center, nearest). Not yet wired to `viewport_state` (future Phase 2).
- `inference_engine.h/.cpp` — auto-detect coincident endpoints and concentric circles at entity-commit time. Populates `constraints[]` on `SketchFeatureParameters`.
- `dof_counter.h/.cpp` — DOF counting for every sketch entity. Reads inline constraints, `line_relations[]`, `constraints[]`, dimensions, fixed points, anchors, and shared endpoints. Produces `EntityDofResult` with `"under"`/`"full"`/`"over"` status.

#### C++ Core — Modified
- `feature.h`: `SketchConstraint` struct, `SelectionFilter` struct (16 boolean toggles), `constraints[]` on `SketchFeatureParameters`.
- `document.h/.cpp`: `selection_filter` on `DocumentState`, `update_selection_filter` with undo/redo.
- `app.cpp`: `update_selection_filter` IPC handler (reads all 16 payload fields).
- `viewport.h/.cpp`: `selection_filter` and `dof_statuses` on `ViewportState`. `count_sketch_dof()` called in `build_viewport_state` for active sketches.
- `serialization.cpp`: constraints, filter, and DOF statuses round-trip (backward-compat: absent → defaults).
- `sketch_feature.cpp`: calls inference engine in `add_sketch_line` / `add_sketch_circle`.
- `CMakeLists.txt`: 3 new source files added.

#### Protocol
- `commands.schema.json`: `update_selection_filter`.

#### TypeScript — Types, IPC, Hooks
- `types/ipc.ts`: `UpdateSelectionFilterCommand`, `dof_statuses` and `selection_filter` on `ViewportState`.
- `lib/ipcProtocol.ts`: `makeUpdateSelectionFilterCommand`.
- `hooks/useCadCore.ts`: `updateSelectionFilter` hook with viewport refresh.
- `lib/schemas/ipcSchema.ts`: zod schemas for `dof_statuses` and `selection_filter`.

#### Selection Filter Panel UI
- `layout/SelectionFilterPanel.tsx`: **NEW** — checkbox panel with 3 sections (Sketch Geometry, Snap Types, Global). Persists to localStorage. Instant snap gating (no IPC round trip).
- `AppHeader.tsx`: gear button next to `f(x)`, renders panel below button.
- `App.tsx`: `filterPanelOpen` state + IPC wiring.

#### Snap Gating
- `ViewportPanel.tsx` — `resolveSnappedSketchPoint()` reads filter from localStorage directly (instant response). Static candidates, grid, and perpendicular-foot snaps all gated.
- Grid snap shows `"Snap: Grid"` label when active, respects filter immediately.

#### DOF Coloring
- `ViewportPanel.tsx` — `paintSketchEntityMaterials()`: single-pass DOF integration. Priority: selected → hovered → projected → DOF-full (dark blue) → DOF-over (red) → default yellow.
- Status bar: selected entity shows `"Line · 3/4 DOF"` / `"Line · 4/4 DOF (fully constrained)"` / `"Line · 5/4 DOF (OVER-CONSTRAINED)"`.
- `i18n/en.json`: `entitySelectedDof`, `dofFull`, `dofOver`.

#### Constraint Deletion
- Constraint badges: scale 6 (was 4.4), raycaster `recursive: true`.
- Click badge → `selectedConstraint` state set → status bar `"Constraint: {kind} — Press Delete to remove"`.
- Delete/Backspace → calls `clearSketchConstraintRef` with stored kind/entityId/relatedEntityId.
- Escape / click elsewhere → deselects.
- `i18n/en.json`: `constraintSelected`.

#### Known Issues & Follow-up
- **Perpendicular snap**: general perpendicular-to-line parked. Perpendicular-foot (start-on-host-line) works.
- **Constraint badge highlight**: ✅ shipped 2026-05-24 — selected badges turn cyan + scale up via a dedicated useEffect. Right-click → Delete also works now.
- **Driven dimension proposal**: should offer driven dim when entity already fully constrained.
- **DOF color legend**: help menu entry needed (blue=full, red=over, yellow=under).
- **Line dimension tool redesign**: full specification in the 2026-05-24 section below. Three work items remain: post-creation endpoint drag (select tool), intent-based drag-direction detection (dimension tool), and perpendicular snap for dimension tool disambiguation.

## 2026-05-24

### Line Dimension Tool — Specification

> This is a behavioral specification for the line dimension tool, captured
> as the target design. It describes the full workflow: preview-phase
> dimension editing, post-creation drag, and intent-based dimension
> detection. Not yet implemented — the snap engine and DOF system are
> prerequisite foundations that are now finished.

#### Goal
The line dimension tool must handle the full lifecycle of a line's
dimensions: constrained dimensions entered during preview, post-creation
dimension addition via drag, and intent-based dimension type detection
based on the user's drag direction relative to the line.

#### Preview-Phase Dimension Editing (Line Creation)

During line creation, the preview shows 2 floating input fields: **length**
and **angle**. The user can Tab between them.

| User action | Result |
|---|---|
| Modify length → press Enter | Length becomes a **constrained** dimension on the line |
| Modify angle → press Enter | Angle becomes a **constrained** dimension on the line |
| Modify both → press Enter | Both become constrained dimensions |
| Do not modify either → click final point | Line is drawn. **No dimension or angle constraint** is created. Values are lost (driven, not visible). |
| Modify a value → drag the endpoint (mouse) | The manually-entered value is overridden by the drag. Reverts to unconstrained. |

**Key rule:** only explicitly entered values become constraints. A value
that appears pre-populated in the input but is never edited does NOT
create a constraint — it is a driven value that is invisible after commit.

**Flickering problem (implementation artifact):** During line preview,
the core creates auto-dimensions for the preview geometry. When the user
commits the line without editing the input fields (click-only commit),
`scheduleDimensionDeletion()` fires and deletes those auto-dimensions.
This causes the dimension to appear momentarily on screen (as a flash)
then disappear. Two impacts:

- **Visual**: the dimension label flashes in/out during commit, creating
  a distracting flicker.
- **IPC overhead**: a round-trip IPC command (`add_sketch_line` → core
  creates auto-dim → `delete_sketch_dimension` to remove it) per commit.

The flicker is benign but wasteful. A cleaner approach would be to
suppress auto-dimension creation during preview entirely and only create
the dimension on explicit user edit, but the current architecture ties
auto-dimension creation to entity push in the C++ core — there's no
"preview mode" flag to gate it.

#### Post-Creation Dimension Workflow

If no dimension was created during preview, the user can add one later
through the Dimension tool (`D`):

1. Activate Dimension tool (`D`)
2. Click on the line body → the tool offers a **single-entity line
   length dimension** (the `add_sketch_line_length_dimension` flow)
3. The dimension editor opens with the current length
4. User types a value → Enter → dimension becomes constrained

**Current bug (root cause):** clicking the line body immediately commits
to a single-entity dimension. This happens because:

1. `dimensionToolFirstLineRef` is `null` on the first click.
2. The code resolves the entity ID, sees no staged entity, and falls
   through to the entity-kind dispatch (line ~6540).
3. For a line with no existing `dim-line-{id}`, `dimCreateLine()` is
   called immediately — it sends `add_sketch_line_length_dimension` IPC
   and sets `pendingDimensionPlacementRef`.
4. `dimCreateLine()` does NOT stage the entity (`dimensionToolFirstLineRef`
   stays `null`) — only `dimSelectLine()` (called when dimension already
   exists) stages. So the first click on a dimension-less line both
   creates the dimension AND fails to stage for two-point follow-up.

**Consequence:** the first click steals the interaction. The user cannot
perform a two-point distance dimension from one endpoint of the line
because clicking anywhere on the line body immediately triggers the
single-entity dimension creation. The two-point `dimensionToolFirstLineRef`
staging is never reached.

**Example:** User wants to dimension the distance between one line endpoint
and an arbitrary point in space. Clicks near the endpoint → the raycast
hits the line body → `dimCreateLine` fires → single-entity dimension is
created. The user never gets a chance to pick the second point.

This also blocks the two-arbitrary-points distance dimension: clicking
near a line's endpoint resolves to the line entity (via point regex),
not to a point-distance pick.

#### Intent-Based Dimension Detection

When the user activates the Dimension tool and starts a drag on or near
a line, the tool should detect intent based on drag direction relative
to the line's geometry:

| Drag direction relative to line | Dimension type created |
|---|---|
| Perpendicular to the line (within ±15°) | **Line length dimension** — single entity, `dim-line-{id}` |
| Parallel to the line (within ±15°) from one endpoint | **Endpoint-to-endpoint distance** (diagonal if non-axis-aligned) — two-point, `dim-point-distance-{a}-{b}` |
| Diagonal / ambiguous (between the two zones) | Default to **line length dimension** |

**Cube diagonal scenario as example:** When the user has a cube wireframe
and clicks diagonal opposite points, the result should be:

- If the drag moves perpendicular to the line → line length dimension
- If the drag moves parallel along the line's direction → diagonal
  distance dimension (distance between the two endpoints)
- The tool must NOT immediately snap to one interpretation — it waits
  for sufficient mouse movement to disambiguate

**Commit-early-with-regroup model (correct design):**

The Dimension tool uses the snap engine's resolution on the first click
to jump to the most likely conclusion immediately, then allows the user
to "regroup" — pick a second point that overrides the initial choice.

**Principle:** Jump to conclusion from the first click when the snap type
strongly indicates intent. Save clicks in the common case. Allow
regrouping when the user contradicts the initial guess.

**First-click behavior (by snap target):**

| Snap target (first click) | Immediate action | Allows regroup? |
|---|---|---|
| **Line body** (snap on line) | Immediately create line length dimension + open editor. Stages the entity for two-entity follow-up. | Yes — if user clicks a second point or entity while the dimension editor is open, delete the line dimension and create a point-distance or two-entity dimension instead. |
| **Circle edge** (snap on circle circumference) | Immediately create diameter dimension + show preview. Stages the circle for two-entity follow-up. | Yes — if user clicks a second point, delete the circle dimension and create point-distance instead. |
| **Circle center / endpoint / sketch point** | **Wait.** Stage the point. Do not create a dimension yet. The next click determines: same entity → dimension, different entity → distance/angle, second point → point-to-point. | N/A — no dimension created yet, so no regroup needed. |
| **Polygon edge / polygon center** | Same as line/circle respectively. | Yes. |
| **Empty space** | Create a sketch-plane coordinate as the first point. Wait for second click → point-to-point distance. | N/A. |

**Acceptance without editing:**

After the first click creates a dimension and opens the editor, the user
can accept the current value as-is without typing anything:

| Method | Result |
|---|---|
| Click **empty canvas space** | Accept current value, close editor, dimension stays visible |
| **Right-click** | Interpreted as "Enter" — accept + close |
| **Enter** key | Accept + close (no text typed) |

The dimension stays with its measured value — it does NOT become a
constrained dimension (user didn't type), but it remains visible as a
reference/driven dimension for the sketch. The editor closes.

**Regrouping flow (the core mechanism):**

When a dimension was created on the first click but the user picks a
second point belonging to a **different** geometry (not the same entity):

```
1. First click on line body
   → dimCreateLine() fires
   → dimension `dim-line-{id}` created, editor opens
   → pendingDimensionPlacementRef = true

2. User clicks a point on a DIFFERENT entity (other line, circle, etc.)
   → Tool detects: dimension editor is open AND target != staged entity
   → delete_sketch_dimension(dim-line-{id})
   → clear pendingDimensionPlacementRef
   → create appropriate two-entity/two-point dimension

3. Entity-to-entity regroup:
   First click on line A → line dim created
   Second click on line B (different entity) → delete line dim → create
   angle or distance between A and B
```

**Contrast with acceptance:** If the second click is on empty space (not
on another entity), it's an acceptance — dimension stays.

**Why this is better than strict two-phase:**

- Common case (user wants line length) = 1 click, done. No waiting.
- Regroup is an error recovery path — delete the hasty dimension and
  create the correct one. The delete IPC is cheap compared to making the
  user always wait.
- Ties directly into the snap engine: the snap type on the first click
  IS the intent signal. Line body snap → length. Circle edge snap →
  diameter. Point snap → ambiguous, wait.

#### Line Drag After Creation

Lines created without a dimension constraint should be **draggable after
creation** to adjust their length/position. This belongs to the **select
tool** functionality (not the line tool or dimension tool):

1. Select tool active → hover over a line endpoint
2. Click and drag the endpoint → line resizes in real time
3. On release → the new geometry is committed (same IPC path as
   `update_sketch_line`)
4. If the line has existing constrained dimensions, they are **updated
   to driven** (since the direct drag overrides the constraint)
5. If the line has no constrained dimensions, it remains unconstrained
   after the drag

#### Polygon Dimension Treatment

Complex shapes (polygons, rectangles) should be treated as collections of
individual lines for dimensioning purposes. Each constituent line of a
polygon or rectangle should be individually dimensionable through the same
line dimension workflow. Deleting one line from a polygon results in
dimensions on remaining lines staying valid; any polygon-radius dimension
that referenced the polygon as a whole is treated as `dependency_broken`.

#### Prerequisites

- Snap engine finished ✅ (2026-05-23)
- DOF counting + coloring ✅ (2026-05-23)
- Single-entity dimension creation IPC ✅ (2026-05-24)
- Post-creation endpoint drag (select tool) — **not yet implemented**
- Intent-based drag-direction detection — **not yet implemented**
- Perpendicular snap for dimension tool — **not yet implemented** (see Known Issues)

#### Right-Click — Context-Sensitive Behavior

Professional CAD software uses right-click as a context-sensitive shortcut
whose function depends on the current tool/command state. PolySmith must
implement this as a first-class interaction primitive:

| State | Right-click behavior |
|---|---|
| **Active dimension editor** (value input open) | **Enter** — accept the current value, close editor. Same as pressing Enter key. |
| **Active tool mid-operation** (line being drawn, circle being placed) | **Escape** — cancel the current operation, exit the tool. Same as pressing Escape. |
| **No tool active** (select/idle mode) | **Repeat last tool** — re-launch the most recently used sketch or modeling tool. If last tool was Line, right-click starts drawing a new line. If last tool was Delete, right-click enters delete mode. |

**Implementation notes:**

- Right-click behavior must NOT be hardcoded to "context menu." The
  default browser/OS right-click context menu is suppressed in the Tauri
  shell — right-click is a first-class CAD input.
- During dimension editing, right-click is specifically "accept without
  editing" — the dimension stays with its measured value.
- During active drawing (line preview, circle preview), right-click means
  "abort" — discard the in-progress geometry.
- The "last tool" must persist across tool switches. If the user selected
  Dimension tool, closed it (Escape), then right-clicks → Dimension tool
  reactivates. Last tool is cleared when the user starts a new tool via
  toolbar click or hotkey (these are explicit choices, not repetition).

**Last-tool tracking:**

```
let lastActiveTool: SketchTool | null = null;

onToolActivate(tool):
    lastActiveTool = tool   // explicit choice records itself

onToolDeactivate():
    // tool exited via Escape or completion
    // lastActiveTool is preserved for right-click repeat

onRightClick(idle):
    if lastActiveTool:
        activateTool(lastActiveTool)
    else:
        // no previous tool — right-click is a no-op in idle state
```

---

### Session Summary — 2026-05-24 (second half)

**Shipped this session:**

| Item | Files |
|---|---|
| **Regroup-aware dimension tool** — clicking a different entity after a just-created dim now deletes the hasty dim and creates a two-entity dimension instead | `ViewportPanel.tsx` |
| **Point-snap → wait** — clicking a sketch point in Dimension tool stages and waits instead of resolving to entity and creating a dimension | `ViewportPanel.tsx` |
| **Empty-space acceptance** — clicking empty canvas while dimension editor is open keeps the dimension (clears pending refs) | `ViewportPanel.tsx` |
| **Escape fix** — existing dimensions are no longer deleted on Escape (`pendingDimensionIdRef` removed from `dimSelect*` functions) | `ViewportPanel.tsx` |
| **Constraint badge highlight** — selected badge turns cyan + scales up, driven by a `useEffect` on `selectedConstraint` state | `ViewportPanel.tsx` |
| **Constraint right-click → Delete** — new context menu branch for constraints, extending `ViewportContextMenuState` type | `ViewportPanel.tsx`, `types/viewport.ts` |
| **Delete key race fix** — reads selection from document directly instead of relying on async store round-trip; fixes projected-entity deletion (GitHub issue #4) | `ViewportPanel.tsx` |

**Remaining work items (unchanged):**

- Right-click context-sensitive behavior (Enter / Escape / repeat-last-tool)
- Post-creation endpoint drag (select tool)
- Perpendicular snap (needed for dimension tool disambiguation)
- Driven dimension proposal
- DOF color legend
- Pre-existing test failure (`test_fixed_endpoint_stays_put_when_redimensioning`)
- Flickering auto-dimensions during preview (low priority)

### Trim Tool — Planning (2026-05-24)

Full implementation plan written at [Trim-Tool-Implementation-Plan](Trim-Tool-Implementation-Plan).
Summary:

- **New core module:** `trim_engine.h/.cpp` — intersection detection, entity
  splitting, segment selection
- **Intersection types:** line-line, line-circle, line-arc, circle-circle,
  circle-arc, arc-arc (6 pairs, 3 algorithms: segment-segment, line-circle
  quadratic, circle-circle)
- **Entity transformations:** lines → shortened lines, circles → arcs, arcs →
  shortened arcs, polygon lines → independent lines (polygon dissolves)
- **Constraint handling:** re-evaluated by geometric checks where possible;
  `dependency_broken` for type-changing operations (circle→arc)
- **4 phases:** line trimming (MVP) → circle→arc → arc trimming → polish
- **Design decisions:** trim sees only segments — no entity-level logic.
  All constraints are destroyed. Every segment is independently deletable.
  Shared points are severed on trim. Polygon dissolves on any constituent
  line trim.
- **Bugs fixed:** iterator invalidation from `push_back` during split;
  zero-length endpoint segments; H/V constraint badge revival after
  relation deletion; fillet corner/trim point ID survival; TS `u`-parameter
  check missing; coordinate mismatch with `resolveSketchPlanePoint`

---

### Draft Dimension Visualization — Line Tool (2026-05-25)

Full design doc at [Draft Dimension Visualization](Draft-Dimension-Visualization).

#### Goal
Replace the HTML-only floating input boxes during line drafting with Three.js-rendered dimension geometry (length dimension lines, angle arc, reference lines, arrowheads) matching the visual language of committed sketch dimensions.

#### Performance fix — duplicate rendering path removed
The initial implementation had two code paths drawing the same cyan dimension geometry every frame: `renderDraftDimensions()` in the animation loop and a reusable `draftDimSceneObjRef` in
`handlePointerMove`. `clearDraftDimGroup()` destroyed the reusable object every frame;
`handlePointerMove` recreated it on every mouse move — a destroy/recreate cycle at 60 fps.
This caused flickering, GPU churn, and crashes when user input arrived mid-cycle. The
duplicate (~80 lines) was removed; only `renderDraftDimensions()` draws dimension
geometry now. This also eliminated the sluggishness that had been affecting line drafting.

#### Per-frame geometry leak removed
Two diagnostic magenta debug lines created new `THREE.BufferGeometry` +
`THREE.LineBasicMaterial` every frame with 100ms `setTimeout` cleanup — multiple
generations alive simultaneously, leaking ~120 geometry allocations + GPU buffer
uploads per second. Both removed.

#### Angle arc
- Arc centered at line start point (`sx, sy`), not sketch-plane origin (0,0) —
  fixes arc disappearing for chained lines
- Dotted reference line from start along reference angle (horizontal for first
  line, previous segment direction for chained)
- Dotted cursor extension line from cursor outward
- Zoom-aware arc cap: `max(8, min(lineLength, 480 × viewHeight / viewportHeight))`
  — arc hugs rubber band for short lines, caps at ~480px-equivalent for
  readability at any zoom
- `previousLineAngleRef` stores the 2D sketch angle of the last committed
  segment. Chained lines reference the previous segment direction instead of
  horizontal
- Arc arrowheads at both sweep endpoints

#### Length dimension
- Zoom-aware offset: `max(4, 30 × viewHeight / viewportHeight)` doubled
  (`-2 × zoomDimOffset`)
- Offset in `-perpDir` direction (opposite to angle arc) for visual separation

#### Input overlay fixes
- `draftFieldScreenPosition()` fallback updated for line tool: length at line
  midpoint, angle at start point
- Stale `draftDimScreenPositionsRef` values cleared on input change (`delete`)
  to prevent one-frame label jumps

#### Files changed
- `apps/desktop-ui/src/layout/ViewportPanel.tsx` — all changes
  (~95 lines net removed, ~50 lines changed)

### Committed Dimension Style Matching — Planning (2026-05-25)

Committed line dimensions currently use `buildSketchDimensionObject` which
renders fixed-size geometry from C++ core data. They lack the preview's
zoom-aware offsets, reference lines, and angle arc style.

**Two approaches evaluated:**

1. **C++ core approach** — extend IPC schema and core dimension computation to
   emit preview-style geometry (reference lines, zoom-aware offsets, arc data).
   Thorough but spans C++/protocol/TypeScript layers.

2. **Frontend approach (chosen)** — compute committed line dimension rendering
   client-side from existing `sceneData.sketchLines` + `displayedSketchDimensions`,
   reusing the same rendering pipeline built for the draft preview. No IPC changes,
   negligible perf (~1-5 dims per sketch vs preview's 60fps). Non-line dimensions
   (circle, rectangle) stay on the C++ path until their previews are modernized.

#### Original decision (under reconsideration)

Initially chose the frontend approach: render committed line-type dimensions
through the same zoom-aware pipeline as `renderDraftDimensions`, reusing
client-side data without IPC changes.

#### Reconsideration (2026-05-25)

Concerns with the frontend approach:

- **Architectural boundary.** The project rule is that React must not own
  CAD state or compute geometry. Having the frontend derive dimension
  geometry from scene data sets a precedent that breaks this boundary.
- **Consistency.** Every other dimension path (circle, rectangle, polygon,
  arcs, angle, distance) goes through the C++ core → IPC → `buildSketchDimensionObject`.
  A frontend-only path for line dimensions creates a split that future
  contributors will trip over.
- **Scaling.** On large projects with many committed dimensions, frontend
  recomputation at render time may regress vs the core's single-pass
  geometry emission.
- **Long-term advantage of core path.** The C++ core approach extends the
  IPC dimension schema once and benefits every dimension type uniformly,
  not just lines. It also enables the same zoom-aware rendering for
  non-draft-preview contexts (e.g. dimension-only view, printing,
  future 2D drawing workspace).

The core path remains the architecturally sound direction. The frontend
approach is noted as a quicker visual match to the draft preview but carries
long-term debt. Final decision pending further evaluation.

### Draft Dimension Visualization — Formal Specification (2026-05-25)

The draft preview rendering rules in `renderDraftDimensions()` were
extracted into a formal specification in
[Draft-Dimension-Visualization](Draft-Dimension-Visualization).
These rules must be the reference for any committed dimension
restyling — regardless of whether the implementation is C++ core or
frontend.

**Zoom formulas** (computed per frame):

```
viewH         = (camera.top - camera.bottom) / camera.zoom
vpH           = renderer.domElement.height
zoomDimOffset = max(4, 30 × viewH / vpH)      // ≈30 px
zoomCap       = max(20, 480 × viewH / vpH)     // ≈480 px
```

**Length dimension:**
- Offset direction: `-perpDir` (flipped toward camera) — opposite
  side from the angle arc
- Dimension line offset: `-2 × zoomDimOffset × perpDir` (~60 px)
- Extension lines from entity endpoints to dimension line endpoints
- Arrowheads at both ends, oriented inward
- Label at dimension line midpoint

**Angle arc:**
- Center: line start point (pivot)
- Radius: `max(8, min(lineLen, zoomCap))` — follows rubber band, clamps
  at zoom cap, hard minimum of 8
- Sweep: `refAngle → angleRad`, shorter path (normalised to [-π, π])
- CCW direction places arc opposite to length dimension
- Arrowheads at sweep endpoints, tangential to arc
- Label at arc midpoint

**Reference line (angle):** `max(12, lineLen × 0.28)` from start along
`refAngle`, dashed

**Cursor extension:** `max(12, lineLen × 0.35)` from cursor outward,
dashed

**Angle reference:** `previousLineAngleRef` — horizontal (0) for first
line, previous committed segment angle for chained lines

---

### C++ Core Arc Geometry — Committed Dimension Restyling Start (2026-05-25)

Extended the C++ core to emit enriched arc and reference-line geometry
for angle dimensions, and wired it through the full stack:

**C++ Core:**
- `viewport.h`: added 13 fields to `ViewportSketchDimensionPrimitive` —
  arc center/radius/start-angle/end-angle/ccw, reference-line start/end
- `viewport.cpp:make_line_angle_dimension_primitive`: computes arc sweep
  (cross-product sign for CCW), reference line along reference direction,
  arc center at pivot — all in world space
- `serialization.cpp`: serializes all new fields as IPC JSON

**TypeScript types & pipeline:**
- `viewport.ts`: `ViewportSketchDimension` interface with optional
  arc/reference-line fields
- `scene.ts`: `SketchDimensionScene` with matching optional camelCase fields
- `viewportScene.ts`: `makeSketchDimension` maps IPC snake_case → scene tuples
- `ipcSchema.ts`: Zod validator accepts new optional fields
- `viewport.utils.ts`: `buildSketchDimensionObject` prefers core-provided arc
  data when available (`arcRadius > 0 && arcCenter`), falls back to existing
  client-side collinear-ray reconstruction for legacy data

No TS regressions (same 26 pre-existing errors).

### Two-Line Angle Arc + Zoom-Aware Scaling (2026-05-25)

Extended the same arc/reference-line geometry to the two-line angle
dimension primitive (`make_angle_dimension_primitive` in `viewport.cpp`),
and added zoom-aware scaling for all committed dimension groups.

**C++ Core:**
- `viewport.cpp:make_angle_dimension_primitive` — computes arc sweep
  from line-A direction to line-B direction, reference line along
  line A, arc center at the shared pivot endpoint

**Zoom-aware scaling (frontend):**
- Every frame in `render()`, before `renderer.render()`: computes
  `zoomFactor = max(0.5, min(30 × viewH / vpH, 6.0))` from the
  orthographic camera and applies it uniformly to every committed
  dimension group via `obj.scale.setScalar(zoomFactor)`
- Label sprites are separate objects — not scaled
- The zoomFactor range [0.5, 6.0] prevents arrows from disappearing
  at extreme zoom-out and from overwhelming the canvas at zoom-in
- Formula matches the draft preview's `zoomDimOffset / 2` convention

#### Bugfix — zoom scaling reverted (2026-05-25)

Uniform scaling of dimension groups from origin `(0,0,0)` pushes
world-space geometry radially away — wrong for committed dimensions
whose geometry IS world-space position. The zoom-aware scaling
was reverted from the render loop. Zoom-aware committed dimensions
need a different mechanism (recompute offset in world space, not
scale).

#### Fix — arc radius proportional to line length

Committed angle dimensions now use a dynamic arc radius instead of
the fixed 6.0 world units:
- `make_line_angle_dimension_primitive`: `max(4, min(lineLength × 0.5, 30))`
- `make_angle_dimension_primitive`: `max(4, min(shorterLineLen × 0.5, 30))`
- `kLabelRadius` follows the arc (`kArcRadius + 3`) so the label stays
  outside the arc at any size.

#### Bugfix — raycaster recursion

Changed `raycaster.intersectObjects` call for dimension hit
detection from `recursive: false` to `recursive: true`, because
`buildSketchDimensionObject` now returns a `THREE.Group` (containing
LineSegments + optional arrow Mesh) instead of a bare
`THREE.LineSegments`. Both children carry `userData.sketchDimensionId`.

---

### Viewport Rectangle Selection — Planning (2026-05-26)

Design doc and implementation plan for rectangular drag selection in
the sketch viewport. Full plan at [Sketch-Selection-Controls](Sketch-Selection-Controls).

**Industry standard:**
- **Left → Right (window):** solid blue rectangle, selects fully enclosed entities
- **Right → Left (crossing):** dashed green rectangle, selects any touching/crossing entity

**Implementation approach:**
- All frontend — no core changes needed
- Existing IPC primitives: `clear_selection` + `select_sketch_entity { additive: true }`
- Selection rectangle: HTML `<div>` overlay (fast, no Three.js overhead)
- Selection algorithm: project 3D entity points to screen space via
  `projectWorldPointToViewport`, then 2D rectangle hit testing

**Entity hit test per mode:**

| Entity | Window (L→R) | Crossing (R→L) |
|---|---|---|
| Line | Both endpoints inside | Any endpoint inside, or segment crosses rect |
| Circle | Entire bounding box inside | Center or any quadrant inside |
| Arc | Both endpoints + midpoint inside | Any endpoint or arc crossing |
| Point | Point inside | Point inside |
| Polygon | All vertices inside | Any vertex or edge inside |

#### Status: implemented (2026-05-26)

Single file implementation in `ViewportPanel.tsx`:
- `handlePointerDown` starts drag on empty canvas (Select mode, no hit)
- `handlePointerMove` updates rectangle, disables orbit during drag
- `handlePointerUp` runs `performRectangleSelect` and sends IPC
- Canvas-space projection via `projectWorldPointToViewport`
- Segment intersection for crossing mode (Cohen-Sutherland variant)
- HTML overlay div with blue/green border and translucent fill

No TS regressions. No core changes needed.

#### Bugfixes (2026-05-26)

Three bugs fixed after initial implementation:

1. **Point field-name mismatch.** The point-testing loop used snake_case
   (`pt.position_x`, `pt.point_id`) but `SketchPointScene` uses camelCase
   (`pt.position`, `pt.pointId`). Resolved to `undefined`, causing
   `selected.push(undefined)` → `select_sketch_entity` without `entity_id`
   → C++ threw "missing string field 'entity_id'". Fixed field names.
   Additionally, the point loop was removed entirely — points are not
   rectangle-selectable per CAD standard and use a different IPC path
   (`pickSketchPoint`, not `selectSketchEntity`).

2. **Coordinate-space mismatch.** The drag rectangle used viewport coords
   (`event.clientX/Y`) but `projectWorldPointToViewport` returns
   canvas-relative coords. Window-select (L→R) requires both endpoints
   inside rect, which never matched with the offset. Fixed by offsetting
   the rect by `renderer.domElement.getBoundingClientRect()`.

3. **Serial batch select → parallel.** The batch callback looped
   `await selectSketchEntity(id, additive)` serially — each call is 2 IPC
   round-trips (select + getViewportState). For N entities that's 2N
   round-trips, ~0.5 s per entity. Added `batchSelectSketchEntities` to
   `useCadCore` that parallelizes all select commands via `Promise.all`
  and sends one final `getViewportState`. Also added `documentRef` in
  `ViewportPanel.tsx` so `handleContextMenu` always sees the latest
  document state after a batch select.

## Feature-Complete Extrude Parameter Model (2026-05-26)

- upgraded extrude parameters from a single `depth` into a side-based model:
  `extent_mode`, `side1`, optional `side2`, `thin`, `operation`, and
  `intersect_result`, while keeping legacy `depth`, `mode`, and
  `target_body_id` readable and writable
- added `update_extrude_parameters` for full live-preview edits; existing
  `update_extrude_depth`, `update_extrude_mode`, `update_extrude_target_body`,
  and `update_extrude_profiles` remain shorthand compatibility paths
- added core support for one-side, symmetric, two-side, per-side start offset,
  per-side taper, thin closed profiles, thin open sketch line/arc chains,
  Through All / To Object / To Next extent solving, Auto operation, and
  Intersect mode
- reworked the extrude floating panel into grouped controls for operation,
  intersect result, extents, side settings, thin placement, and targets, with
  timeline cancel restoring the complete saved extrude parameter snapshot

## Screws / Holes / Threading Foundation (2026-05-27)

- added semantic native feature kinds for `hole`, `helix`, `thread`, and
  `fastener`, with JSON serialization and IPC commands for create/update/confirm
  flows
- implemented first-pass core-owned Hole: planar face source, local center,
  simple / counterbore / countersink / spotface parameters, blind / through-all
  extent, cosmetic thread metadata, and boolean cut preview through the body
  compiler
- added dependency refresh for holes and helices: hole source faces re-resolve
  against the current body shape, helix axis sources re-resolve against sketch
  lines / construction axes / straight body edges, and failures degrade via
  `dependency_broken`
- added sampled construction helix viewport payloads in `viewport_state.helices[]`
  and a lightweight renderer path using theme construction colors
- exposed the Hole tool in the Create toolbar with the contextual workflow:
  select/invoke or invoke/select planar face, edit the floating panel, confirm or
  cancel with undo
- staged thread and fastener semantics without claiming modeled thread booleans
  are complete; modeled thread generation will build on the helix + sweep path

### Fastener Standards Pass (2026-05-27)

- added metric and imperial hole standard tables for common workshop sizes,
  covering metric M2-M12 and UNC/UNF imperial entries from small numbered
  screws through 1/2 inch
- extended `hole_parameters` with `standard`, `standard_size`, `hole_fit`,
  `thread_pitch`, `major_diameter`, and `minor_diameter` so saved files preserve
  the user’s standard-driven intent instead of only the resulting drill diameter
- updated the Hole panel with Custom / Metric / Imperial preset controls plus
  Clearance / Tap drill / Threaded fit selection; choosing a preset updates the
  drill diameter, counterbore, countersink, and cosmetic thread metadata

### Helix UI Pass (2026-05-27)

- exposed the core-owned Helix feature in the Construct toolbar with the same
  contextual workflow as the other construction tools: invoke/select or
  select/invoke an axis source, edit a floating panel, confirm to keep or cancel
  to undo the created feature
- allowed helix axis picking from inactive sketch lines and straight body edges;
  preselected construction-axis features can also seed the helix
- added live panel controls for radius, pitch, height, handedness, and start
  angle, all driven through `update_helix_parameters`

### Thread UI Pass (2026-05-27)

- exposed the semantic Thread feature in the Create toolbar with contextual
  body-target and axis-source picking
- added a floating Thread panel for external/internal mode, metric/imperial
  standard presets, size, diameter, pitch, length, start offset, handedness, and
  cosmetic representation
- kept modeled thread generation disabled in the UI until the helical
  sweep/boolean path is robust enough to generate real thread geometry
- added dependency refresh for thread target bodies and axis sources so broken
  references surface as `dependency_broken` warnings instead of stale metadata

### Fastener UI Pass (2026-05-27)

- exposed the semantic Fastener feature in the Create toolbar as an immediate
  core-owned generator with a live floating edit panel
- added metric / imperial / custom size controls, diameter, length, thread
  length, head type, and drive type fields driven through
  `update_fastener_parameters`
- kept thread and drive details as cosmetic/metadata for now; the native shape
  currently creates the shaft and head plus a lightweight cosmetic thread curve
  while modeled threads and recess geometry remain staged behind the thread
  modeling path
- added timeline edit support so existing fastener parameters can be reopened
  and cancelled back to their original values

### Cosmetic Thread Fixes (2026-05-27)

- fixed flat/countersunk fastener heads so the cone widens away from the shank
  instead of appearing inverted
- added viewport cosmetic thread helices for semantic `thread` features and
  fasteners that use cosmetic thread representation
- rendered helix/reference thread curves above solid geometry so cosmetic
  threads remain visible as visual markers instead of hidden metadata

### Modeled Fastener Thread First Pass (2026-05-27)

- added a modeled thread representation path for generated fasteners that
  cuts a V-profile external helical groove with OCCT pipe geometry into the
  shaft body
- kept cosmetic threads as the default for speed and compatibility, while the
  Fastener panel now lets users switch between cosmetic and modeled thread
  representations
- left drive recess geometry staged for a later pass; standalone body Thread
  booleans are handled in the modeled Thread feature pass below

### Modeled Thread Feature Pass (2026-05-27)

- enabled the standalone Thread panel's representation picker so users can
  choose Cosmetic or Modeled for a selected target body and axis source
- wired modeled Thread features through the native body compiler as a
  target-body modification: the compiler re-resolves the selected axis, builds
  a V-profile helical cutter, and cuts it from the target body
- kept cosmetic threads as lightweight viewport helices and preserved graceful
  fallback behavior when axis resolution or OCCT booleans fail

### Threading Completion Pass (2026-05-27)

- threaded holes now support the same cosmetic / modeled representation choice:
  cosmetic holes emit a viewport helix and modeled holes cut an internal
  V-profile helical groove after the drill hole
- generated fasteners now persist pitch and minor diameter alongside major
  diameter so standard-selected fastener threads are data-driven instead of
  diameter-estimated
- hex-socket and Phillips drive options now cut simple recess geometry into
  generated fastener heads instead of remaining metadata-only
- attempted to replace the fastener modeled-thread groove-cut experiment with
  an explicit faceted external thread shaft; manual QA later showed this path
  is still bugged and not production-ready

### Known Issue: Modeled Thread Geometry (2026-05-27)

- modeled thread geometry is currently experimental and visibly incorrect in
  manual QA. Generated fastener threads can lose the helical shaft, split into
  detached pieces, or export as incomplete screw geometry.
- cosmetic threads and semantic thread metadata are the reliable path for now.
  Treat modeled fastener threads, standalone modeled thread cuts, and modeled
  threaded holes as known-buggy until the native thread kernel is reworked.
- the follow-up should replace the current ad hoc pipe/cutter/faceted attempts
  with one robust core-owned thread construction path, validated against
  viewport display and exported STL/STEP geometry.

### Materials / Color Overrides (2026-05-27)

- added document-scoped appearance overrides for body and face colors. Body
  colors are keyed by body/root feature id; face colors store the emitted face
  id plus an appearance signature so the core does not blindly trust only a
  face index after recompute.
- added `set_body_color`, `set_face_color`, `clear_body_color`,
  `clear_face_color`, and `clear_appearance_overrides` IPC commands, with
  `.polysmith` save/load round-tripping under `document_state.appearance`.
- extended viewport payloads with optional `appearance_color` on meshes, legacy
  body primitives, and solid faces. The renderer keeps theme body colors as the
  default and applies custom colors only when an override resolves; face colors
  render above body colors.
- added a top-bar Materials popover with Body / Face paint modes, quick color
  swatches, HSV-style picker controls, hex input, apply, clear selected, and
  clear all actions.

### Body Context Move / Copy (2026-05-27)

- added a core-owned `body_copy` timeline feature and `create_body_copy` IPC
  command. Copies re-resolve their source body on recompute, emit a new body
  under the copy feature id, preserve the source local frame, and degrade with a
  dependency warning if the source disappears.
- added Move and Copy entries to body context menus in both the viewport and
  hierarchy. Copy creates the duplicate in place and immediately opens the Move
  panel/gizmo for placement.
- split body copy into linked and independent modes. Linked copies keep
  re-resolving the source body and show the source name in muted hierarchy text;
  independent copies store a frozen core shape snapshot and do not update when
  the source changes.
- added an irreversible-by-design Unlink action for linked body copies. The UI
  shows it only for linked copies, asks for confirmation, and the core converts
  the copy into a standalone snapshot while preserving normal undo support.

### Dimension Visual Snap Preview (2026-05-27)

- added UI-only muted relation previews while placing sketch dimensions near
  eligible second entities: parallel line distance, shared-endpoint line angle,
  and line-circle distance. The preview respects the existing sketch selection
  and snap filters and is rendered as non-pickable Three.js geometry.
- made the relation preview take precedence over the temporary unary dimension
  placement so the two visuals do not flicker against each other.
- added `update_sketch_dimension_label_position` and optional sketch-local
  dimension label placement metadata so confirmed dimension label placement can
  persist in `.polysmith` files. The first core rendering pass applies saved
  placement to line-line and circle-line distance dimensions.

#### Known Issue: Two-Line Angle Dimension Placement (2026-05-28)

- manual QA showed that the shared-endpoint two-line angle relation preview
  still does not hand off cleanly to the confirmed angle dimension. The ghost
  preview can appear at the intended cursor-relative radius, but the committed
  angle indicator can jump far away near the line ends and become effectively
  immovable.
- several incremental fixes were attempted around pending relation placement,
  label-position persistence, and angle arc radius reconstruction. They did
  not produce reliable behavior and should not be treated as the final design.
- follow-up should rewrite angle dimension placement as one explicit model:
  store a sketch-local angle presentation control point/radius for angle
  dimensions, have both ghost preview and committed rendering consume that same
  model, and remove the current after-the-fact inference from generic label
  placement.
