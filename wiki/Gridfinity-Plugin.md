# Gridfinity Plugin

The Gridfinity Generator is the first official PolySmith plugin. It is
available from the header's `Plugins` menu and configurable in
Settings -> Plugins.

## Supported V1 Models

- Bins: Gridfinity X/Y size, Z height in 7 mm units, equal compartments,
  wall/floor thickness, 1.5 mm divider thickness, 13 mm label ridge width,
  optional stacking lip, label ridge, per-row labels, grab curve approximation,
  configurable magnet holes, magnet-removal holes, and screw holes.
- Solid bins: Gridfinity X/Y size, Z height in 7 mm units, optional stacking
  lip, configurable magnet holes, magnet-removal holes, and screw holes.
- Light bins: Gridfinity X/Y size, Z height in 7 mm units, lightweight swept
  per-cell base profile, thin floor rings, light wall thickness, optional
  stacking lip, and optional label ridge.
- Holey bins: hole count in X/Y, circle/square/hexagon hole shape, hole size,
  hole depth, keepout diameter, optional stacking lip, configurable magnet
  holes, magnet-removal holes, and screw holes. X/Y grid size and Z height are
  derived from the hole settings, matching `gridfinitycreator`.
- Baseplates: Gridfinity X/Y size, thin or weighted base, optional drawer-fit
  width/depth, swept rounded-square cell profiles, magnet holes, and screw
  holes.

The generator follows the community Gridfinity unit system from
`gridfinity.xyz/specification/` and cross-checks dimensions against
`jeroen94704/gridfinitycreator`: 42 mm X/Y grid units, 7 mm Z units,
0.5 mm footprint tolerance, 2.25 mm floor, 1.9 mm wall thickness, 4.4 mm
stacking lip height, 1.5 mm dividers, 13 mm label ridge, 6.5 x 2 mm magnet
holes with optional 3.5 mm removal holes, and 3 x 6 mm M3 screw holes.
Like `gridfinitycreator`, the plugin caps generated parts to 6 grid units in X
or Y, bin heights to 2-12 height units, and compartments to 4 per grid unit.
Solid and light bins additionally allow a 1-height-unit base-only part,
matching their upstream generators.
Label ridge width is capped to half the compartment row depth so a label ridge
cannot close off the row.

For bins, the generated underside uses the repeated per-cell z-profile from the
design reference: 35.6 mm lower flat, 0.8 mm lower 45-degree transition,
1.8 mm vertical section, and 2.15 mm upper 45-degree transition into the
41.5 mm footprint. Corner callouts in the reference are treated as diameters,
so the bin uses 3.75 mm outer corner radius and 1.6 mm inner corner radius.
The stacking lip follows gridfinitycreator's classic-bin approach: a raised
outer wall with a straight wall-thickness receiver and only the top inside edge
chamfered open. This lets the lower 35.6/37.2 mm underside profile enter before
the upper chamfer centers it. Compartments are made from one
interior cutout plus explicit divider walls, matching gridfinitycreator's
separate divider-thickness setting rather than reusing the outside wall
thickness. The label ridge is an interior sloped shelf and can be repeated for
each compartment row. The grab curve is modeled as a sampled profile extrusion
inside each row using the generic `profile_extrude` plugin primitive. Magnet
holes are placed in all four corners of every grid unit.

Solid bins use the same repeated per-cell underside and hole options as bins,
but omit the interior compartment cutout, divider walls, label ridge, and grab
curve. When stacking lip is enabled, only the top lip pocket is cut out.

Light bins follow `gridfinitycreator`'s lightweight base approach. Each cell
uses the upstream swept base profile with 0, 0.8, 2.6, 3.15, and 4.75 mm
profile heights, a 41.5 mm rounded-square path, and a 0.9 mm floor ring with
a central cutout. The light wall thickness defaults to 1.5 mm.

Holey bins follow `gridfinitycreator`'s separate hole-grid generator. The
plugin derives X/Y grid units from `ceil((holeCount * keepoutDiameter +
2 * wallThickness + 0.5) / 42)` and derives height units from
`1 + ceil(holeDepth / 7)`. Holes are distributed evenly across the internal
footprint and cut from the top surface down by the requested depth.

For baseplates, each grid cell uses the same side-profile dimensions as
`gridfinitycreator`'s baseplate generator: 4.65 mm profile height, 2.25 mm
inner step, 2.85 mm profile wall, 2.5 mm mid-slope height, and 0.7 mm floor.
The plugin follows the upstream swept rounded-square path with the generic
`rounded_rect_profile_sweep` primitive. Thin baseplates are socket rails rather
than a filled floor plate; weighted baseplates add only the lower weighted body
under the same socket profile.

## Workflow

1. Choose Plugins -> Gridfinity Generator.
2. PolySmith creates a pending native plugin feature.
3. The floating panel edits parameters and sends live
   `update_plugin_feature` commands.
4. Confirm keeps the feature in history; Cancel undoes the pending feature.

## Boundaries

The plugin contributes UI and defaults, but the native core owns geometry,
history, recompute, viewport meshing, and export. Imported object cutouts,
lids, rugged boxes, baskets, and print-bed splitting are out of scope for v1.
