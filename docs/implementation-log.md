# PolySmith Implementation Log

This document tracks concrete implementation milestones as they land in the codebase.

## 2026-04-16

### Architecture and Protocol

- established a strict JSON IPC path between UI and native core
- moved native CAD startup, document handling, and protocol logic out of `main.cpp`
- moved Tauri process management and protocol forwarding out of `main.rs`
- documented roadmap, protocol direction, and onboarding updates in `docs/`

### Core Document Foundation

- added document creation and document state queries
- added session state queries
- made the native core the source of truth for document and feature history state

### First Modeling and Viewport Slices

- added `add_box_feature`
- added `update_box_feature`
- added viewport snapshots derived from core-owned feature data
- rendered viewport state in the UI as a lightweight SVG wireframe preview

### Selection and Editing Loop

- added core-owned feature selection
- synchronized selection between feature browser, timeline, and viewport
- added rename and delete behavior for features
- added undo and redo in the native core through snapshot history
- added a second primitive, `cylinder`, through the core, protocol, viewport, and UI forms

### UI Rebuild

- adopted the `Midnight Carbon` design direction from `docs/DESIGN.md`
- added Tailwind-based styling and font setup
- rebuilt the desktop shell into a more CAD-like workspace layout
- introduced a top mode header, floating command bar, side panels, and bottom feature timeline

### Current Focus

- replaced the SVG debug viewport with a focused `three`-based 3D renderer spike in the UI
- added a renderer-facing scene adapter so the viewport still renders strictly from core-owned snapshot data
- kept viewport selection on the existing IPC feature-selection flow while adding orbit, pan, and zoom controls
- extended the viewport snapshot with core-owned primitive centers and scene bounds for renderer-facing camera framing
- added fit-view and hover feedback to make the 3D viewport feel more usable without moving behavior into React
- added core-owned origin reference planes and axes so the viewport can render CAD-style construction geometry
- added reference-plane selection and a first `start_sketch_on_plane` command stub to establish the sketch entry flow
- turned sketching into a real first-pass workflow by creating core-owned sketch features on planes
- added a first sketch entity, `line`, created from two viewport clicks and rendered back from the core snapshot
- extended sketching with core-owned tool state, viewport snapping, rectangle and circle creation, and selectable sketch entities
- added minimal inferred horizontal and vertical line hints in the core as a lightweight first step toward sketch constraints
- split sketch mode into explicit drawing and selection behavior, including chained line creation and `Escape` to return to selection mode
- added editable sketch lines and circles in the inspector, visible sketch points in the viewport, and first explicit horizontal/vertical line constraints
- added derived closed sketch profile detection in the core plus selectable profile overlays in the viewport
- added a first `extrude_profile` command that turns rectangular profiles into box-like extrudes and XY circular profiles into cylinders
- widened closed-profile detection from rectangle-only cases to arbitrary clean closed line loops and rendered those extrudes as polygonal prisms in the viewport
- added core-derived sketch dimensions to the viewport so active sketches now show line lengths and circle radii directly in the 3D sketch view
- promoted sketch dimensions into core-owned sketch data and added a first `update_sketch_dimension` IPC command for driving line length and circle radius
- kept the viewport on core-owned dimension snapshots while adding a minimal inspector path to edit those driving values from the UI
- added explicit core-owned sketch dimension selection plus a `select_sketch_dimension` IPC path so viewport overlays can be selected directly and highlighted
- added a first in-viewport dimension editor with autofocus and keyboard-friendly editing while reducing duplicate inspector dimension controls during viewport editing
- added a first solver-lite propagation pass for connected sketch lines so moving or redimensioning a shared endpoint also updates coincident neighbors and re-applies horizontal/vertical line constraints through that chain
- stabilized shared sketch corners further by snapping added, edited, constrained, and dimension-driven line endpoints onto existing coincident endpoints in the core before propagation runs
- promoted sketch line endpoint connectivity from coordinate-only inference to explicit internal point ids so connected redimensioning and constraint propagation can follow stable shared topology
- added a first cross-line `equal_length` sketch relation in the core and re-applied it through the existing line edit, constraint, and dimension-driving flows
- moved sketch line constraints into the top ribbon with a dedicated sketch constraints section, keeping the UI as intent-only while the core owns constraint and relation state
- shifted sketch constraints to an armed click flow in the UI so horizontal, vertical, clear, and equal-length constraints are chosen from the ribbon and then applied by clicking one or two sketch lines in the viewport
- added core-owned viewport constraint markers for sketch lines and equal-length relations, including direct badge-click clearing through the existing constraint commands
- tightened equal-length propagation so relation-driven updates also re-propagate shared start-point changes caused by snapping or connected geometry
- added a first solver-lite `perpendicular` sketch relation with the same armed two-line workflow as equal length and core-owned perpendicular viewport markers
- kept perpendicular behavior intentionally narrow by preserving the driven line start point and length while rejecting setups that conflict with direct horizontal or vertical axis constraints
- added a first solver-lite `parallel` sketch relation with the same armed two-line workflow and core-owned parallel viewport markers
- kept parallel behavior intentionally narrow by preserving the driven line start point and length while rejecting setups that conflict with direct horizontal or vertical axis constraints
- added an explicit endpoint-only `coincident` sketch tool so the UI can pick real core-owned endpoint ids and ask the native sketch core to merge those points through IPC
- kept coincidence solver-lite by making it a direct point-id merge in the native core, which now preserves connected redimensioning behavior without introducing a separate removable point-constraint model yet
- added a focused `export_document` IPC spike that writes a real STEP file from core-owned feature history
- kept export in the native core by rebuilding solid-producing OCCT shapes there and returning a `document_exported` event to the UI
- generalize the selected-feature inspector beyond box-only editing
- keep viewport data derived from the native core
- continue expanding the design system while preserving architecture boundaries
- added core-owned planar solid-face snapshots for primitive picking in the viewport
- added `select_face` and `start_sketch_on_face` IPC commands so the UI can select a face and start a sketch from core-owned face identity
- kept face-to-sketch placement in the native core by deriving a sketch plane frame from the selected face before creating the sketch feature
- fixed the face-sketch relay so the UI now passes the core-emitted face plane frame back into `start_sketch_on_face`, which keeps sketch clicks and dimension labels on the chosen face
- preserved the sketch plane frame through closed-profile detection and extrusion so face-based profile overlays and extruded bodies stay on the selected face instead of snapping to a perpendicular origin plane
- promoted polygon profile detection to use core-owned shared sketch point ids as the primary topology source, which makes closed-loop detection more robust when coincident endpoints stay connected through redimensioning and point merges
- promoted sketch points into explicit core-owned records and added `select_sketch_point` so the viewport can select stable point ids through IPC instead of fabricating point state in React
- promoted sketch profiles into stored core-owned region data on the sketch feature so selection, viewport rendering, and extrusion read cached regions instead of re-deriving them ad hoc
- kept redimensioning and coincident merges on the native side while rebuilding stored sketch points and profiles after sketch edits, which makes closed loops more resilient under parametric edits
- added a minimal core-owned fixed-point relation with `set_sketch_point_fixed`, fixed-point viewport badges, and point-aware inspector actions so point selection now drives a real parametric edit flow
- kept fixed-point behavior solver-lite in the native core by preserving fixed flags through point rebuilds and coincident merges while driving line-length edits from the unfixed endpoint whenever possible
- added point-driven sketch editing through `update_sketch_point`, tightened fixed-point preservation across line edits and solver-lite relations, and refreshed profile-linked extrude parameters when source sketch profiles change
- added editable extrude depth through a core-owned `update_extrude_depth` IPC command and an inspector form so a selected extrude feature can be redimensioned without rebuilding from the source sketch
- adopted a Fusion-style select-action-preview-confirm pattern across the UI: hoverable solid faces with face hover highlighting, double-click on any face to start a sketch, world-aligned camera framing on sketch entry, a subtle frameless inline dimension input that auto-dismisses on Enter, and an `E` hotkey that triggers a floating extrude preview panel driving live `update_extrude_depth` previews with confirm/cancel; documented the pattern as binding in `docs/architecture/fusion-style-behavior.md` so future contributions follow the same flow
- stabilized sketch UX further: the camera now reframes only on sketch plane transitions instead of every viewport snapshot, the extrude preview panel debounces depth previews so the inspector no longer needs its own depth form, profiles can be selected and extruded outside an active sketch via relaxed `select_sketch_profile` and `extrude_profile`, finished sketches can be reopened through a new `reenter_sketch` IPC command, and the document browser was rebuilt as a Fusion-like collapsible hierarchy with Origin / Sketches / Bodies categories, compact icon-and-name rows, and per-row plus per-category eye toggles that drive UI-side viewport visibility filtering
- fixed the live extrude depth bug at its root: the Tauri IPC bridge is fire-and-forget, so reading the document store immediately after `await sendCoreCommand(...)` returned stale state and the floating Extrude panel was driving `update_extrude_depth` on the wrong feature id; introduced an `awaitDocumentChange` helper on the store and used it to capture the freshly-created extrude feature id before opening the action panel
- shipped binary STL export through a new core `export_document_as_stl` (BRepMesh_IncrementalMesh + StlAPI_Writer), an `export_document_stl` IPC command, and an Export STL button in the header; the existing `document_exported` reply distinguishes formats via its `format` field
- added Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z, and Cmd/Ctrl+Y hotkeys for undo/redo at the App level, gated by typing-target detection and core-reported `can_undo`/`can_redo` so users can iterate without reaching for the header buttons
- refreshed `docs/roadmap/v1-roadmap.md` to reflect actual project state and a tiered next-feature plan (cut extrude, save/load, fillet/chamfer with edge selection, etc.)
