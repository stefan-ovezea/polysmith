#include "core/cam/laser/laser_test_pattern.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "core/cam/cam_generator.h"
#include "core/cam/cam_types.h"
#include "core/cam/laser/laser_fill.h"
#include "core/cam/laser/laser_generate.h"
#include "core/document/document.h"
#include "core/text_engine.h"

namespace polysmith::core::laser {

namespace {

using polysmith::core::CamGenerateResult;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;

using cam2d::XY;
using cam2d::xy_length;

// Grid value at index i of a (steps-1)-segmented sweep: the extremes
// are included (LightBurn test cards include min and max).
double sweep(double from, double to, int steps, int index) {
  if (steps <= 1) {
    return from;
  }
  return from + (to - from) * static_cast<double>(index) / (steps - 1);
}

}  // namespace

CamGenerateResult generate_laser_test_pattern_toolpath(
    const polysmith::core::CamGenerateContext& context) {
  CamGenerateResult result;
  const auto& op = context.operation;

  if (!op.parameters.test_pattern.has_value()) {
    result.ok = false;
    result.error_message =
        "The test-pattern operation is missing its parameters.";
    return result;
  }
  const auto& tp = op.parameters.test_pattern.value();

  if (tp.pattern != "engrave_grid" && tp.pattern != "cut_grid" &&
      tp.pattern != "kerf_gauge") {
    result.ok = false;
    result.error_message =
        "Unknown test pattern '" + tp.pattern +
        "' (expected engrave_grid, cut_grid, or kerf_gauge).";
    return result;
  }
  if (tp.power_steps < 2 || tp.speed_steps < 2) {
    result.ok = false;
    result.error_message =
        "A test pattern needs at least 2 power and 2 speed steps.";
    return result;
  }
  if (tp.cell_size_mm <= 0.0 || tp.cell_spacing_mm < 0.0) {
    result.ok = false;
    result.error_message =
        "The test-pattern cell size must be positive.";
    return result;
  }
  if (tp.power_min_percent < 0.0 || tp.power_max_percent > 100.0 ||
      tp.power_min_percent > tp.power_max_percent) {
    result.ok = false;
    result.error_message =
        "The power range must stay within 0..100%.";
    return result;
  }
  if (tp.speed_min_mm_per_s <= 0.0 ||
      tp.speed_max_mm_per_s < tp.speed_min_mm_per_s) {
    result.ok = false;
    result.error_message = "The speed range must be positive.";
    return result;
  }

  const double cellStep = tp.cell_size_mm + tp.cell_spacing_mm;
  const double gridWidth =
      tp.power_steps * cellStep - tp.cell_spacing_mm;
  const double gridHeight =
      tp.speed_steps * cellStep - tp.cell_spacing_mm;

  // The card must fit the machine bed (when the machine settings
  // carry a work area).
  if (context.document.cam.machine_settings.has_value()) {
    const auto& machine = context.document.cam.machine_settings.value();
    if (tp.start_x_mm + gridWidth > machine.work_area_x_mm ||
        tp.start_y_mm + gridHeight > machine.work_area_y_mm) {
      result.warnings.push_back(
          "The test pattern (" + std::to_string(gridWidth) + " × " +
          std::to_string(gridHeight) + " mm at " +
          std::to_string(tp.start_x_mm) + ", " + std::to_string(tp.start_y_mm) +
          ") exceeds the machine work area (" +
          std::to_string(machine.work_area_x_mm) + " × " +
          std::to_string(machine.work_area_y_mm) + " mm).");
    }
  }

  Toolpath toolpath;
  toolpath.op_id = op.op_id;

  const auto append_move = [&](const XY& point, bool laserOn, double power,
                               double speed_mm_per_s) {
    ToolpathMove move;
    move.kind = ToolpathMoveKind::FeedLinear;
    move.x = point.x;
    move.y = point.y;
    move.z = 0.0;
    move.feedrate_mm_per_min = speed_mm_per_s * 60.0;
    move.power_percent = power;
    move.laser_on = laserOn;
    move.dwell_seconds = 0.0;
    toolpath.moves.push_back(move);
  };
  // Travel between cells at the fastest cell speed, laser off.
  const double travelSpeed = tp.speed_max_mm_per_s;

  // Engraves a label under a cell ("P85 S10" style) with the cell's
  // own power/speed — the core text engine lays out the glyph
  // contours, the UI never touches geometry.
  const auto emit_label = [&](const XY& center, const std::string& text,
                              double power, double speed) {
    polysmith::core::text::TextStyle style;
    style.height_mm = 2.2;
    style.h_align = "center";
    style.v_align = "middle";
    polysmith::core::text::TextLayout layout;
    std::string error;
    if (!polysmith::core::text::TextEngine::instance().layout(
            text, center.x, center.y, style, &layout, &error)) {
      result.warnings.push_back("A test-card label could not be laid "
                                "out: " + error);
      return;
    }
    for (const auto& contour : layout.contours) {
      if (contour.points.size() < 2) {
        continue;
      }
      append_move(XY{contour.points[0].x, contour.points[0].y},
                  /*laserOn=*/false, 0.0, travelSpeed);
      for (size_t i = 1; i < contour.points.size(); ++i) {
        append_move(XY{contour.points[i].x, contour.points[i].y},
                    /*laserOn=*/true, power, speed);
      }
      append_move(XY{contour.points[0].x, contour.points[0].y},
                  /*laserOn=*/true, power, speed);
    }
  };

  // ── Kerf gauge: ONE calibration square cut with the current
  // kerf/power/speed.  Measure the plug: kerf = (cell − plug) / 2.
  if (tp.pattern == "kerf_gauge") {
    const XY origin{tp.start_x_mm, tp.start_y_mm};
    std::vector<cam2d::BaseSegment> base;
    const std::vector<XY> corners = {
        {origin.x, origin.y},
        {origin.x + tp.cell_size_mm, origin.y},
        {origin.x + tp.cell_size_mm, origin.y + tp.cell_size_mm},
        {origin.x, origin.y + tp.cell_size_mm},
    };
    for (size_t i = 0; i < 4; ++i) {
      cam2d::BaseSegment segment;
      segment.start = corners[i];
      segment.end = corners[(i + 1) % 4];
      base.push_back(segment);
    }
    if (cam2d::base_segments_signed_area(base) < 0) {
      cam2d::reverse_segments(base);
    }
    std::vector<cam2d::OffsetSegment> offset;
    if (!cam2d::offset_closed_loop(base, tp.kerf_width_mm / 2.0, offset)) {
      result.ok = false;
      result.error_message =
          "The kerf gauge square could not be offset (check the kerf).";
      return result;
    }
    const auto samples = cam2d::sample_offset_loop(offset, /*tolerance=*/0.05);
    if (samples.size() < 3) {
      result.ok = false;
      result.error_message = "The kerf gauge square could not be sampled.";
      return result;
    }
    // Pierce at the vertex nearest the square center, then cut.
    const XY center{origin.x + tp.cell_size_mm / 2.0,
                    origin.y + tp.cell_size_mm / 2.0};
    size_t pierceIndex = 0;
    double best = std::numeric_limits<double>::max();
    for (size_t i = 0; i < samples.size(); ++i) {
      const double d = xy_length(samples[i].x - center.x,
                                 samples[i].y - center.y);
      if (d < best) {
        best = d;
        pierceIndex = i;
      }
    }
    append_move(samples[pierceIndex], /*laserOn=*/false, 0.0, travelSpeed);
    for (size_t k = 0; k < samples.size(); ++k) {
      const auto& point = samples[(pierceIndex + k) % samples.size()];
      append_move(point, /*laserOn=*/true, tp.power_percent,
                  tp.speed_mm_per_s);
    }
    if (tp.cell_labels) {
      emit_label(XY{center.x, origin.y + tp.cell_size_mm + 2.5},
                 "KERF", tp.power_percent, tp.speed_mm_per_s);
    }
    toolpath.op_id = op.op_id;
    result.toolpath = std::move(toolpath);
    return result;
  }

  // One square loop (world coordinates) for the fill hatching.
  const auto make_square_loop = [&](const XY& origin) {
    PlannedLoop square;
    square.isWorldXY = true;
    square.worldZ = 0.0;
    square.centroid = XY{origin.x + tp.cell_size_mm / 2.0,
                         origin.y + tp.cell_size_mm / 2.0};
    square.area = tp.cell_size_mm * tp.cell_size_mm;
    square.length = 4.0 * tp.cell_size_mm;
    square.samples = {
        {origin.x, origin.y},
        {origin.x + tp.cell_size_mm, origin.y},
        {origin.x + tp.cell_size_mm, origin.y + tp.cell_size_mm},
        {origin.x, origin.y + tp.cell_size_mm},
    };
    for (size_t i = 0; i < 4; ++i) {
      cam2d::OffsetSegment segment;
      segment.is_arc = false;
      segment.start = square.samples[i];
      segment.end = square.samples[(i + 1) % 4];
      square.segments.push_back(segment);
    }
    return square;
  };

  for (int row = 0; row < tp.speed_steps; ++row) {
    const double speed = sweep(tp.speed_min_mm_per_s, tp.speed_max_mm_per_s,
                               tp.speed_steps, row);
    for (int col = 0; col < tp.power_steps; ++col) {
      const double power = sweep(tp.power_min_percent, tp.power_max_percent,
                                 tp.power_steps, col);
      const XY origin{tp.start_x_mm + col * cellStep,
                      tp.start_y_mm + row * cellStep};

      if (tp.pattern == "engrave_grid") {
        // Filled square at this cell's power/speed.  The hatch spans
        // come from the shared fill module; the cell emitter keeps
        // the beam on across lines (bidirectional).
        LaserCutParameters fillParams;
        fillParams.mode = "engrave";
        fillParams.engrave_style = "fill";
        fillParams.line_spacing_mm = tp.line_spacing_mm;
        fillParams.fill_angle_deg = 0.0;
        fillParams.fill_bidirectional = true;
        const auto hatchLines = hatch_region({make_square_loop(origin)},
                                             fillParams);
        if (hatchLines.empty()) {
          result.warnings.push_back(
              "A test-pattern cell produced no fill lines (check the "
              "line spacing).");
          continue;
        }
        append_move(hatchLines.front().front().start, /*laserOn=*/false, 0.0,
                    travelSpeed);
        for (size_t li = 0; li < hatchLines.size(); ++li) {
          auto spans = hatchLines[li];
          if (li % 2 == 1) {
            std::reverse(spans.begin(), spans.end());
            for (auto& span : spans) {
              std::swap(span.start, span.end);
            }
          }
          for (size_t si = 0; si < spans.size(); ++si) {
            append_move(spans[si].end, /*laserOn=*/true, power, speed);
            if (si + 1 < spans.size()) {
              append_move(spans[si + 1].start, /*laserOn=*/false, 0.0,
                          travelSpeed);
            }
          }
          if (li + 1 < hatchLines.size()) {
            const auto& next = hatchLines[li + 1];
            const XY nextStart = (li + 1) % 2 == 1 ? next.back().end
                                                   : next.front().start;
            append_move(nextStart, /*laserOn=*/true, power, speed);
          }
        }
      } else {
        // Through-cut square contour at this cell's power/speed.
        append_move(origin, /*laserOn=*/false, 0.0, travelSpeed);
        append_move(XY{origin.x + tp.cell_size_mm, origin.y},
                    /*laserOn=*/true, power, speed);
        append_move(XY{origin.x + tp.cell_size_mm,
                       origin.y + tp.cell_size_mm},
                    /*laserOn=*/true, power, speed);
        append_move(XY{origin.x, origin.y + tp.cell_size_mm},
                    /*laserOn=*/true, power, speed);
        append_move(origin, /*laserOn=*/true, power, speed);
      }

      // Label the cell ("P85 S10") so the card reads like a
      // LightBurn material test.
      if (tp.cell_labels) {
        emit_label(XY{origin.x + tp.cell_size_mm / 2.0,
                      origin.y + tp.cell_size_mm + 2.5},
                   "P" + std::to_string(static_cast<int>(std::round(power))) +
                       " S" +
                       std::to_string(static_cast<int>(std::round(speed))),
                   power, speed);
      }
    }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No test-pattern moves were produced.";
    return result;
  }
  result.toolpath = std::move(toolpath);
  return result;
}

void register_laser_test_pattern_generator() {
  polysmith::core::register_cam_generator(
      {"laser_test_pattern", generate_laser_test_pattern_toolpath,
       generate_laser_test_pattern_toolpath});
}

}  // namespace polysmith::core::laser
