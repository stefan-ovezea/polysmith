// Clean up zero‑length lines — a symptom of a constraint‑resolver
// or trim bug. Collect their ids first, then erase the lines and
// every piece of associated data (constraints, dimensions, relations,
// anchors, fillets) so they don't keep re‑logging on every refresh.
{
  std::vector<std::string> zero_ids;
  for (const auto &line : p.lines) {
    if (points_match(line.start_x, line.start_y, line.end_x, line.end_y)) {
      zero_ids.push_back(line.id);
    }
  }
  if (!zero_ids.empty()) {
    for (const auto &zid : zero_ids) {
      fprintf(stderr, "ERROR deleted zero-length line %s\n", zid.c_str());

      // Erase constraints that target this line.
      // NOTE: endpoint IDs are NOT used here — the line's endpoints
      // may be shared with other surviving lines, and deleting a
      // coincident constraint on a shared endpoint would break the
      // other line's geometry.
      p.constraints.erase(
          std::remove_if(p.constraints.begin(), p.constraints.end(),
                         [&](const SketchConstraint &c) {
                           for (const auto &tid : c.target_ids) {
                             if (tid == zid) return true;
                           }
                           return false;
                         }),
          p.constraints.end());

      // Erase dimensions on this line.
      p.dimensions.erase(std::remove_if(p.dimensions.begin(),
                                        p.dimensions.end(),
                                        [&](const SketchDimension &d) {
                                          return d.entity_id == zid ||
                                                 d.secondary_entity_id == zid;
                                        }),
                         p.dimensions.end());

      // Erase relations involving this line.
      p.line_relations.erase(std::remove_if(p.line_relations.begin(),
                                            p.line_relations.end(),
                                            [&](const SketchLineRelation &rel) {
                                              return rel.first_line_id == zid ||
                                                     rel.second_line_id == zid;
                                            }),
                             p.line_relations.end());

      // Erase fillets that involve this line.
      p.fillets.erase(std::remove_if(p.fillets.begin(), p.fillets.end(),
                                     [&](const SketchFillet &f) {
                                       return f.line_a_id == zid ||
                                              f.line_b_id == zid;
                                     }),
                      p.fillets.end());

      // Erase anchors hosted on this line.
      p.midpoint_anchors.erase(
          std::remove_if(
              p.midpoint_anchors.begin(), p.midpoint_anchors.end(),
              [&](const SketchMidpointAnchor &a) { return a.line_id == zid; }),
          p.midpoint_anchors.end());
      p.point_line_anchors.erase(
          std::remove_if(
              p.point_line_anchors.begin(), p.point_line_anchors.end(),
              [&](const SketchPointLineAnchor &a) { return a.line_id == zid; }),
          p.point_line_anchors.end());

      // Erase the line itself.
      p.lines.erase(std::remove_if(
                        p.lines.begin(), p.lines.end(),
                        [&](const SketchLine &line) { return line.id == zid; }),
                    p.lines.end());
    }
  }
}
