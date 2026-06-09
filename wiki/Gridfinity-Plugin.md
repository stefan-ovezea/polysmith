# Gridfinity Plugin

The Gridfinity Generator is the first official PolySmith plugin. It is
available from the header's `Plugins` menu and configurable in
Settings -> Plugins.

## Supported V1 Models

- Bins: Gridfinity X/Y size, Z height in 7 mm units, equal compartments,
  wall/floor thickness, optional stacking lip, label tab, magnet holes, and
  screw holes.
- Baseplates: Gridfinity X/Y size, thin or weighted base, optional drawer-fit
  width/depth, magnet holes, and screw holes.

The generator follows the community Gridfinity unit system from
`gridfinity.xyz/specification/` and the unofficial specification repository:
42 mm X/Y grid units, 7 mm Z units, 41.5 mm nominal bin footprint tolerance,
6 x 2 mm magnets, and M3 screw holes.

For bins, the generated underside uses the repeated per-cell z-profile from the
design reference: 35.6 mm lower flat, 0.8 mm lower 45-degree transition,
1.8 mm vertical section, and 2.15 mm upper 45-degree transition into the
41.5 mm footprint. Corner callouts in the reference are treated as diameters,
so the bin uses 3.75 mm outer corner radius and 1.6 mm inner corner radius.
Magnet holes are placed in all four corners of every grid unit.

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
