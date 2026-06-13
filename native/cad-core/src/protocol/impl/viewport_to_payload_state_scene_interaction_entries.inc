      {"scene_width", viewport.scene_width},
      {"scene_height", viewport.scene_height},
      {"scene_depth", viewport.scene_depth},
      {"scene_bounds",
       {
           {"center",
            {
                {"x", viewport.scene_bounds.center_x},
                {"y", viewport.scene_bounds.center_y},
                {"z", viewport.scene_bounds.center_z},
            }},
           {"size",
            {
                {"x", viewport.scene_bounds.width},
                {"y", viewport.scene_bounds.height},
                {"z", viewport.scene_bounds.depth},
            }},
            {"max_dimension", viewport.scene_bounds.max_dimension},
        }},
       {"selection_filter",
        [&viewport]() {
          const auto& sf = viewport.selection_filter;
          return json{
              {"select_curves", sf.select_curves},
              {"select_points", sf.select_points},
              {"select_construction", sf.select_construction},
              {"select_constraints", sf.select_constraints},
              {"snap_endpoint", sf.snap_endpoint},
              {"snap_midpoint", sf.snap_midpoint},
              {"snap_center", sf.snap_center},
              {"snap_intersection", sf.snap_intersection},
              {"snap_nearest", sf.snap_nearest},
              {"snap_quadrant", sf.snap_quadrant},
              {"snap_perpendicular", sf.snap_perpendicular},
              {"snap_parallel", sf.snap_parallel},
              {"snap_tangent", sf.snap_tangent},
              {"snap_grid", sf.snap_grid},
              {"snap_grid_line", sf.snap_grid_line},
              {"snap_polar", sf.snap_polar},
              {"polar_angle_degrees", sf.polar_angle_degrees},
              {"magnetic_pull", sf.magnetic_pull},
              {"tolerance_px", sf.tolerance_px},
          };
        }()},
       {"snap_candidates",
        [&viewport]() {
          json arr = json::array();
          for (const auto& c : viewport.snap_candidates) {
            arr.push_back({
                {"kind", c.kind},
                {"entity_id", c.entity_id},
                {"point_id", c.point_id},
                {"local_x", c.local_x},
                {"local_y", c.local_y},
                {"label", c.label},
            });
          }
          return arr;
        }()},
       {"dof_statuses",
        [&viewport]() {
          json statuses = json::array();
          for (const auto& e : viewport.dof_statuses) {
            std::string status_str = "under";
            if (e.status == polysmith::core::DofStatus::FullyConstrained)
              status_str = "full";
            else if (e.status == polysmith::core::DofStatus::OverConstrained)
              status_str = "over";
            statuses.push_back({
                {"entity_id", e.entity_id},
                {"entity_kind", e.entity_kind},
                {"total_dof", e.total_dof},
                {"consumed_dof", e.consumed_dof},
                {"status", status_str},
            });
          }
          return statuses;
        }()},
       {"solver_dofs", viewport.solver_dofs},
       {"solver_conflicting_count", viewport.solver_conflicting_count},
       {"solver_redundant_count", viewport.solver_redundant_count},
