# Emboss / Deboss — Design (follow-up, not yet implemented)

> Design notes for the Fusion-style Emboss feature. Sketch text was built
> to make this trivial: **text profiles are ordinary sketch profiles**, so
> emboss needs zero new profile machinery. This page captures the shape of
> the follow-up so it can land as its own small PR.

## Goal

Raise (emboss) or carve (deboss/engrave) a sketch profile — typically
text — onto the face(s) of a body, with constant depth along the face
normal.

## Feature shape (proposed)

```
emboss feature kind:
  EmbossFeatureParameters {
    source_sketch_feature_id   // owning sketch (profile source)
    profile_ids                // text profiles or any closed sketch profile
    target_body_id             // body to emboss onto
    depth                      // mm, > 0
    mode: "emboss" | "deboss"
    taper_angle                // optional, default 0
    is_pending                 // contextual-panel pending flag
  }
```

## Geometry (v1 — planar path)

The extrude machinery already implements every OCCT call needed:

1. `make_extrude_parameters_for_profile`
   (`core/document/impl/private_extrude_profile_parameter_helpers.inc`)
   snapshots the profile the same way extrude does.
2. Build the exact wire with `make_sketch_wire_exact`
   (`core/geometry/impl/sketch_wire_extrude.inc:195-266`) and the prism
   with `BRepPrimAPI_MakePrism` (as `make_wire_extrude_shape` does).
3. Boolean with the target body exactly like the body compiler's
   join/cut modes: `BRepAlgoAPI_Fuse` (emboss) / `BRepAlgoAPI_Cut`
   (deboss) + `SimplifyResult()` + `ShapeUpgrade_UnifySameDomain`
   (machinery in `compile_bodies_feature_insert.inc`). Reuse the
   existing `mode` / `target_body_id` fields pattern from
   `extrude_types.h`.

The sketch plane sits on/above the target face (sketch-on-face or a
construction plane), so the prism direction already follows the face
normal for planar faces. STEP/STL export works for free (BRep-based).

## Curved faces (follow-up of the follow-up)

For non-planar target faces the prism does not hug the surface. The
plan is normal projection, per the original research:

1. Project each profile wire onto the face with
   `BRepOffsetAPI_NormalProjection` (or pointwise projection +
   re-fitting).
2. Build the embossed wall between the source wire and the projected
   wire (pipe along the projection direction / loft between wires).
3. Boolean fuse/cut with the target body as above.

Fusion's "Tangent Chain" option (including tangentially-adjacent faces
in the wrap) is a further refinement — out of scope for the first cut.

## TNP / degradation

- The emboss re-resolves `target_body_id` + profile refs on every
  recompute in `refresh_history_dependencies`
  (`core/geometry/impl/refresh_profile_feature_dependencies.inc` is the
  pattern to extend — a new `emboss` block).
- Sketch edits follow the same rules as extrude: parametric text edits
  that preserve profile ids re-snapshot; destructive edits degrade with
  `dependency_broken` + a timeline warning (see the TNP table in
  [Text-Tool-Implementation-Plan](Text-Tool-Implementation-Plan)).
- The body-compiler skip of broken features
  (`compile_bodies_modifier_replay.inc:9`) already applies.

## UI

Contextual workflow like every other feature: select the text profiles
→ Emboss action (Modify or Create ribbon) → floating panel with
depth / emboss-deboss toggle / taper → live core-computed preview →
Enter confirm, Escape cancel (undo). Text profiles are pickable today
(the same profile picking extrude uses).
