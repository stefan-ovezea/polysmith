# Plugin System

PolySmith supports trusted plugins that extend the UI while keeping CAD state
inside the native core.

## Model

- Plugins are packages with a manifest, default configuration, optional
  settings UI, and one or more extension registrations.
- Plugin configuration is stored separately from app settings at
  `plugins/config.json` under the PolySmith user config directory.
- The React plugin host loads bundled trusted plugins, applies enablement and
  per-plugin config, and exposes menu/settings extension points.
- Plugins do not mutate React CAD state directly. Geometry-producing work goes
  through schema-backed IPC commands and the native plugin feature host.

## Current Extension Points

- `Plugins` menu entries in the app header.
- Settings panels under Settings -> Plugins.
- Plugin-backed CAD features through `create_plugin_feature`,
  `update_plugin_feature`, and `confirm_plugin_feature`.

## Native Feature Host

Runtime native DLL loading is intentionally out of scope for v1. Trusted
plugins provide a JSON-serializable geometry recipe with generic `add` /
`subtract` operations over primitive solids. The native core validates and
interprets that neutral recipe as a normal CAD feature, which preserves document
save/load, viewport, undo, recompute, hierarchy, and export behavior without
teaching the core about any plugin's domain.

The first bundled plugin is `polysmith.gridfinity`. All Gridfinity-specific
defaults, validation, and recipe generation live in the plugin package; the
core only sees `plugin_id`, `feature_type`, display metadata, serialized
plugin parameters, and the generic geometry recipe.

The desktop app only owns plugin host glue: loading trusted plugin runtimes,
showing their menu items, forwarding generic command selections, storing an
active plugin action descriptor, and asking the owning plugin to render its
context panel. It must not import a bundled plugin's command builders or panels
directly.
