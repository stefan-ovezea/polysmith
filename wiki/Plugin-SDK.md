# Plugin SDK

The TypeScript SDK lives under `apps/desktop-ui/src/plugins/`.

## Define a Plugin

Use `definePlugin` with:

- `manifest`: `id`, `name`, `version`, `sdkVersion`, and `description`
- `defaultConfig`: JSON-serializable plugin defaults
- `activate(context)`: returns extension registrations

The activation context provides read-only `document` and `viewport` snapshots,
`sendCommand`, and `refreshViewport`.

## Configuration

Plugin config is not part of `config.json`. The plugin host bootstraps and
saves a separate document:

```json
{
  "plugins": {
    "polysmith.gridfinity": {
      "enabled": true,
      "config": {}
    }
  }
}
```

Settings panels receive `{ config, disabled, onChange }` and must emit a full
replacement config object.

## CAD Commands

Plugins that create geometry must use plugin feature IPC:

- `create_plugin_feature { plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`
- `update_plugin_feature { feature_id, plugin_id, feature_type, display_name, parameters_summary, parameters, geometry }`
- `confirm_plugin_feature { feature_id }`

`parameters` is the plugin-owned JSON state needed to rebuild the recipe.
`geometry` is a neutral recipe made of ordered operations:

```json
{
  "operation": "add",
  "primitive": "rounded_box",
  "x": 0,
  "y": 0,
  "z": 0,
  "width": 42,
  "depth": 42,
  "height": 21,
  "radius": 3
}
```

Supported operations are `add` and `subtract`. Supported primitives are `box`,
`rounded_box`, `tapered_rounded_box`, `cylinder`, `profile_extrude`, and
`rounded_rect_profile_sweep`.
`tapered_rounded_box` uses the normal `width`, `depth`, and `radius` as the
bottom profile and optional `top_width`, `top_depth`, and `top_radius` for a
top profile. Optional `top_offset_x` and `top_offset_y` shift that top profile
from the centered position, which lets plugins create one-direction printable
ramps while still using a generic lofted primitive. Recipe coordinates use
plugin-local X/Y for the footprint plane and Z for height. The core validates
and interprets this recipe as a CAD body; plugins must not generate mesh/STL
geometry in React, and the native core must not contain plugin-specific
modeling code.

`profile_extrude` takes a generic 2D polygon profile and an extrusion vector:

```json
{
  "operation": "add",
  "primitive": "profile_extrude",
  "x": 0,
  "y": 0,
  "z": 0,
  "profile_plane": "yz",
  "profile_points": [
    { "u": 0, "v": 0 },
    { "u": 12, "v": 0 },
    { "u": 12, "v": 4 }
  ],
  "extrude_x": 20,
  "extrude_y": 0,
  "extrude_z": 0
}
```

`profile_plane` is `xy`, `xz`, or `yz`; `u`/`v` are local coordinates in that
plane relative to `x`/`y`/`z`. The extrusion vector uses the same plugin-local
X/Y/Z axes.

`rounded_rect_profile_sweep` sweeps a `yz` profile around a rounded rectangle
path:

```json
{
  "operation": "add",
  "primitive": "rounded_rect_profile_sweep",
  "x": 0,
  "y": 0,
  "z": 0,
  "profile_plane": "yz",
  "profile_points": [
    { "u": 0, "v": 0 },
    { "u": 0, "v": 4 },
    { "u": 3, "v": 0 }
  ],
  "path_width": 42,
  "path_depth": 42,
  "path_radius": 4
}
```

The path uses plugin-local X/Y for the rounded rectangle footprint and Z for
height. The profile is placed at the first path tangent and swept by the native
core as a generic CAD body.

## Actions

Plugins can keep modeling workflows self-contained by returning menu items with
plugin-owned command strings, then handling those strings in the runtime's
`handleCommand` hook. The app supplies generic helpers such as
`sendCommand`, `refreshViewport`, and `awaitCreatedFeature`; the plugin returns
a generic active-action descriptor containing its `featureId`, `actionId`, and
plugin-owned `state`. The app stores that descriptor and calls the plugin's
`renderAction` hook to render the floating contextual panel.

The host must not switch on plugin ids for feature creation or panel rendering.
Bundled plugins use the same action path external plugins will use.
