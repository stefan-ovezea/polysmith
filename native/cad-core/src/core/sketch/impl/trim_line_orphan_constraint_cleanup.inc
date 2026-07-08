// Safety net: delete coincident constraints referencing orphaned point IDs.
{
  std::unordered_set<std::string> live_pt;
  for (const auto &l : params.lines) {
    live_pt.insert(l.start_point_id);
    live_pt.insert(l.end_point_id);
  }
  for (const auto &c : params.circles) {
    live_pt.insert(circle_center_point_id(c));
    // Quadrants still use legacy format (Phase 4.1 will update)
    live_pt.insert("point-circle-" + c.id + "-quadrant-0");
    live_pt.insert("point-circle-" + c.id + "-quadrant-1");
    live_pt.insert("point-circle-" + c.id + "-quadrant-2");
    live_pt.insert("point-circle-" + c.id + "-quadrant-3");
  }
  for (const auto &a : params.arcs) {
    live_pt.insert(a.start_point_id);
    live_pt.insert(a.end_point_id);
  }
  params.constraints.erase(std::remove_if(params.constraints.begin(),
                                          params.constraints.end(),
                                          [&](const SketchConstraint &c) {
                                            if (c.kind != "coincident")
                                              return false;
                                            for (const auto &tid : c.target_ids)
                                              if (!live_pt.count(tid))
                                                return true;
                                            return false;
                                          }),
                           params.constraints.end());
}
