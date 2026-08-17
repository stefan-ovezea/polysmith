# Coordinate System Fix: Y-up → CAD Standard Z-up

## Status: Planning

## Problem

PolySmith uses a **game-engine coordinate convention** inherited from Three.js defaults where Y is the vertical/world-up axis. This conflicts with the **CAD standard** (ISO 10303 / right-hand rule) where Z is the vertical axis and the three canonical planes are:

| Plane | Normal | Spanned by | Sketch→World | Extrude direction |
|---|---|---|---|---|
| XY (front) | Z (0,0,1) | X, Y | (x,y) → (x, y, offset) | +Z |
| XZ (top) | Y (0,1,0) | X, Z | (x,y) → (x, offset, y) | +Y |
| YZ (right) | X (1,0,0) | Y, Z | (x,y) → (offset, x, y) | +X |

### Current state (incorrect for CAD)

| Plane ID | Normal | Sketch→World | Sketch axes | Extrude |
|---|---|---|---|---|
| `ref-plane-xy` | Y (0,1,0) | (x,y) → (x, 0, y) | Red + Blue | +Y |
| `ref-plane-yz` | X (1,0,0) | (x,y) → (0, x, y) | Green + Blue | +X |
| `ref-plane-xz` | Z (0,0,1) | (x,y) → (x, y, 0) | Red + Green | +Z |

### Target state (CAD standard)

| Plane ID | Normal | Sketch→World | Sketch axes | Extrude |
|---|---|---|---|---|
| `ref-plane-xy` | Z (0,0,1) | (x,y) → (x, y, offset) | Red + Green | +Z |
| `ref-plane-yz` | X (1,0,0) | (x,y) → (offset, x, y) | Green + Blue | +X |
| `ref-plane-xz` | Y (0,1,0) | (x,y) → (x, offset, y) | Red + Blue | +Y |

**Key change**: `ref-plane-xy` and `ref-plane-xz` swap normals. `ref-plane-yz` stays the same.

## Scope: ~25 files

The change is concentrated in **hardcoded plane frame definitions** and **coordinate mapping functions**. Code that accepts a `PlaneFrame` from a resolved source (construction plane, face) works correctly regardless of convention — only the hardcoded fallback values for the three origin reference planes need changing.

### C++ Core (~13 files)

#### 1. Plane frame definitions (most critical)

**`native/cad-core/src/core/geometry/impl/dependency_sketch_frame_helpers.inc`** (lines 58-108)
- Function `origin_plane_frame()`: Swap `ref-plane-xy` and `ref-plane-xz` frames
- `ref-plane-xy`: x_axis=(1,0,0), y_axis=(0,1,0), normal=(0,0,1) [was: y_axis=(0,0,1), normal=(0,1,0)]
- `ref-plane-xz`: x_axis=(1,0,0), y_axis=(0,0,1), normal=(0,1,0) [was: y_axis=(0,1,0), normal=(0,0,1)]
- Update comment block documenting the mapping

**`native/cad-core/src/core/geometry/impl/world_point_helpers.inc`** (lines 1-15)
- Function `to_world_point()`:
- `ref-plane-xy`: `gp_Pnt(local_x, local_y, 0.0)` [was: `gp_Pnt(local_x, 0.0, local_y)`]
- `ref-plane-xz`: `gp_Pnt(local_x, 0.0, local_y)` [was: `gp_Pnt(local_x, local_y, 0.0)`]

**`native/cad-core/src/core/viewport/impl/plane_frame_mapping_helpers.inc`** (lines 118-226)
- Viewport `to_world_point()`: swap XY and XZ mappings
- `make_face_frame_for_plane()`: swap XY and XZ frames

**`native/cad-core/src/core/geometry/impl/sketch_wire_extrude.inc`** (lines 92-112)
- `make_sketch_wire()`: swap XY and XZ PlaneFrame fallbacks

**`native/cad-core/src/core/geometry/impl/body_frame_helpers.inc`** (lines 106-123)
- `frame_from_plane_id()`: swap XY and XZ axis assignments

#### 2. Extrude direction

**`native/cad-core/src/core/geometry/impl/extrusion_vector_helpers.inc`** (lines 1-15)
- `extrusion_vector()`:
- `ref-plane-xy`: `gp_Vec(0.0, 0.0, depth)` [was: `gp_Vec(0.0, depth, 0.0)`] — extrude along Z
- `ref-plane-xz`: `gp_Vec(0.0, depth, 0.0)` [was: `gp_Vec(0.0, 0.0, depth)`] — extrude along Y

**`native/cad-core/src/core/document/impl/private_extrude_extent_resolution_helpers.inc`** (lines 1-15)
- `extrude_normal_vector()`: same swap as above

#### 3. Profile wire normals

**`native/cad-core/src/core/geometry/impl/profile_wire_normal_helpers.inc`** (lines 1-117)
- All 5 overloads of `plane_normal()` / `profile_wire_normal()`:
- `ref-plane-xy`: normal = `gp_Dir(0.0, 0.0, 1.0)` [was: `gp_Dir(0.0, 1.0, 0.0)`]
- `ref-plane-xz`: normal = `gp_Dir(0.0, 1.0, 0.0)` [was: `gp_Dir(0.0, 0.0, 1.0)`]
- Wire normal for XY: `gp_Dir(0.0, 0.0, -1.0)` [was: `gp_Dir(0.0, -1.0, 0.0)`] — cross product of new axes

#### 4. Feature helpers

**`native/cad-core/src/core/document/impl/private_extrude_frame_helpers.inc`** (lines 16-47)
- `plane_id_from_frame()`: Y-dominant → `ref-plane-xz`, Z-dominant → `ref-plane-xy` [swapped]

**`native/cad-core/src/core/document/impl/private_revolve_profile_parameter_helpers.inc`** (lines 13-19)
- Revolve axis resolution: swap XY and XZ cases

**`native/cad-core/src/core/document/impl/private_projection_command_helpers.inc`** (lines 74-92)
- Projection coordinate mapping: swap XY and XZ cases

#### 5. Viewport emit

**`native/cad-core/src/core/viewport/impl/extrude_polygon_emit.inc`** (lines 109-117)
- Polygon vertex emit: swap XY and XZ plane cases

**`native/cad-core/src/core/viewport/impl/extrude_rectangle_emit.inc`** (lines 87-111)
- Rectangle emit: swap XY and XZ plane cases

**`native/cad-core/src/core/viewport/impl/extrude_circle_emit.inc`** (lines 2-6)
- Circle emit: swap XY and XZ plane cases

**`native/cad-core/src/core/viewport/impl/world_bounds_helpers.inc`** (lines 21-27)
- World bounds for planes: swap XY and XZ axis extraction

### TypeScript UI (~8 files with direct changes, ~14 delegate automatically)

#### 1. Core coordinate mapping

**`apps/desktop-ui/src/utils/viewport/viewportMath.ts`** — 5 functions

| Function | Lines | Current | Change |
|---|---|---|---|
| `legacySketchPlane()` | 11-21 | XY: normal(0,1,0), XZ: normal(0,0,1) | Swap: XY→(0,0,1), XZ→(0,1,0) |
| `frameCameraToSketchPlane()` | 56-107 | XY: cam(0,d,0) up(0,0,-1), XZ: cam(0,0,d) up(0,1,0) | Swap camera positions and up vectors |
| `resolveSketchPlanePoint()` | 109-180 | XY: local=[x,z] world=[x,offset,z], XZ: local=[x,y] world=[x,y,offset] | Swap local extraction and world output |
| `toWorldPoint()` | 182-209 | XY: [x, offset, y], XZ: [x, y, offset] | XY→[x,y,offset], XZ→[x,offset,y] |

**`apps/desktop-ui/src/layout/viewport/grid.ts`** — 3 functions

| Function | Lines | Current | Change |
|---|---|---|---|
| `getSketchGridFrame()` | 254-320 | XY: origin(0,offset,0) yAxis(0,0,1) normal(0,1,0) | Swap with XZ |
| `worldPointToSketchLocal()` | 333-368 | XY: [world[0], world[2]] | XY→[world[0], world[1]], XZ→[world[0], world[2]] |
| `getCardinalGridFrame()` | 476-508 | Already determines frame from camera normal | No structural change needed |

**`apps/desktop-ui/src/layout/viewport/trimHoverPreview.ts`** — 2 functions (note: uses offset=0, not SKETCH_PLANE_OFFSET)

| Function | Lines | Change |
|---|---|---|
| `trimWorldPointToLocal()` | 62-68 | XY: [px, pz]→[px, py] etc. |
| `toWorldPoint()` | 111-131 | XY: [ux, 0, uy]→[ux, uy, 0], XZ: reverse |

**`apps/desktop-ui/src/utils/viewport/primitiveObjects.ts`** — 1 function

| Function | Lines | Change |
|---|---|---|
| `makePlaneTransformMatrix()` | 44-105 | Swap XY and XZ 4×4 matrices. Currently XY has determinant -1 (Y/Z swap). After fix, neither should have neg determinant. |

**`apps/desktop-ui/src/utils/viewport/sketchObjects.ts`** — 1 function

| Function | Lines | Change |
|---|---|---|
| `resolveSketchPlaneAxes()` | 33-65 | XY: yAxis(0,0,1)→(0,1,0), XZ: yAxis(0,1,0)→(0,0,1) |

**`apps/desktop-ui/src/layout/viewport/sketchProfilePicking.ts`** — 1 function

| Function | Lines | Change |
|---|---|---|
| `profileLocalPoint()` | 67-73 | XY: [hitPoint.x, hitPoint.z]→[hitPoint.x, hitPoint.y], XZ: reverse |

#### 2. Files that delegate (NO changes needed)

These 14 files use `toWorldPoint()` or `worldPointToSketchLocal()` as centralized functions. Once those are fixed, these work correctly:

`draftChordHint.ts`, `draftLinePreview.ts`, `endpointDrag.ts`, `snapResolution.ts`, `dimensionLabelDrag.ts`, `dimensionRelationPreviewGeometry.ts`, `draftPointerPreview.ts`, `arcDraftPreview.ts`, `circleDraftPreview.ts`, `draftDimensionPreview.ts`, `draftDimensionScreenPosition.ts`, `draftDimensionPostCommit.ts`, `dimensionPlacementActions.ts`, `dimensionRelationPlacementActions.ts`

#### 2. Camera and ViewCube

**`apps/desktop-ui/src/utils/viewCube.utils.ts`**

| Location | Lines | Change |
|---|---|---|
| FACE_NORMALS | 31-38 | TOP=(0,1,0)→(0,0,1), BOTTOM=(0,-1,0)→(0,0,-1), FRONT=(0,0,1)→(0,-1,0), BACK=(0,0,-1)→(0,1,0) |
| Face label mapping | 169-195 | Swap TOP↔FRONT and BOTTOM↔BACK on cube faces. Under Z-up: +Z is TOP, -Y is FRONT |
| `getDefaultUpForDirection()` | 584-590 | worldUp stays (0,1,0) — still valid as reference vector |

#### 3. CAM WCS (minor)

CAM backend is entirely stubbed (all C++ CAM commands are no-ops). WCS orientation is purely frontend state, never persisted.

**`apps/desktop-ui/src/layout/viewport/camSceneObjects.ts`** (lines 75-120)
- `addWcsOriginMarker()`: Under Z-up, default axes (X=right, Y=into-screen, Z=up) are correct for CAD. Remove or simplify the `z_up`/`y_up` reorientation since Z-up becomes the default.

**`apps/desktop-ui/src/App.tsx`** (line 327)
- Default `wcsOrientation` from `"model"` to `"z_up"`

### Files NOT changing

- Protocol serialization: Passes `PlaneFrame` verbatim, no hardcoded axes
- Construction plane derivation: Works with generic PlaneFrame
- Dependency refresh: Uses resolved PlaneFrame (except `origin_plane_frame` which IS listed)
- Body compilation / shape building: Uses `to_world_point(frame, ...)` which handles arbitrary frames

## Implementation Order

1. **C++ plane frames** — `origin_plane_frame()`, `to_world_point()`, `extrusion_vector()`, `plane_normal()`, etc.
2. **C++ viewport emit** — polygon/rectangle/circle emit, plane frame mapping
3. **C++ feature helpers** — extrude frame, extent resolution, revolve, projection
4. **Rebuild C++ core** — `pnpm core:rebuild`
5. **TypeScript coordinate mapping** — `toWorldPoint`, `resolveSketchPlanePoint`, `getSketchGridFrame`, etc.
6. **TypeScript viewport** — camera framing, ViewCube labels, CAM WCS
7. **TypeScript type-check** — `pnpm --filter desktop-ui exec tsc --noEmit`

## Verification

1. `pnpm core:rebuild` — C++ compiles
2. `pnpm --filter desktop-ui exec tsc --noEmit` — TypeScript type-checks
3. Run app, create new document:
   - XY plane sketch: red X + green Y axes, extrude goes +Z
   - XZ plane sketch: red X + blue Z axes, extrude goes +Y
   - YZ plane sketch: green Y + blue Z axes, extrude goes +X
4. ViewCube: TOP face = +Z, FRONT face = -Y
5. CAM: WCS origin marker shows Z as blue (up), tool axis = Z
6. Grid axis labels: correct spacing and values on all three planes
