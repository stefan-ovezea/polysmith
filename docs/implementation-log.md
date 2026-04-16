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
- split sketch mode into explicit drawing and selection behavior, including chained line creation and `Space` to return to selection mode
- generalize the selected-feature inspector beyond box-only editing
- keep viewport data derived from the native core
- continue expanding the design system while preserving architecture boundaries
