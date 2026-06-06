# Active Task: Remove C++ Snap from IPC & Move Drag to UI

**Started:** 2026-06-03
**Status:** In Progress (Steps 1-11 complete — drag preview done, UI toggles removed)
**Source:** `.deepseek/instructions.md` → Active Priority section
**Principle:** `wiki/Core-UI-Design-Principles.md` — Core sends DOCUMENT STATE, not INTERACTION STATE

## Goal

Remove `resolve_draft_snap` and `drag_sketch_point` IPC paths. The C++ snap engine must not send per-frame snap candidates to the UI. Drag must be handled entirely in the TypeScript UI layer until mouse-up.

## Steps

### Step 1: C++ — Remove `resolve_draft_snap` handler ✅
- `native/cad-core/src/app.cpp`: deleted handler block + `#include "core/snap_engine.h"`
- Verified: `cmake --build` passes

### Step 2: C++ — Remove `drag_sketch_point` handler ✅
- `native/cad-core/src/app.cpp`: deleted handler block
- Verified: `cmake --build` passes

### Step 3: C++ — Remove `drag_sketch_point` from document ✅
- `native/cad-core/src/core/document.cpp`: deleted method + `#include`
- `native/cad-core/src/core/document.h`: deleted `DragPointResult` + declaration
- Verified: `cmake --build` passes

### Step 4: C++ — Move `SnapCandidate`, delete snap_engine ✅
- Moved `SnapCandidate` struct into `viewport.h`
- Deleted `snap_engine.h` and `snap_engine.cpp`
- Updated `CMakeLists.txt` (3 source lists)
- Verified: `cmake --build` passes

### Step 5: TS — Remove IPC protocol functions ✅
- `ipcProtocol.ts`: removed `makeDragSketchPointCommand`, `makeResolveDraftSnapCommand`
- `types/ipc.ts`: removed 4 interfaces + union members
- `schemas/ipcSchema.ts`: removed 2 schemas
- `aiCommandProtocol.ts`: removed `"resolve_draft_snap"`
- Verified: `tsc --noEmit` passes (after Step 6)

### Step 6: TS — Remove useCadCore plumbing ✅
- Removed `dragSketchPoint`, `resolveDraftSnap` functions
- Removed `draft_snap_resolved` and `drag_snap_result` event dispatches
- Removed imports
- Verified: `tsc --noEmit` passes

### Step 7: TS — Remove ViewportPanel C++ snap dependencies ✅
- Removed `onResolveDraftSnap` prop and call site
- Removed `cppSnapCacheRef` and C++ cache fast-path in `resolveSnappedSketchPoint`
- Removed `polysmith-cpp-snap` event listener
- Verified: `tsc --noEmit` passes

### Step 8: TS — Remove drag IPC from ViewportPanel ✅
- Removed `onDragSketchPoint` prop
- Removed `dragSketchPointRef` and rAF batch call (replaced with local snap)
- Removed `polysmith-drag-snap` event listener
- Removed `pendingSnapHighlightRef`
- Removed `snapKindToBadgeKind`
- Added null snap fields to TS fallthrough returns
- Verified: `tsc --noEmit` passes

### Step 9: TS — Implement local drag preview ✅
- During endpoint drag: `resolveSnappedSketchPoint` called locally
- Result stored in `dragSnapResultRef` for pointerup commit
- Snap label updated via `setSketchSnapLabel`
- Dashed line overlay rendered from anchored endpoints to current snapped position
- Preview lines cleared on pointerup, tool switch, and renderer teardown
- Verified: `tsc --noEmit` passes

### Step 10: Cleanup — Remove snap engine UI toggles ✅
- `SelectionFilterPanel.tsx`: removed per-tool snap engine fields, defaults, `snapToolRows`, and UI section
- `en.json`: removed 8 related i18n strings
- Verified: `tsc --noEmit` passes

### Step 11: Protocol schema ✅
- `protocol/schema/commands.schema.json`: removed `"resolve_draft_snap"`
- Verified: diff check

## Progress Log

### 2026-06-03 — Session 1
- Read `.deepseek/instructions.md` and pastes
- Mapped all affected files
- Built step-by-step plan
- Completed Steps 1-8: All C++ IPC handlers removed, all TS plumbing removed
- C++ build passes, TS type-check passes with zero errors
- Step 9 (drag preview) partially done — local snap resolves during drag, result stored for commit
- Step 10 (UI toggles) not started

### Session 2 — 2026-06-03 (30 min)
- Completed Step 9: drag preview rendering with dashed line overlay
- Completed Step 10: removed snap engine UI toggles
- All 11 steps complete. C++ build passes, TS type-check zero errors.

### Remaining work
- Functional testing in the running app — verify endpoint drag works end-to-end
- Consider re-adding dynamic snaps (axis_lock, parallel, perpendicular, tangent) as a TS-side follow-up. Currently only static snaps (endpoint, midpoint, center, etc.) are resolved in TS.

### Key files changed
- `native/cad-core/src/app.cpp` — removed 2 handlers
- `native/cad-core/src/core/document.cpp` — removed `drag_sketch_point` method
- `native/cad-core/src/core/document.h` — removed `DragPointResult`, declaration
- `native/cad-core/src/core/viewport.h` — added `SnapCandidate` struct
- `native/cad-core/src/core/snap_engine.h` — DELETED
- `native/cad-core/src/core/snap_engine.cpp` — DELETED
- `native/cad-core/CMakeLists.txt` — removed snap_engine.cpp references
- `apps/desktop-ui/src/hooks/useCadCore.ts` — removed snap/drag functions + event dispatch
- `apps/desktop-ui/src/lib/ipcProtocol.ts` — removed 2 command builders
- `apps/desktop-ui/src/types/ipc.ts` — removed 4 interfaces
- `apps/desktop-ui/src/lib/schemas/ipcSchema.ts` — removed 2 schemas
- `apps/desktop-ui/src/lib/aiCommandProtocol.ts` — removed command
- `apps/desktop-ui/src/App.tsx` — removed prop wiring
- `apps/desktop-ui/src/layout/ViewportPanel.tsx` — major cleanup: props, refs, event listeners, cache
- `protocol/schema/commands.schema.json` — removed command

