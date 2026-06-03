# Topological Naming Problem — Strategy & Defenses

> **This is PolySmith's mantra.** The topological naming problem (TNP) is
> the most notorious class of bugs in parametric CAD. When you edit an
> upstream feature, the OCCT kernel may renumber faces, edges, and
> vertices — breaking every downstream feature that referenced them by
> index. A CAD application that doesn't handle TNP will silently produce
> garbage geometry or explode into errors. PolySmith must never ship a
> feature that introduces new unhandled TNP exposure. Every new feature
> kind must either remain topology-independent or re-resolve its

> **The core owns the document. The UI owns the interaction. Never
> confuse them.** If it moves with the mouse, it's UI. If it saves to a
> file, it's core.

> **Never store a naked OCCT topology index and trust it across
> recomputes.** Every feature that references 3D geometry must re-resolve
> its references against live body shapes on every recompute.

---

## What TNP Looks Like

```
1. Sketch a box → extrude → 6 faces: face-0, face-1, ..., face-5
2. Create a sketch on face-2 (the top face)
3. Extrude it → new body with a boss on top

4. Now edit step 1: add a fillet to one edge of the original box
5. OCCT renumbers faces: the old face-2 may now be face-7
6. The downstream sketch is still referencing "face-2" → garbage
```

This is FreeCAD's most infamous bug. It affects every CAD that builds on
top of a boundary-representation kernel (OCCT, Parasolid, ACIS) without
an explicit TNP strategy. Commercial CADs have invested years in solving
this; PolySmith must avoid it from day one.

---

## Strategy: Re-Resolve, Never Trust Stored Indices

The core rule: **never store a topology index and assume it will be valid
later.** Instead, store a stable reference (feature id + a semantic
identifier) and re-resolve it against the live body shape on every
recompute.

PolySmith uses the dependency walker `refresh_history_dependencies` as
the single recompute pass. Every feature's dependency resolution runs
inside this pass, in feature-history order, ensuring all upstream bodies
are fully compiled before downstream features try to read them.

---

## Existing Defenses (Verified in Source)

### 1. Fillet / Chamfer: Edge Re-Resolution

**Files:** `feature.h` (lines 69-98), `body_compiler.cpp`

Edge references use the format `<body_id>:edge:<index>` where `index` is
OCCT's `TopExp::MapShapes` enumeration order. The body compiler calls
`TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map)` on the **live**
target body shape at the moment the feature is replayed — not on the
shape that existed when the user selected the edge.

During the picking session, `is_pending` keeps the pre-fillet shape on
the compiled body so edge IDs remain stable while the user adjusts
parameters. Only after `confirm_fillet` / `confirm_chamfer` does the
body compiler apply the feature permanently.

**TNP resilience:** If an upstream edit changes the topology (e.g.
adding a fillet elsewhere shifts edge indices), the compiler may resolve
to a different edge. This is acceptable — the stored index is a *hint*,
and the worst case is the wrong edge gets filleted, which the user can
correct. The application never crashes or produces invalid geometry.

### 2. Offset Construction Plane: Face Frame Re-Resolution

**Files:** `feature.h` (lines 100-125), `refresh_dependents.cpp`

Face-based construction planes use `<body_id>:face:<index>`. The
dependency walker re-derives the `PlaneFrame` against the compiled body
before re-computing the plane position. Chained offsets work naturally
because each plane resolves into a real frame during the walk.

**TNP resilience:** If the source face no longer exists (consumed by a
boolean cut, deleted), the plane stays at its last-known frame and
`dependency_broken` is set. The timeline shows a warning; the feature
doesn't crash.

### 3. Project Tool: Source Re-Projection

**Files:** `feature.h` (lines 226-260), `refresh_dependents.cpp`

Every projection link (`SketchProjection`) stores the source body id
and re-resolves the projected geometry on every recompute via
`refresh_sketch_projections`. The source is identified by its
`<body>:<kind>:<index>` id, resolved against the compiled body, and the
generated sketch entities' coordinates are rewritten in place.

If resolution fails — body deleted, edge type changed, face consumed —
`dependency_broken = true` and the entities freeze at their last-known
coords. The timeline surfaces a human-readable warning.

### 4. Feature-Level Dependency Tracking

**Files:** `feature.h` (lines 461-493), `document.cpp`

Every `FeatureEntry` carries:
- `dependency_broken` — set when upstream geometry can't be resolved
- `dependency_warning` — human-readable explanation for the timeline tooltip
- `suppressed` — excluded from body compilation while retaining its ID

Body compiler and viewport builder skip features with `dependency_broken`
or `status == "warning"`, silently falling back to last-known state. The
model never explodes — it degrades gracefully with visible warnings.

### 5. Feature Suppression

**Files:** `feature.h` (line 473), `document.cpp`

Suppressed features are excluded from body compilation but retain their
ID in the feature history. Downstream features referencing a suppressed
parent silently no-op via the existing "missing input" fallbacks. This
lets the user temporarily disable a feature without destroying
topological continuity for everything downstream.

---

## Rules for New Features (the "Mantra")

Every new feature kind that references 3D topology must follow these
rules:

1. **Never store a naked OCCT topology index as a permanent reference.**
   Store the owning feature id + a semantic tag (edge/face/vertex +
   index) as a string, and re-resolve against the compiled body on every
   recompute.

2. **Always call `refresh_history_dependencies` after any geometry
   mutation.** This single pass walks the feature tree in order and
   gives every feature a chance to re-resolve its dependencies against
   the latest compiled state.

3. **Gracefully degrade when resolution fails.** Set
   `dependency_broken = true`, freeze at last-known geometry, and write
   a human-readable `dependency_warning`. Never throw, crash, or
   silently produce garbage.

4. **Test TNP scenarios.** For every new feature, write a manual test
   that edits an upstream feature the new feature depends on, and verify
   the result is either correct re-resolution or a clear warning.

5. **Sketch-to-sketch references are topology-independent.** Sketch
   entity ids (`line-N`, `circle-N`, etc.) are stable within their
   owning sketch feature and are never affected by body topology
   changes. Dimensions, constraints, fillets, and projections within a
   sketch are TNP-safe by construction.

---

## Known Gaps (v1)

| Gap | Severity | Mitigation |
|---|---|---|
| Edge indices in fillet/chamfer can shift after upstream topology changes | Medium | Compiler re-resolves per replay; wrong-edge fillet is user-visible and correctable |
| Face indices in face-based sketches can shift | Medium | `dependency_broken` flag + last-known frame fallback |
| No explicit "named" topology (e.g. "the face opposite this one") | Low | v1 stays index-based; named topology is a follow-up |
| No persistent edge/face identity across suppression | Low | Suppressed features retain their ID but not their topology |

---

## References

- `native/cad-core/src/core/feature.h` — FeatureEntry, all parameter structs
- `native/cad-core/src/core/body_compiler.cpp` — Per-replay edge/face resolution
- `native/cad-core/src/core/refresh_dependents.cpp` — Dependency walker, projection refresh
- `native/cad-core/src/core/document.cpp` — `refresh_history_dependencies`, bump, undo/redo
