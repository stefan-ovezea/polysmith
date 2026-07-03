# Sketch Dimension — Implementation Status

**Date:** 2026-06-16
**Branch:** `dimensions-2`

## Overview

The Dimension tool has a split-button dropdown in the Sketch toolbar with 7 sketch modes (10 drawing-sheet modes hidden in Sketch, visible only in the future ISO Drawing workspace). The mode value is threaded through the full pointer-up pipeline into `handleDimensionToolClick` where a mode-aware `switch` scaffold is ready for future per-mode logic.

## Architecture

```
App.tsx (state: dimensionToolMode)
  └─> SketchToolbar.tsx / SketchToolPanel.tsx (dropdown UI, sketch modes only)
  └─> ViewportPanel.tsx (ref: dimensionToolModeRef)
       └─> handlePointerUp
            └─> ViewportPointerUpParams.dimensionToolMode
                 └─> ActiveSketchPointerUpContext.dimensionToolMode
                      └─> DimensionToolClickContext.dimensionToolMode
                           └─> handleDimensionToolClick (mode switch)
                                └─> dimensionEntityPickAction / dimensionPointPickAction
```

## Dropdown Modes — Sketch vs Drawing

### Sketch-visible (7 modes)

| Mode | Functional? | What happens |
|------|------------|-------------|
| `auto` | ✅ Fully | Smart detection: line→`line_length`, circle→`circle_radius`, two lines→`angle`/`line_line_distance`, two points→`point_distance`, polygon→`polygon_radius` |
| `aligned` | ⚠️ Falls through to auto | All dimensions are inherently true-length aligned — works identically to auto |
| `angular` | ⚠️ Falls through to auto | Auto already handles angles, but mode doesn't restrict to angle-only picking |
| `radius` | ⚠️ Falls through to auto | Creates `circle_radius`/`polygon_radius` but doesn't reject lines or arcs |
| `diameter` | ⚠️ Falls through to auto | Creates `circle_radius` with diameter display (`display_as` toggle). No dedicated `diameter` dimension kind in core |
| `linear` | ❌ Falls through to auto | No horizontal/vertical projection detection — just creates aligned dims |
| `arc_length` | ❌ No-op for arcs | Arcs are caught early and consumed without error, but no `arc_length` dimension kind exists |

### Drawing-only (10 modes, hidden in Sketch)

`ordinate`, `jogged_radial`, `curve_min_max`, `baseline`, `chain`, `tidy_up`, `arrange`, `flip_arrows`, `match`, `dimension_break`

All reserved for future ISO Drawing workspace. Visible only when `workspaceView === "drawing"`.

## Core Dimension Kinds

### Implemented (9 kinds)

| Kind | Created by | Solver constraint | Notes |
|------|-----------|-------------------|-------|
| `line_length` | Clicking a line | ✅ P2PDistance | Auto-created on line commit |
| `line_angle` | Auto on line commit | ✅ P2PAngle | No IPC create command — auto-only |
| `circle_radius` | Clicking a circle | ✅ CircleRadius | Supports diameter display via `display_as` |
| `polygon_radius` | Clicking a polygon | ❌ Direct C++ drive | Not GCS-constrained |
| `angle` | Clicking two non-parallel lines | ✅ L2LAngle | Stored as signed angle |
| `point_distance` | Clicking two points | ✅ P2PDistance | |
| `line_line_distance` | Clicking two parallel lines | ❌ Driven-sync only | Not GCS-constrained |
| `circle_center_distance` | Clicking two circles | ✅ P2PDistance (on centers) | |
| `circle_line_distance` | Clicking circle + line | ✅ C2LDistance | |

### Not Implemented (needed for dropdown mode completion)

| Kind | Needed for | Notes |
|------|-----------|-------|
| `arc_radius` | `radius` mode on arcs | Arcs have no constraint/dimension support at all (`sketch_geometry_types.h:62`) |
| `arc_length` | `arc_length` mode | Arc curve length measurement |
| `horizontal_distance` | `linear` mode (X projection) | Could use existing `line_length`/`point_distance` with projection |
| `vertical_distance` | `linear` mode (Y projection) | Same as above |
| Dedicated `diameter` | `diameter` mode | Currently reuses `circle_radius` with `display_as: "diameter"` |

## Other Known Gaps

| Gap | Detail |
|-----|--------|
| **No arc dimensions** | Arcs have no constraint/dimension support — clicking an arc in dimension mode is a graceful no-op |
| **No mode filtering** | `angular`/`radius`/`diameter`/`arc_length` modes don't restrict picking — they accept any entity and fall through to auto behavior |
| **No solver constraint for polygon_radius** | Polygon radius drives geometry via direct C++ code, not through the GCS solver |
| **No solver constraint for line_line_distance** | Parallel-line distance is driven-sync only — no GCS constraint enforces it |
| **No horizontal/vertical detection** | `linear` mode has no projection logic — everything is true-length aligned |

## Mode Switch Scaffold

Located in `dimensionToolPicking.ts:handleDimensionToolClick`:

```typescript
switch (context.dimensionToolMode) {
  case "auto":         break;  // current smart behavior
  // Sketch modes       ↓       fall through to auto
  case "linear":       break;
  case "aligned":      break;
  case "angular":      break;
  case "radius":       break;
  case "diameter":     break;
  case "arc_length":   break;
  // Drawing modes      ↓       fall through to auto
  case "ordinate":     break;
  case "jogged_radial":break;
  case "curve_min_max":break;
  case "baseline":     break;
  case "chain":        break;
  case "tidy_up":      break;
  case "arrange":      break;
  case "flip_arrows":  break;
  case "match":        break;
  case "dimension_break": break;
}
```

Each mode has a dedicated `case` block ready for implementation. To add mode-specific behavior, add filtering/picking logic in the case and `return` early before reaching the auto-detection chain.

## Files

| File | Role |
|------|------|
| `apps/desktop-ui/src/layout/viewport/dimensionToolPicking.ts` | Click-handling logic, entity/point dispatch, mode switch |
| `apps/desktop-ui/src/layout/viewport/dimensionToolActions.ts` | Create/select functions that send IPC dimension commands |
| `apps/desktop-ui/src/layout/viewport/pointerUpActiveSketch.ts` | Routes pointer-up to dimension handler with mode context |
| `apps/desktop-ui/src/layout/viewport/viewportPointerUp.ts` | `ViewportPointerUpParams` interface with `dimensionToolMode` |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Ref creation, pointer-up param construction |
| `apps/desktop-ui/src/layout/header/SketchToolbar.tsx` | Dropdown (sketch modes only) |
| `apps/desktop-ui/src/layout/viewport/SketchToolPanel.tsx` | Floating panel dropdown (sketch modes only) |
| `apps/desktop-ui/src/types/geometry/sketch.ts` | `DimensionToolMode` type (all 17 modes) |
| `apps/desktop-ui/src/i18n/en.json` | Translation keys |
| `wiki/Dimension-Tool-Split-Button-Plan.md` | Original planning document |
