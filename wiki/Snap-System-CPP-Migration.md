# Snap System — C++ Migration Plan

> **Goal:** Move ALL snap computation from TypeScript (`ViewportPanel.tsx`) to C++ (`snap_engine.cpp`).
> The TS layer becomes a pure renderer of C++-resolved snap results.
>
> **Branch:** `snap-cpp-migration` (from `constraints`)

---

## 1. Current Split

| Layer | What it computes |
|---|---|
| **C++** `snap_engine.cpp` | Static candidates: endpoint, midpoint, center, quadrant, nearest (line/circle), intersection, grid, grid-line, perpendicular-foot, tangent-to-circle, polar-angle |
| **TS** `ViewportPanel.tsx` | Dynamic direction locks: parallel-to-line, perpendicular-to-line, H/V axis lock, line-body (collinear "ℓ"), snap priority ordering |

The TS runs at 60fps without IPC. The C++ runs on document state changes. They disagree about snap priority — e.g. parallel snap overrides collinear snap.

---

## 2. Target Architecture

```
Cursor move (TS) 
    → IPC: resolve_draft_snap(cursor_x, cursor_y, start_x, start_y, tool, filter)
    → C++: snap_engine collects ALL candidates (static + dynamic)
    → C++: resolves best candidate by priority
    → returns: { snap_x, snap_y, snap_kind, snap_label, constraint_preview_kind, host_entity_id }
    → TS: updates rubber-band + preview badge
```

One new IPC command: `resolve_draft_snap`. The TS calls it on every cursor move during drafting. The C++ returns a snap result or `null`.

---

## 3. Dynamic Snaps to Add in C++

### 3a. Parallel Direction Lock (NEW)
```
Input: cursor, start_point, sketch_lines[]
Logic:
  - For each non-construction line, compute its angle
  - Find the line whose angle is closest to the cursor ray from start
  - Project cursor onto that direction line through start
  - If distance < tolerance, return projected point
Priority: BELOW line-body (collinear)
```

### 3b. Perpendicular Direction Lock (NEW)
```
Input: cursor, start_point, sketch_lines[]
Logic:
  - Same as parallel but with angle + 90°
  - Project cursor onto perpendicular direction through start
Priority: BELOW line-body, ABOVE parallel
```

### 3c. H/V Axis Lock (NEW)
```
Input: cursor, start_point
Logic:
  - If cursor is nearly horizontal from start (within 3°), lock Y to start_y
  - If cursor is nearly vertical from start, lock X to start_x
  - Then ray-cast from start along locked axis to find line crossings
Priority: HIGH — just below endpoint/center snap
```

### 3d. Line-Body Snap / Collinear (EXISTS in TS, MOVE to C++)
Already partially handled by `collect_nearest_candidates`. Extend to return `host_line_id` + `t` value for constraint creation.

---

## 4. IPC Contract

### New command: `resolve_draft_snap`
```json
{
  "id": "...",
  "type": "resolve_draft_snap",
  "payload": {
    "cursor_x": 10.5,
    "cursor_y": 20.3,
    "start_x": 5.0,
    "start_y": 15.0,
    "tool": "line"
  }
}
```

### Response: `draft_snap_resolved`
```json
{
  "id": "...",
  "type": "draft_snap_resolved",
  "payload": {
    "snap_x": 12.0,
    "snap_y": 18.0,
    "snap_kind": "parallel",
    "snap_label": "Parallel",
    "constraint_preview_kind": null,
    "host_entity_id": "line-2",
    "host_point_id": null
  }
}
```

Or `null` payload if no snap.

---

## 5. TS-Side Cleanup

After C++ migration, REMOVE from `ViewportPanel.tsx`:

1. **Parallel snap** (~lines 5112–5167) — entire block
2. **Axis lock H/V** (~lines 4970–5110) — entire block  
3. **Line-body snap** (~lines 5187–5260) — entire block
4. **Snap priority ordering** — now owned by `kDefaultSnapPriority` in C++
5. **`buildAxisLockSnap`** helper function
6. **`readDimensionPreviewFilter`** parallel/axis overrides

Keep in TS:
- Rubber-band rendering (pure visual)
- Constraint preview badge rendering (reads C++ result)
- Hover highlight (reads C++ result)

---

## 6. Implementation Phases

### Phase 1 — Foundation (C++ only)
- [ ] Add `resolve_draft_snap` IPC command + response type to `app.cpp`
- [ ] Add `draft_snap_resolved` event to `protocol/schema/`
- [ ] Add TS types for new command/response in `ipc.ts`
- [ ] Wire IPC builder in `ipcProtocol.ts`
- **Test:** Send a `resolve_draft_snap` from TS, verify C++ responds with existing static snap (endpoint/midpoint)

### Phase 2 — Dynamic snaps in C++
- [ ] Add `collect_parallel_candidates(cursor, start, sketch, tolerance, filter, candidates)`
- [ ] Add `collect_perpendicular_direction_candidates(cursor, start, sketch, tolerance, filter, candidates)` 
- [ ] Add `collect_axis_lock_candidates(cursor, start, tolerance, candidates)` — H/V lock + line crossing
- [ ] Extend `resolve_snap` to call new collectors (gated by `filter.snap_parallel`, `filter.snap_perpendicular`)
- [ ] Update `SelectionFilter` — add axis-lock toggle (`snap_axis_lock`, default `true`)
- **Test:** C++ unit test: cursor near parallel direction → returns parallel snap candidate

### Phase 3 — TS integration
- [ ] TS calls `resolve_draft_snap` on cursor move (debounced ~16ms / 60fps)
- [ ] TS uses C++ response for rubber-band position
- [ ] TS uses C++ response for constraint preview badge
- [ ] TS uses C++ response for `host_entity_id` / `host_point_id` for post-commit constraint creation
- **Test:** Draw a line, snap to midpoint → rubber-band + preview badge work exactly as before

### Phase 4 — TS cleanup
- [ ] Remove TS parallel snap code
- [ ] Remove TS axis lock code
- [ ] Remove TS line-body snap code
- [ ] Remove TS snap priority ordering
- [ ] Remove `readDimensionPreviewFilter` overrides for snap
- **Test:** Full regression: all snap types work, preview badges correct, constraints created from snaps

### Phase 5 — Constraint preview unification
- [ ] C++ `resolve_draft_snap` returns `constraint_preview_kind` ("horizontal", "vertical", "perpendicular", "tangent", "midpoint", "on_line", "parallel")
- [ ] TS constraint preview badge reads this field directly (no TS-side inference)
- **Test:** Constraint preview badges match exactly what will be committed

---

## 7. Snap Priority (Final Order)

```
1. endpoint        — highest
2. center
3. midpoint
4. axis_lock       — H/V axis lock from start point
5. intersection
6. quadrant
7. line_body       — collinear "ℓ" on line segment (was "nearest")
8. perpendicular   — perpendicular foot from cursor to line
9. perp_direction  — perpendicular direction lock from start
10. tangent
11. parallel       — parallel direction lock from start
12. polar
13. grid
14. grid_line
15. nearest        — lowest (catch-all body snap)
```

---

## 8. Building Blocks for Future Tools

Complex shapes decompose to **atomic building blocks** recognized by the snap system:

| Complex shape | Decomposes to |
|---|---|
| Rectangle | 4 lines (with equal-length + H/V constraints auto-applied) |
| Polygon | N lines |
| Circle | 1 circle + center point + 4 quadrant points |
| Arc | 1 arc + 2 endpoint points |
| TEXT | Set of lines + arcs (font glyph decomposition) |
| Spline / Curve | Approximated as lines + arcs |
| DXF Import | Set of lines + circles + arcs + points |
| Array | N copies of the source building blocks |

The snap system operates on the **decomposed** building blocks, never on the complex shape directly. This is already how rectangles work (they decompose to 4 lines via `add_sketch_line`).

**Future-proofing in snap_engine:**
- All collectors already iterate `sketch.lines`, `sketch.circles`, `sketch.arcs`
- No change needed for new entity types — if they produce lines/circles/arcs/points, snaps work automatically
- For entities that produce new geometric types (e.g. NURBS surfaces), a new collector would be added

---

## 9. Edge Cases & Risk

| Risk | Mitigation |
|---|---|
| IPC latency at 60fps | Debounce in TS; C++ snap is <1ms computation |
| Snap flicker at boundary | Hysteresis: keep previous snap if new snap changes kind within 2px |
| Rectangle/polygon corner merging | `snap_line_endpoints_to_coincident_geometry` still handles this in C++ |
| Overlapping invisible line return | Already fixed — start point not auto-merged |
| Tool-specific snap behavior | `tool` field in IPC command allows per-tool filtering |
