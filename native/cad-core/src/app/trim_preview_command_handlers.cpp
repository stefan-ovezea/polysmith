#include "app/trim_preview_command_handlers.h"

#include <algorithm>

#include "app/command_context.h"
#include "core/document/document.h"
#include "core/sketch/trim_engine.h"

namespace polysmith {

namespace {

using polysmith::protocol::json;

json make_segment_json(double param_start, double param_end, double start_x,
                      double start_y, double end_x, double end_y) {
  return json{{"param_start", param_start},
              {"param_end", param_end},
              {"start", {start_x, start_y}},
              {"end", {end_x, end_y}}};
}

}  // namespace

bool handle_trim_preview_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type != "trim_preview") {
    return false;
  }

  const std::string entity_id = read_string(command.payload, "entity_id");
  const double cursor_x = read_dimension(command.payload, "cursor_x");
  const double cursor_y = read_dimension(command.payload, "cursor_y");

  const auto doc_opt = manager.get_document();
  if (!doc_opt.has_value() ||
      !doc_opt->active_sketch_feature_id.has_value()) {
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", nullptr}});
    return true;
  }

  const auto feature_it = std::find_if(
      doc_opt->feature_history.begin(), doc_opt->feature_history.end(),
      [&](const auto& f) {
        return f.id == doc_opt->active_sketch_feature_id.value();
      });

  if (feature_it == doc_opt->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", nullptr}});
    return true;
  }

  const auto& params = feature_it->sketch_parameters.value();
  json result;
  result["entity_id"] = entity_id;
  result["revision"] = doc_opt->revision;

  const auto line_it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const auto& l) { return l.id == entity_id; });
  if (line_it != params.lines.end()) {
    auto isects = polysmith::core::find_all_intersections(*line_it, params);
    result["entity_kind"] = "line";
    auto& jsegs = result["segments"] = json::array();
    if (isects.empty()) {
      result["hovered_index"] = 0;
      jsegs.push_back(make_segment_json(0.0, 1.0, line_it->start_x,
                                        line_it->start_y, line_it->end_x,
                                        line_it->end_y));
    } else {
      auto segs = polysmith::core::split_line_at_intersections(*line_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *line_it, cursor_x, cursor_y);
      result["hovered_index"] = idx;
      for (const auto& s : segs) {
        jsegs.push_back(make_segment_json(s.param_start, s.param_end, s.start_x,
                                          s.start_y, s.end_x, s.end_y));
      }
    }
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", result}});
    return true;
  }

  const auto circle_it = std::find_if(
      params.circles.begin(), params.circles.end(),
      [&](const auto& c) { return c.id == entity_id; });
  if (circle_it != params.circles.end()) {
    auto isects = polysmith::core::find_all_intersections(*circle_it, params);
    auto r = json::object();
    r["entity_id"] = entity_id;
    r["entity_kind"] = "circle";
    r["revision"] = doc_opt->revision;
    if (isects.empty()) {
      r["hovered_index"] = 0;
      r["full_circle"] = true;
    } else {
      auto segs = polysmith::core::split_circle_at_intersections(*circle_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *circle_it, cursor_x, cursor_y);
      r["hovered_index"] = idx;
      r["full_circle"] = false;
      auto& jsegs = r["segments"] = json::array();
      for (const auto& s : segs) {
        jsegs.push_back(make_segment_json(s.param_start, s.param_end, s.start_x,
                                          s.start_y, s.end_x, s.end_y));
      }
    }
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", r}});
    return true;
  }

  const auto arc_it = std::find_if(
      params.arcs.begin(), params.arcs.end(),
      [&](const auto& a) { return a.id == entity_id; });
  if (arc_it != params.arcs.end()) {
    auto isects = polysmith::core::find_all_intersections(*arc_it, params);
    auto r = json::object();
    r["entity_id"] = entity_id;
    r["entity_kind"] = "arc";
    r["revision"] = doc_opt->revision;
    if (isects.empty()) {
      r["hovered_index"] = 0;
      r["full_arc"] = true;
    } else {
      auto segs = polysmith::core::split_arc_at_intersections(*arc_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *arc_it, cursor_x, cursor_y);
      r["hovered_index"] = idx;
      r["full_arc"] = false;
      auto& jsegs = r["segments"] = json::array();
      for (const auto& s : segs) {
        jsegs.push_back(make_segment_json(s.param_start, s.param_end, s.start_x,
                                          s.start_y, s.end_x, s.end_y));
      }
    }
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", r}});
    return true;
  }

  const auto ellipse_it = std::find_if(
      params.ellipses.begin(), params.ellipses.end(),
      [&](const auto& e) { return e.id == entity_id; });
  if (ellipse_it != params.ellipses.end()) {
    auto isects = polysmith::core::find_all_intersections(*ellipse_it, params);
    auto r = json::object();
    r["entity_id"] = entity_id;
    r["entity_kind"] = "ellipse";
    r["revision"] = doc_opt->revision;
    if (isects.empty()) {
      r["hovered_index"] = 0;
      r["full_ellipse"] = true;
    } else {
      auto segs = polysmith::core::split_ellipse_at_intersections(*ellipse_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *ellipse_it, cursor_x, cursor_y);
      r["hovered_index"] = idx;
      r["full_ellipse"] = false;
      auto& jsegs = r["segments"] = json::array();
      for (const auto& s : segs) {
        jsegs.push_back(make_segment_json(s.param_start, s.param_end, s.start_x,
                                          s.start_y, s.end_x, s.end_y));
      }
    }
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", r}});
    return true;
  }

  const auto spline_it = std::find_if(
      params.splines.begin(), params.splines.end(),
      [&](const auto& s) { return s.id == entity_id; });
  if (spline_it != params.splines.end()) {
    auto isects = polysmith::core::find_all_intersections(*spline_it, params);
    auto r = json::object();
    r["entity_id"] = entity_id;
    r["entity_kind"] = "spline";
    r["revision"] = doc_opt->revision;
    if (isects.empty()) {
      r["hovered_index"] = 0;
      r["full_spline"] = true;
    } else {
      auto segs = polysmith::core::split_spline_at_intersections(*spline_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *spline_it, cursor_x, cursor_y);
      r["hovered_index"] = idx;
      r["full_spline"] = false;
      auto& jsegs = r["segments"] = json::array();
      for (const auto& s : segs) {
        jsegs.push_back(make_segment_json(s.param_start, s.param_end, s.start_x,
                                          s.start_y, s.end_x, s.end_y));
      }
    }
    polysmith::protocol::write_message(
        json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", r}});
    return true;
  }

  polysmith::protocol::write_message(
      json{{"id", command.id}, {"type", "trim_preview_result"}, {"payload", nullptr}});
  return true;
}

}  // namespace polysmith
