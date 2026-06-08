  // ── CAM toolpaths from operations ────────────────────────────
  std::vector<ViewportToolpathPrimitive> toolpaths;
  for (const auto& op : view->cam_operations) {
    CamToolpath camTp;
    if (op.type == CamOperationType::FaceMilling) {
      camTp = generate_face_milling_toolpath(op, *view);
    }
    // Convert CamToolpath to ViewportToolpathPrimitive.
    if (camTp.totalPoints > 0) {
      ViewportToolpathPrimitive vtp;
      vtp.id = op.id;
      vtp.label = op.name;
      for (const auto& move : camTp.moves) {
        for (const auto& pt : move.points) {
          vtp.points.push_back({pt.x, pt.y, pt.z, move.isRapid});
        }
      }
      toolpaths.push_back(std::move(vtp));
    }
  }

  // Toolpaths are only emitted when CAM operations exist.

  // ── CAM stock — deferred until non-interactive rendering is available.
  // TODO: render stock as a translucent box that doesn't participate in
  // the CAD picking chain (ViewportBoxPrimitive triggers feature lookup).
