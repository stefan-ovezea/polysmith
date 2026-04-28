# Fusion-Style Behavior Pattern

PolySmith intentionally mirrors **Autodesk Fusion 360** for its modeling
interactions. AI assistants and human contributors must follow the same UX
pattern when adding new features.

This document is binding. Diverging from this pattern requires an explicit
design discussion before implementation.

## Why Fusion

Fusion is a strong reference for parametric CAD UX:

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

2. **Invoke the action.**
   - The user clicks an action button or presses its hotkey.
   - The UI sends an IPC command that creates the feature in the core with
     **default parameters**.
   - The core performs a real recompute and the viewport renders the result.

3. **Show a floating context panel.**
   - A small panel appears near the action source (header, sidebar slot, or
     near the cursor) showing only the parameters relevant to that action.
   - The panel must be subtle: it should not occupy the main inspector
     real estate or block the model.

4. **Live preview while editing.**
   - Every parameter edit dispatches an IPC update command (e.g.
     `update_extrude_depth`).
   - The viewport snapshot updates from the core. There are **no fake
     previews built only in React**.

5. **Confirm or cancel.**
   - **Confirm** (Enter or button): close the panel. The feature stays in
     the document with its current parameters.
   - **Cancel** (Escape or button): call `undo` to remove the in-progress
     edit/feature, then close the panel. The user must always be able to
     back out cleanly.

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
  schema entry **and** a doc update in `docs/architecture/ipc-protocol.md`.
- Live previews must be real geometry recomputed by the core. The UI may
  poll viewport snapshots; it must not invent geometry locally.

## What "Fusion-style" is not

- It is not a 1:1 visual clone of Fusion. PolySmith uses the
  `Midnight Carbon` design language (see `docs/DESIGN.md`).
- It is not a contract to expose every Fusion option. We add parameters
  only when the core supports them.
- It is not a license to build modal blocking dialogs. Floating panels are
  preferred.

## Checklist for new features

When adding a Fusion-style action:

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
