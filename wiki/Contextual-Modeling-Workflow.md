# Contextual Modeling Workflow

PolySmith uses a contextual modeling workflow for its modeling interactions.
AI assistants and human contributors must follow the same UX pattern when
adding new features.

This document is binding. Diverging from this pattern requires an explicit
design discussion before implementation.

## Why This Pattern

This is a strong pattern for parametric CAD UX:

- it has a clear separation between "what is selected" and "what action is
  being performed"
- previews are real geometry computed by the kernel, not fake UI mockups
- inputs only appear once an action is in progress
- canceling an action restores the previous state cleanly

PolySmith honors that contract while keeping CAD logic in the native core.

## The Canonical Flow

For any non-trivial modeling feature (Extrude, Fillet, Pattern, Hole, ...):

1. **Select** the input(s).
   - The user selects a face, edge, profile, sketch entity, or feature.
   - Selection lives in the native core (`selected_feature_id`,
     `selected_sketch_profile_id`, `selected_face_id`, ...).
   - Hover feedback is a UI-only highlight; selection is owned by the core.

2. **Show a floating context panel.**
   - A small panel appears near the action source (header, sidebar slot, or
     near the cursor) showing only the parameters relevant to that action.
   - The panel must be subtle: it should not occupy the main inspector
     real estate or block the model.

3. **Live preview while editing.**
   - Every parameter edit dispatches an IPC update command (e.g.
     `update_extrude_depth`).
   - The viewport snapshot updates from the core. There are **no fake
     previews built only in React**.

4. **Confirm or cancel.**
   - **Confirm** (Enter or button): close the panel. The feature stays in
     the document with its current parameters.
   - **Cancel** (Escape or button): call `undo` to remove the in-progress
     edit/feature, then close the panel. The user must always be able to
     back out cleanly.

Notes:

- For extrude, the user selects a face first, then the extrude action is invoked, then the user can adjust the depth in the floating panel. But for Fillet or Chamfer, the user selects the action then he can chose to select edges or vertices.

### Fillet / Chamfer specifics

The two-phase flow above lands as follows for edge-input actions:

1. The user clicks **Fillet** (`F` hotkey) or **Chamfer** (button only — no
   hotkey) with no requirement to have any edge pre-selected.
2. The floating panel opens immediately in a **pending** phase. No feature
   exists in the document yet. The panel auto-focuses its numeric input,
   so the user can type a radius / distance straight away.
3. The user clicks edges in the viewport. The **first** click triggers
   `create_fillet` / `create_chamfer` with the currently-typed value and
   that single edge, transitioning the panel into the **active** phase.
   Every subsequent click toggles edge membership through
   `update_fillet_edges` / `update_chamfer_edges`. The body recompiles
   live so the user sees the fillet grow / shrink as they pick.
4. Editing the radius / distance in the panel during the active phase
   dispatches `update_fillet_radius` / `update_chamfer_distance`. The
   change applies to **every** edge currently in the feature
   retroactively — that's just how the feature is parameterised.
5. **Confirm** (Enter / button): close the panel; clear edge selection.
6. **Cancel** (Escape / button): if the panel never advanced past the
   pending phase, just close it (nothing to undo). Otherwise call `undo`
   to remove the freshly-created feature and every live edit made during
   the session.

If the user happens to have edges pre-selected when they invoke the
action (the "select-then-invoke" shortcut), the panel skips the
pending phase and creates the feature immediately with the existing
selection. The user-facing flow is the same from that point on.

The UI must own the active edge list optimistically while the panel is
open. Reading it back from the latest `document.feature_history` snapshot
inside an edge-click handler is unsafe under rapid clicking — the IPC
echo from the previous click may not have arrived yet, and the next
click will read a stale list and clobber earlier picks. The active-phase
state therefore mirrors `edge_ids` locally and updates it through a
functional `setState` so toggles compound correctly.

### Import Placement Specifics

Image and SVG imports use the same contextual pattern, but their input sequence
starts with a plane and a file instead of geometry selection alone:

1. The user invokes **Import > Image** or **Import > SVG**.
2. If a reference plane, construction plane, or planar face is already selected,
   the panel uses it. Otherwise the panel opens in a choose-plane phase.
3. File picking is disabled until the panel has a valid plane. For images, the
   UI sends `create_image_import` after the file is picked so the core can own
   the pending reference-image preview. For SVGs, the UI keeps only the selected
   source file path and placement form state until **Confirm**.
4. For images, the core owns the pending feature, copied asset reference,
   placement parameters, and viewport preview payload. The UI only renders that
   payload and edits plane-local X/Y offset, in-plane rotation, width/height,
   and aspect lock through the matching update command.
5. For SVGs, **Confirm** sends `create_svg_import`; the core converts supported
   vector geometry directly into one or more sketches on the selected plane.
   Ungrouped SVG geometry becomes one sketch; visible SVG groups/layers become
   separate sketches. SVG files are not kept as document assets or shown as
   pending asset previews.
6. **Confirm** closes the image panel and marks the reference image
   non-pending, or commits the SVG sketch import. **Cancel** calls the matching
   cancel command for pending image features and cleans staged image assets when
   no other feature references them. SVG cancel closes the panel without a core
   command because no pending SVG feature exists.

## Selection feedback rules

- Hover targets must show a clear, non-permanent visual change.
- Selectable surfaces such as solid faces must be invisible at rest and
  brighten on hover.
- Selected surfaces must remain highlighted until selection clears.
- Hover and selection styles must come from `applyXVisualState` helpers in
  `apps/desktop-ui/src/utils/viewport.utils.ts`. Do not write inline color
  overrides at the call site.

## Camera rules during sketch / face actions

- When the user enters a sketch, the camera frames the sketch plane
  face-on. The up vector is **world-aligned**, not the face's local Y axis.
- For nearly-vertical face normals, fall back to a stable secondary axis
  (e.g. world `-Z`) so the sketch reads consistently across faces.

## Inline numeric inputs

- Inline dimension inputs (post-line, post-circle, post-feature) must be
  small, frameless, and unobtrusive.
- They auto-focus and pre-select the current value.
- **Enter** commits and closes the inline input.
- **Escape** restores the prior value and closes the inline input.
- They must never block the canvas while the user is still drawing.

## Architecture rules (do not break)

- React UI must not own CAD state. The core owns documents, features,
  selection, sketch state, and parametric edits.
- All cross-boundary work uses the JSON IPC protocol. New commands need a
  schema entry **and** a doc update in `IPC-Protocol`.
- Live previews must be real geometry recomputed by the core. The UI may
  poll viewport snapshots; it must not invent geometry locally.

## What This Pattern Is Not

- It is not a 1:1 visual clone of another CAD product. PolySmith uses the
  `Midnight Carbon` design language (see `Design-System`).
- It is not a contract to expose every option from other CAD tools. We add parameters
  only when the core supports them.
- It is not a license to build modal blocking dialogs. Floating panels are
  preferred.

## Checklist for new features

When adding a contextual modeling action:

- [ ] Selection is owned by the core, not React.
- [ ] Hover feedback is provided through the existing `applyXVisualState`
      pattern.
- [ ] An IPC command creates the feature with sensible defaults.
- [ ] An IPC update command supports live edits (one per editable
      parameter is fine).
- [ ] The floating panel auto-focuses the primary input.
- [ ] Enter confirms; Escape cancels with `undo`.
- [ ] A hotkey is registered globally (e.g. `E` for Extrude).
- [ ] Schema, IPC docs, and the implementation log are updated together.
