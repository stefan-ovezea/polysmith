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
`rounded_box`, `tapered_rounded_box`, and `cylinder`.
`tapered_rounded_box` uses the normal `width`, `depth`, and `radius` as the
bottom profile and optional `top_width`, `top_depth`, and `top_radius` for a
top profile. Optional `top_offset_x` and `top_offset_y` shift that top profile
from the centered position, which lets plugins create one-direction printable
ramps while still using a generic lofted primitive. Recipe coordinates use
plugin-local X/Y for the footprint plane and Z for height. The core validates
and interprets this recipe as a CAD body; plugins must not generate mesh/STL
geometry in React, and the native core must not contain plugin-specific
modeling code.
