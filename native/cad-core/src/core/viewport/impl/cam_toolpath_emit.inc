// ── CAM toolpaths from operations ────────────────────────────
  // Toolpath generation is being rebuilt on the new cam_types.h schema.
  // For now, emit empty toolpaths so the viewport doesn't attempt to
  // generate geometry from the old CamOperationEntry type.
  std::vector<ViewportToolpathPrimitive> toolpaths;

  // ── CAM stock — deferred until non-interactive rendering is available.
  // TODO: render stock as a translucent box that doesn't participate in
  // the CAD picking chain (ViewportBoxPrimitive triggers feature lookup).