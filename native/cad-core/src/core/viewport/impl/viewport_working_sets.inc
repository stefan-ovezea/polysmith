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
  std::vector<ViewportSketchEllipsePrimitive> sketch_ellipses;
  std::vector<ViewportSketchSplinePrimitive> sketch_splines;
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchVertexPrimitive> sketch_vertices;
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
    // Per-vertex colours from the file's per-FACE styles. A part commonly
    // carries several (gold contacts, black body, white legend); with one
    // material colour per body those faces all come out the same shade, which
    // is why a connector's plated contacts read as a flat orange ribbon and a
    // shield's engraved legend vanished into the shield.
    const FeatureEntry* owner = nullptr;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body_mesh.body_id) {
        owner = &feature;
        break;
      }
    }
    if (owner != nullptr && owner->step_import_parameters.has_value() &&
        !owner->step_import_parameters->face_colors.empty() &&
        body_mesh.vertex_faces.size() == body_mesh.vertices.size() / 3) {
      const std::vector<std::uint32_t>& face_colors =
          owner->step_import_parameters->face_colors;
      const auto to_linear = [](std::uint32_t channel) {
        // The renderer works in linear light; three.js converts material
        // colours but not vertex colours, so the sRGB table has to be
        // converted here or every imported part renders washed out.
        const double value = static_cast<double>(channel) / 255.0;
        return value <= 0.04045 ? value / 12.92
                                : std::pow((value + 0.055) / 1.055, 2.4);
      };
      mesh.colors.reserve(body_mesh.vertices.size());
      bool any_color = false;
      for (const int face_index : body_mesh.vertex_faces) {
        std::uint32_t packed = 0u;
        if (face_index >= 0 &&
            static_cast<std::size_t>(face_index) < face_colors.size()) {
          packed = face_colors[static_cast<std::size_t>(face_index)];
        }
        if ((packed & 0xFF000000u) != 0u) {
          any_color = true;
        }
        mesh.colors.push_back(to_linear((packed >> 16) & 0xFFu));
        mesh.colors.push_back(to_linear((packed >> 8) & 0xFFu));
        mesh.colors.push_back(to_linear(packed & 0xFFu));
      }
      if (!any_color) {
        mesh.colors.clear();
      }
      if (!mesh.colors.empty() && !body_mesh.vertex_faces.empty()) {
        // Coplanar ink inside ONE body. An engraved legend and the shield
        // plate around it are separate faces of the same solid sitting in the
        // same plane, so they tie in the depth buffer and which one wins is
        // decided per pixel — the legend shows as chopped fragments at one
        // angle and vanishes at another. There is no per-face depth bias in
        // the renderer, so push every face whose colour is NOT the body's
        // dominant colour 10 um along its own normal. That is far below
        // anything visible on a 72 mm board (a quarter of a 0.05 mm trace),
        // it does not touch the B-rep, its edges or its picking, and it makes
        // the ink win deterministically in every view.
        std::map<std::uint32_t, int> face_colour_counts;
        for (const int face_index : body_mesh.vertex_faces) {
          if (face_index < 0 ||
              static_cast<std::size_t>(face_index) >= face_colors.size()) {
            continue;
          }
          const std::uint32_t packed =
              face_colors[static_cast<std::size_t>(face_index)];
          if ((packed & 0xFF000000u) == 0u) {
            continue;
          }
          face_colour_counts[packed] += 1;
        }
        if (face_colour_counts.size() > 1) {
          std::uint32_t dominant = 0u;
          int dominant_count = 0;
          for (const auto& [colour, count] : face_colour_counts) {
            if (count > dominant_count) {
              dominant = colour;
              dominant_count = count;
            }
          }
          // Measured, not guessed: on the ESP32-P4C5 module the shield plate
          // tops out at z=3.25 and its engraved legend at z=3.24 — the file
          // sinks the ink exactly 10 um. A 10 um lift therefore landed the
          // legend back ON the plate and the tie came straight back. Lift by
          // the recess plus a margin: 50 um still leaves the ink under every
          // other surface on the board (a 0.05 mm trace) and is invisible at
          // any zoom, but it wins the depth test outright.
          constexpr double kInkLift = 0.05;
          for (std::size_t vertex = 0;
               vertex < body_mesh.vertex_faces.size(); ++vertex) {
            const int face_index = body_mesh.vertex_faces[vertex];
            if (face_index < 0 ||
                static_cast<std::size_t>(face_index) >= face_colors.size()) {
              continue;
            }
            const std::uint32_t packed =
                face_colors[static_cast<std::size_t>(face_index)];
            if (packed == dominant || (packed & 0xFF000000u) == 0u) {
              continue;
            }
            if (vertex * 3 + 2 >= mesh.positions.size()) {
              continue;
            }
            mesh.positions[vertex * 3 + 0] += mesh.normals[vertex * 3 + 0] * kInkLift;
            mesh.positions[vertex * 3 + 1] += mesh.normals[vertex * 3 + 1] * kInkLift;
            mesh.positions[vertex * 3 + 2] += mesh.normals[vertex * 3 + 2] * kInkLift;
          }
        }
      }
    }
    meshes.push_back(std::move(mesh));
  }
  const std::set<std::string>& consumed = compiled_bodies.consumed_feature_ids;
