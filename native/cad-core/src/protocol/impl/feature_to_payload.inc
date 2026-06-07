json to_payload(const polysmith::core::FeatureEntry& feature) {
  return {
      {"feature_id", feature.id},
      {"kind", feature.kind},
      {"name", feature.name},
      {"status", feature.status},
      {"suppressed", feature.suppressed},
      // Surface broken-dependency state to the UI so the timeline can
      // render the warning. The frame on the feature itself stays at
      // its last-known value; this flag is the only signal that the
      // upstream face it referenced is gone.
      {"dependency_broken", feature.dependency_broken},
      {"dependency_warning", feature.dependency_warning},
      {"parameters_summary", feature.parameters_summary},
      {"box_parameters",
       feature.box_parameters.has_value()
           ? json{
                 {"width", feature.box_parameters->width},
                 {"height", feature.box_parameters->height},
                 {"depth", feature.box_parameters->depth},
             }
           : json(nullptr)},
      {"cylinder_parameters",
       feature.cylinder_parameters.has_value()
           ? json{
                 {"radius", feature.cylinder_parameters->radius},
                 {"height", feature.cylinder_parameters->height},
             }
           : json(nullptr)},
      {"extrude_parameters",
       feature.extrude_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.extrude_parameters->sketch_feature_id},
                 {"profile_id", feature.extrude_parameters->profile_id},
                 {"profile_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id : feature.extrude_parameters->profile_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"open_entity_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id :
                         feature.extrude_parameters->open_entity_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"plane_id", feature.extrude_parameters->plane_id},
                 {"plane_frame",
                  feature.extrude_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->origin_x},
                                 {"y", feature.extrude_parameters->plane_frame->origin_y},
                                 {"z", feature.extrude_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->x_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->x_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->y_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->y_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->normal_x},
                                 {"y", feature.extrude_parameters->plane_frame->normal_y},
                                 {"z", feature.extrude_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
                 {"profile_kind", feature.extrude_parameters->profile_kind},
                 {"start_x", feature.extrude_parameters->start_x},
                 {"start_y", feature.extrude_parameters->start_y},
                 {"width", feature.extrude_parameters->width},
                 {"height", feature.extrude_parameters->height},
                 {"radius", feature.extrude_parameters->radius},
                 {"profile_points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point :
                         feature.extrude_parameters->profile_points) {
                      points.push_back({
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return points;
                  }()},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.extrude_parameters->inner_loops) {
                      json loop_payload = json::array();
                      for (const auto& point : loop) {
                        loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      loops.push_back(loop_payload);
                    }
                    return loops;
                  }()},
                 {"additional_profile_points",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile :
                         feature.extrude_parameters->additional_profile_points) {
                      json points = json::array();
                      for (const auto& point : profile) {
                        points.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      profiles.push_back(points);
                    }
                    return profiles;
                  }()},
                 {"additional_inner_loops",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile_loops :
                         feature.extrude_parameters->additional_inner_loops) {
                      json loops = json::array();
                      for (const auto& loop : profile_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                        }
                        loops.push_back(loop_payload);
                      }
                      profiles.push_back(loops);
                    }
                    return profiles;
                 }()},
                 {"depth", feature.extrude_parameters->depth},
                 {"extent_mode", feature.extrude_parameters->extent_mode},
                 {"side1",
                  extrude_side_to_payload(feature.extrude_parameters->side1)},
                 {"side2",
                  feature.extrude_parameters->side2.has_value()
                      ? extrude_side_to_payload(
                            feature.extrude_parameters->side2.value())
                      : json(nullptr)},
                 {"thin",
                  {
                      {"enabled", feature.extrude_parameters->thin.enabled},
                      {"thickness", feature.extrude_parameters->thin.thickness},
                      {"placement", feature.extrude_parameters->thin.placement},
                  }},
                 {"mode", feature.extrude_parameters->mode},
                 {"operation", feature.extrude_parameters->operation},
                 {"intersect_result",
                  feature.extrude_parameters->intersect_result},
                 {"target_body_id",
                  feature.extrude_parameters->target_body_id.has_value()
                      ? json(feature.extrude_parameters->target_body_id.value())
                      : json(nullptr)},
             }
           : json(nullptr)},
      {"loft_parameters",
       feature.loft_parameters.has_value()
           ? json{
                 {"ruled", feature.loft_parameters->ruled},
                 {"sections",
                  [&feature]() {
                    json sections = json::array();
                    for (const auto& section :
                         feature.loft_parameters->sections) {
                      sections.push_back({
                          {"sketch_feature_id", section.sketch_feature_id},
                          {"profile_id", section.profile_id},
                          {"plane_id", section.plane_id},
                          {"plane_frame",
                           section.plane_frame.has_value()
                               ? plane_frame_to_payload(section.plane_frame.value())
                               : json(nullptr)},
                          {"profile_points",
                           profile_points_to_payload(section.profile_points)},
                      });
                    }
                    return sections;
                  }()},
             }
           : json(nullptr)},
      {"revolve_parameters",
       feature.revolve_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.revolve_parameters->sketch_feature_id},
                 {"profile_id", feature.revolve_parameters->profile_id},
                 {"plane_id", feature.revolve_parameters->plane_id},
                 {"plane_frame",
                  feature.revolve_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.revolve_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.revolve_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.revolve_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.revolve_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"axis_sketch_feature_id",
                  feature.revolve_parameters->axis_sketch_feature_id},
                 {"axis_entity_id",
                  feature.revolve_parameters->axis_entity_id},
                 {"axis_start_x", feature.revolve_parameters->axis_start_x},
                 {"axis_start_y", feature.revolve_parameters->axis_start_y},
                 {"axis_start_z", feature.revolve_parameters->axis_start_z},
                 {"axis_end_x", feature.revolve_parameters->axis_end_x},
                 {"axis_end_y", feature.revolve_parameters->axis_end_y},
                 {"axis_end_z", feature.revolve_parameters->axis_end_z},
                 {"angle_degrees",
                  feature.revolve_parameters->angle_degrees},
             }
           : json(nullptr)},
      {"sweep_parameters",
       feature.sweep_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.sweep_parameters->sketch_feature_id},
                 {"profile_id", feature.sweep_parameters->profile_id},
                 {"plane_id", feature.sweep_parameters->plane_id},
                 {"plane_frame",
                  feature.sweep_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.sweep_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.sweep_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.sweep_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.sweep_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"path_sketch_feature_id",
                  feature.sweep_parameters->path_sketch_feature_id},
                 {"path_entity_id",
                  feature.sweep_parameters->path_entity_id},
                 {"path_start_x", feature.sweep_parameters->path_start_x},
                 {"path_start_y", feature.sweep_parameters->path_start_y},
                 {"path_start_z", feature.sweep_parameters->path_start_z},
                 {"path_end_x", feature.sweep_parameters->path_end_x},
                 {"path_end_y", feature.sweep_parameters->path_end_y},
                 {"path_end_z", feature.sweep_parameters->path_end_z},
                 {"path_segments",
                  [&feature]() {
                    json segments = json::array();
                    for (const auto& segment :
                         feature.sweep_parameters->path_segments) {
                      segments.push_back(
                          json{{"entity_id", segment.entity_id},
                               {"kind", segment.kind},
                               {"start_x", segment.start_x},
                               {"start_y", segment.start_y},
                               {"start_z", segment.start_z},
                               {"end_x", segment.end_x},
                               {"end_y", segment.end_y},
                               {"end_z", segment.end_z},
                               {"center_x", segment.center_x},
                               {"center_y", segment.center_y},
                               {"center_z", segment.center_z},
                               {"mid_x", segment.mid_x},
                               {"mid_y", segment.mid_y},
                               {"mid_z", segment.mid_z},
                               {"radius", segment.radius},
                               {"ccw", segment.ccw}});
                    }
                    return segments;
                  }()},
             }
           : json(nullptr)},
      {"sketch_parameters",
       feature.sketch_parameters.has_value()
           ? json{
                 {"plane_id", feature.sketch_parameters->plane_id},
                 {"plane_frame",
                  feature.sketch_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->origin_x},
                                 {"y", feature.sketch_parameters->plane_frame->origin_y},
                                 {"z", feature.sketch_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->x_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->x_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->y_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->y_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->normal_x},
                                 {"y", feature.sketch_parameters->plane_frame->normal_y},
                                 {"z", feature.sketch_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
                 {"lines",
                  [&feature]() {
                    json lines = json::array();
                    for (const auto& line : feature.sketch_parameters->lines) {
                      lines.push_back({
                          {"line_id", line.id},
                          {"start_point_id", line.start_point_id},
                          {"end_point_id", line.end_point_id},
                          {"start_x", line.start_x},
                          {"start_y", line.start_y},
                          {"end_x", line.end_x},
                          {"end_y", line.end_y},
                          {"constraint",
                           line.constraint.has_value()
                               ? json(line.constraint.value())
                               : json(nullptr)},
                          {"is_construction", line.is_construction},
                      });
                    }
                    return lines;
                  }()},
                 {"circles",
                 [&feature]() {
                    json circles = json::array();
                    for (const auto& circle : feature.sketch_parameters->circles) {
                      circles.push_back({
                          {"circle_id", circle.id},
                          {"center_x", circle.center_x},
                          {"center_y", circle.center_y},
                          {"radius", circle.radius},
                          {"is_construction", circle.is_construction},
                      });
                    }
                    return circles;
                  }()},
                 {"arcs",
                 [&feature]() {
                    json arcs = json::array();
                    for (const auto& arc : feature.sketch_parameters->arcs) {
                      arcs.push_back({
                          {"arc_id", arc.id},
                          {"start_point_id", arc.start_point_id},
                          {"end_point_id", arc.end_point_id},
                          {"center_x", arc.center_x},
                          {"center_y", arc.center_y},
                          {"radius", arc.radius},
                          {"start_x", arc.start_x},
                          {"start_y", arc.start_y},
                          {"end_x", arc.end_x},
                          {"end_y", arc.end_y},
                          {"ccw", arc.ccw},
                          {"is_construction", arc.is_construction},
                      });
                    }
                    return arcs;
                  }()},
                 {"fillets",
                  [&feature]() {
                    json fillets = json::array();
                    for (const auto& fillet : feature.sketch_parameters->fillets) {
                      fillets.push_back({
                          {"fillet_id", fillet.id},
                          {"corner_point_id", fillet.corner_point_id},
                          {"corner_x", fillet.corner_x},
                          {"corner_y", fillet.corner_y},
                          {"line_a_id", fillet.line_a_id},
                          {"line_b_id", fillet.line_b_id},
                          {"trim_a_point_id", fillet.trim_a_point_id},
                          {"trim_b_point_id", fillet.trim_b_point_id},
                          {"arc_id", fillet.arc_id},
                          {"radius", fillet.radius},
                      });
                    }
                    return fillets;
                  }()},
                  {"polygons",
                   [&feature]() {
                     json polygons = json::array();
                     for (const auto& polygon : feature.sketch_parameters->polygons) {
                       polygons.push_back({
                           {"polygon_id", polygon.id},
                           {"center_x", polygon.center_x},
                           {"center_y", polygon.center_y},
                           {"radius", polygon.radius},
                           {"sides", polygon.sides},
                           {"mode", polygon.mode},
                           {"start_x", polygon.start_x},
                           {"start_y", polygon.start_y},
                           {"end_x", polygon.end_x},
                           {"end_y", polygon.end_y},
                           {"is_construction", polygon.is_construction},
                       });
                     }
                     return polygons;
                   }()},
                  {"points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point : feature.sketch_parameters->points) {
                      points.push_back({
                          {"point_id", point.id},
                          {"kind", point.kind},
                          {"x", point.x},
                          {"y", point.y},
                          {"is_fixed", point.is_fixed},
                      });
                    }
                    return points;
                  }()},
                 {"dimensions",
                  [&feature]() {
                    json dimensions = json::array();
                    for (const auto& dimension :
                         feature.sketch_parameters->dimensions) {
                      dimensions.push_back({
                          {"dimension_id", dimension.id},
                          {"kind", dimension.kind},
                          {"entity_id", dimension.entity_id},
                          {"secondary_entity_id",
                           dimension.secondary_entity_id},
                          {"value", dimension.value},
                          {"expression", dimension.expression},
                          {"driven", dimension.driven},
                          {"is_auto", dimension.is_auto},
                          {"display_as", dimension.display_as},
                          {"label_x",
                           dimension.label_x.has_value()
                               ? json(*dimension.label_x)
                               : json(nullptr)},
                          {"label_y",
                           dimension.label_y.has_value()
                               ? json(*dimension.label_y)
                               : json(nullptr)},
                      });
                    }
                    return dimensions;
                  }()},
                 {"line_relations",
                  [&feature]() {
                    json relations = json::array();
                    for (const auto& relation :
                         feature.sketch_parameters->line_relations) {
                      relations.push_back({
                          {"relation_id", relation.id},
                          {"kind", relation.kind},
                          {"first_line_id", relation.first_line_id},
                          {"second_line_id", relation.second_line_id},
                        });
                    }
                    return relations;
                   }()},
                  {"constraints",
                   [&feature]() {
                     json constraint_list = json::array();
                     for (const auto& constraint :
                          feature.sketch_parameters->constraints) {
                       json target_ids = json::array();
                       for (const auto& tid : constraint.target_ids) {
                         target_ids.push_back(tid);
                       }
                       constraint_list.push_back({
                           {"constraint_id", constraint.constraint_id},
                           {"kind", constraint.kind},
                           {"target_ids", target_ids},
                           {"value", constraint.value},
                           {"driven", constraint.driven},
                         });
                     }
                     return constraint_list;
                   }()},
                  {"midpoint_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->midpoint_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                      });
                    }
                    return anchors;
                  }()},
                 {"point_line_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->point_line_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                          {"t", anchor.t},
                      });
                    }
                    return anchors;
                  }()},
                 {"projected_points",
                  [&feature]() {
                    json projected = json::array();
                    for (const auto& point :
                         feature.sketch_parameters->projected_points) {
                      projected.push_back({
                          {"point_id", point.id},
                          {"source_id", point.source_id},
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return projected;
                  }()},
                 {"projected_sources",
                  [&feature]() {
                    json sources = json::array();
                    for (const auto& source :
                         feature.sketch_parameters->projected_sources) {
                      sources.push_back(source);
                    }
                    return sources;
                  }()},
                 {"projections",
                  [&feature]() {
                    json projections_payload = json::array();
                    for (const auto& projection :
                         feature.sketch_parameters->projections) {
                      json line_ids = json::array();
                      for (const auto& id : projection.generated_line_ids) {
                        line_ids.push_back(id);
                      }
                      json circle_ids = json::array();
                      for (const auto& id : projection.generated_circle_ids) {
                        circle_ids.push_back(id);
                      }
                      json arc_ids = json::array();
                      for (const auto& id : projection.generated_arc_ids) {
                        arc_ids.push_back(id);
                      }
                      projections_payload.push_back({
                          {"projection_id", projection.id},
                          {"source_id", projection.source_id},
                          {"source_kind", projection.source_kind},
                          {"generated_line_ids", line_ids},
                          {"generated_circle_ids", circle_ids},
                          {"generated_arc_ids", arc_ids},
                          {"generated_point_id",
                           projection.generated_point_id},
                          {"dependency_broken", projection.dependency_broken},
                          {"dependency_warning",
                           projection.dependency_warning},
                      });
                    }
                    return projections_payload;
                  }()},
                 {"profiles",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile : feature.sketch_parameters->profiles) {
                      json point_ids = json::array();
                      for (const auto& point_id : profile.point_ids) {
                        point_ids.push_back(point_id);
                      }

                      json line_ids = json::array();
                      for (const auto& line_id : profile.line_ids) {
                        line_ids.push_back(line_id);
                      }

                      json points = json::array();
                      for (const auto& point : profile.points) {
                        points.push_back({
                            {"x", point.x},
                            {"y", point.y},
                        });
                      }
                      json inner_loops = json::array();
                      for (const auto& loop : profile.inner_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({
                              {"x", point.x},
                              {"y", point.y},
                          });
                        }
                        inner_loops.push_back(loop_payload);
                      }

                      profiles.push_back({
                          {"profile_id", profile.id},
                          {"kind", profile.kind},
                          {"point_ids", point_ids},
                          {"line_ids", line_ids},
                          {"points", points},
                          {"inner_loops", inner_loops},
                          {"source_circle_id",
                           profile.source_circle_id.has_value()
                               ? json(profile.source_circle_id.value())
                               : json(nullptr)},
                          {"center_x", profile.center_x},
                          {"center_y", profile.center_y},
                          {"radius", profile.radius},
                      });
                    }
                    return profiles;
                  }()},
                 {"active_tool", feature.sketch_parameters->active_tool},
                 // Pending mirror tool state. Emits `null` when no
                 // mirror is in progress; the UI uses presence of
                 // this object as the signal to show the floating
                 // panel.
                 {"pending_mirror",
                  feature.sketch_parameters->pending_mirror.has_value()
                      ? [&feature]() {
                          const auto& pm =
                              *feature.sketch_parameters->pending_mirror;
                          json object_ids = json::array();
                          for (const auto& id : pm.object_ids) {
                            object_ids.push_back(id);
                          }
                          json generated_lines = json::array();
                          for (const auto& line : pm.generated_lines) {
                            generated_lines.push_back({
                                {"line_id", line.id},
                                {"start_point_id", line.start_point_id},
                                {"end_point_id", line.end_point_id},
                                {"start_x", line.start_x},
                                {"start_y", line.start_y},
                                {"end_x", line.end_x},
                                {"end_y", line.end_y},
                                {"is_construction", line.is_construction},
                            });
                          }
                          json generated_circles = json::array();
                          for (const auto& circle : pm.generated_circles) {
                            generated_circles.push_back({
                                {"circle_id", circle.id},
                                {"center_x", circle.center_x},
                                {"center_y", circle.center_y},
                                {"radius", circle.radius},
                                {"is_construction", circle.is_construction},
                            });
                          }
                          return json{
                              {"axis_line_id",
                               pm.axis_line_id.has_value()
                                   ? json(*pm.axis_line_id)
                                   : json(nullptr)},
                              {"object_ids", object_ids},
                              {"generated_lines", generated_lines},
                              {"generated_circles", generated_circles},
                          };
                        }()
                      : json(nullptr)},
             }
           : json(nullptr)},
      {"fillet_parameters",
       feature.fillet_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.fillet_parameters->target_body_id},
                 {"edge_ids", feature.fillet_parameters->edge_ids},
                 {"radius", feature.fillet_parameters->radius},
                 {"is_pending", feature.fillet_parameters->is_pending},
             }
           : json(nullptr)},
      {"chamfer_parameters",
       feature.chamfer_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.chamfer_parameters->target_body_id},
                 {"edge_ids", feature.chamfer_parameters->edge_ids},
                 {"distance", feature.chamfer_parameters->distance},
                 {"is_pending", feature.chamfer_parameters->is_pending},
             }
           : json(nullptr)},
      {"shell_parameters",
       feature.shell_parameters.has_value()
           ? json{
                 {"target_body_id", feature.shell_parameters->target_body_id},
                 {"removed_face_ids",
                  feature.shell_parameters->removed_face_ids},
                 {"thickness", feature.shell_parameters->thickness},
                 {"is_pending", feature.shell_parameters->is_pending},
             }
           : json(nullptr)},
      {"construction_plane_parameters",
       feature.construction_plane_parameters.has_value()
           ? json{
                 {"plane_type",
                  feature.construction_plane_parameters->plane_type},
                 {"source_plane_id",
                 feature.construction_plane_parameters->source_plane_id},
                 {"source_plane_ids",
                  feature.construction_plane_parameters->source_plane_ids},
                 {"source_axis_id",
                  feature.construction_plane_parameters->source_axis_id},
                 {"offset", feature.construction_plane_parameters->offset},
                 {"angle_degrees",
                  feature.construction_plane_parameters->angle_degrees},
                 {"plane_frame",
                  {
                      {"origin",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.origin_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.origin_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.origin_z},
                       }},
                      {"x_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_z},
                       }},
                      {"y_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_z},
                       }},
                      {"normal",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.normal_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.normal_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.normal_z},
                       }},
                  }},
             }
           : json(nullptr)},
      {"construction_axis_parameters",
       feature.construction_axis_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_axis_parameters->source_id},
                 {"source_kind",
                  feature.construction_axis_parameters->source_kind},
                 {"start",
                  {{"x", feature.construction_axis_parameters->start_x},
                   {"y", feature.construction_axis_parameters->start_y},
                   {"z", feature.construction_axis_parameters->start_z}}},
                 {"end",
                  {{"x", feature.construction_axis_parameters->end_x},
                   {"y", feature.construction_axis_parameters->end_y},
                   {"z", feature.construction_axis_parameters->end_z}}},
             }
           : json(nullptr)},
      {"construction_point_parameters",
       feature.construction_point_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_point_parameters->source_id},
                 {"source_kind",
                  feature.construction_point_parameters->source_kind},
                 {"position",
                  {{"x", feature.construction_point_parameters->x},
                   {"y", feature.construction_point_parameters->y},
                   {"z", feature.construction_point_parameters->z}}},
             }
           : json(nullptr)},
      {"hole_parameters",
       feature.hole_parameters.has_value()
           ? json{
                 {"target_body_id", feature.hole_parameters->target_body_id},
                 {"source_face_id", feature.hole_parameters->source_face_id},
                 {"plane_frame",
                  plane_frame_to_payload(feature.hole_parameters->plane_frame)},
                 {"center_x", feature.hole_parameters->center_x},
                 {"center_y", feature.hole_parameters->center_y},
                 {"hole_type", feature.hole_parameters->hole_type},
                 {"extent_type", feature.hole_parameters->extent_type},
                 {"diameter", feature.hole_parameters->diameter},
                 {"depth", feature.hole_parameters->depth},
                 {"counterbore_diameter",
                  feature.hole_parameters->counterbore_diameter},
                 {"counterbore_depth",
                  feature.hole_parameters->counterbore_depth},
                 {"countersink_diameter",
                  feature.hole_parameters->countersink_diameter},
                 {"countersink_angle_degrees",
                  feature.hole_parameters->countersink_angle_degrees},
                 {"standard", feature.hole_parameters->standard},
                 {"standard_size", feature.hole_parameters->standard_size},
                 {"hole_fit", feature.hole_parameters->hole_fit},
                 {"thread_enabled", feature.hole_parameters->thread_enabled},
                 {"thread_spec", feature.hole_parameters->thread_spec},
                 {"thread_pitch", feature.hole_parameters->thread_pitch},
                 {"major_diameter", feature.hole_parameters->major_diameter},
                 {"minor_diameter", feature.hole_parameters->minor_diameter},
                 {"thread_depth", feature.hole_parameters->thread_depth},
                 {"thread_representation",
                  feature.hole_parameters->thread_representation},
                 {"is_pending", feature.hole_parameters->is_pending},
             }
           : json(nullptr)},
      {"helix_parameters",
       feature.helix_parameters.has_value()
           ? json{
                 {"axis_source_id",
                  feature.helix_parameters->axis_source_id},
                 {"axis_start",
                  {{"x", feature.helix_parameters->axis_start_x},
                   {"y", feature.helix_parameters->axis_start_y},
                   {"z", feature.helix_parameters->axis_start_z}}},
                 {"axis_end",
                  {{"x", feature.helix_parameters->axis_end_x},
                   {"y", feature.helix_parameters->axis_end_y},
                   {"z", feature.helix_parameters->axis_end_z}}},
                 {"radius", feature.helix_parameters->radius},
                 {"pitch", feature.helix_parameters->pitch},
                 {"height", feature.helix_parameters->height},
                 {"turns", feature.helix_parameters->turns},
                 {"handedness", feature.helix_parameters->handedness},
                 {"start_angle_degrees",
                  feature.helix_parameters->start_angle_degrees},
                 {"points", feature.helix_parameters->points},
             }
           : json(nullptr)},
      {"thread_parameters",
       feature.thread_parameters.has_value()
           ? json{
                 {"target_body_id", feature.thread_parameters->target_body_id},
                 {"axis_source_id", feature.thread_parameters->axis_source_id},
                 {"mode", feature.thread_parameters->mode},
                 {"standard", feature.thread_parameters->standard},
                 {"size", feature.thread_parameters->size},
                 {"major_diameter",
                  feature.thread_parameters->major_diameter},
                 {"minor_diameter",
                  feature.thread_parameters->minor_diameter},
                 {"pitch", feature.thread_parameters->pitch},
                 {"length", feature.thread_parameters->length},
                 {"thread_angle_degrees",
                  feature.thread_parameters->thread_angle_degrees},
                 {"start_offset", feature.thread_parameters->start_offset},
                 {"handedness", feature.thread_parameters->handedness},
                 {"representation", feature.thread_parameters->representation},
                 {"is_pending", feature.thread_parameters->is_pending},
             }
           : json(nullptr)},
      {"fastener_parameters",
       feature.fastener_parameters.has_value()
           ? json{
                 {"standard", feature.fastener_parameters->standard},
                 {"size", feature.fastener_parameters->size},
                 {"diameter", feature.fastener_parameters->diameter},
                 {"minor_diameter",
                  feature.fastener_parameters->minor_diameter},
                 {"pitch", feature.fastener_parameters->pitch},
                 {"length", feature.fastener_parameters->length},
                 {"thread_length", feature.fastener_parameters->thread_length},
                 {"head_type", feature.fastener_parameters->head_type},
                 {"drive_type", feature.fastener_parameters->drive_type},
                 {"thread_representation",
                  feature.fastener_parameters->thread_representation},
             }
           : json(nullptr)},
      {"move_parameters",
       feature.move_parameters.has_value()
           ? json{
                 {"target_body_id", feature.move_parameters->target_body_id},
                 {"translation_x", feature.move_parameters->translation_x},
                 {"translation_y", feature.move_parameters->translation_y},
                 {"translation_z", feature.move_parameters->translation_z},
                 {"rotation_x_degrees",
                  feature.move_parameters->rotation_x_degrees},
                 {"rotation_y_degrees",
                  feature.move_parameters->rotation_y_degrees},
                 {"rotation_z_degrees",
                  feature.move_parameters->rotation_z_degrees},
                 {"is_pending", feature.move_parameters->is_pending},
             }
           : json(nullptr)},
      {"body_copy_parameters",
       feature.body_copy_parameters.has_value()
           ? json{
                 {"source_body_id",
                  feature.body_copy_parameters->source_body_id},
                 {"copy_mode", feature.body_copy_parameters->copy_mode},
                 {"source_body_name",
                  feature.body_copy_parameters->source_body_name},
                 {"serialized_shape",
                  feature.body_copy_parameters->serialized_shape},
                 {"local_x_axis_x",
                  feature.body_copy_parameters->local_x_axis_x},
                 {"local_x_axis_y",
                  feature.body_copy_parameters->local_x_axis_y},
                 {"local_x_axis_z",
                  feature.body_copy_parameters->local_x_axis_z},
                 {"local_y_axis_x",
                  feature.body_copy_parameters->local_y_axis_x},
                 {"local_y_axis_y",
                  feature.body_copy_parameters->local_y_axis_y},
                 {"local_y_axis_z",
                  feature.body_copy_parameters->local_y_axis_z},
                 {"local_z_axis_x",
                  feature.body_copy_parameters->local_z_axis_x},
                 {"local_z_axis_y",
                  feature.body_copy_parameters->local_z_axis_y},
                 {"local_z_axis_z",
                  feature.body_copy_parameters->local_z_axis_z},
             }
           : json(nullptr)},
  };
}
