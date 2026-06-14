  const DocumentState viewport_document =
      document_at_timeline_cursor(document.value());
  const DocumentState* view = &viewport_document;

  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportPolygonExtrudePrimitive> polygon_extrudes;
  std::vector<ViewportSolidFace> solid_faces;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportReferencePoint> reference_points;
  std::vector<ViewportHelixPrimitive> helices;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchPointPrimitive> sketch_points;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  std::vector<ViewportMeshPrimitive> meshes;
  std::vector<ViewportCutPreview> cut_previews;
  std::vector<ViewportBodySummary> bodies;
  std::vector<ViewportEdgePrimitive> edges;
  std::vector<ViewportVertexPrimitive> vertices;
  std::vector<EntityDofResult> dof_statuses;
  double current_x_offset = 0.0;
  double scene_width = 0.0;
  double max_height = 0.0;
  double max_depth = 0.0;

  // Walk the feature history once with boolean operators applied so we
  // know which features get consumed by Fuse/Cut and which bodies need
  // to be tessellated as mesh primitives. Features in the resulting
  // `consumed_feature_ids` set must be skipped by the legacy primitive
  // emission below to avoid double-rendering. Failures in OCCT booleans
  // produce empty meshes — see body_compiler.cpp — so legacy fallback
  // still renders something.
  CompiledBodies compiled_bodies = compile_bodies_for_viewport(*view);
  std::optional<std::string> selected_move_target_body_id;
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id == view->selected_feature_id.value() &&
          feature.kind == "move" &&
          feature.move_parameters.has_value()) {
        selected_move_target_body_id = feature.move_parameters->target_body_id;
        break;
      }
    }
  }
  for (const auto& body_mesh : compiled_bodies.meshes) {
    ViewportMeshPrimitive mesh{};
    mesh.id = body_mesh.body_id;
    mesh.positions = body_mesh.vertices;
    mesh.normals = body_mesh.normals;
    mesh.indices = body_mesh.indices;
    mesh.is_selected =
        view->selected_feature_id.has_value() &&
        (view->selected_feature_id.value() == body_mesh.body_id ||
         (selected_move_target_body_id.has_value() &&
          selected_move_target_body_id.value() == body_mesh.body_id));
    mesh.appearance_color = body_appearance_color(*view, body_mesh.body_id);
    meshes.push_back(std::move(mesh));
  }
  const std::set<std::string>& consumed = compiled_bodies.consumed_feature_ids;
