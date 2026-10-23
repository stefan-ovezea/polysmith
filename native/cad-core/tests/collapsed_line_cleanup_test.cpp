// Regression tests for the collapsed-line cleanup (S6).
//
// A line the solver collapses cannot be drawn or extruded, so the refresh
// removes it together with every record that referenced it. S6's requirement is
// that the removal is REPORTED: one warning naming how many dimensions,
// constraints and relations went with the line (before, it vanished silently and
// the sketch looked like the user's dimension had never existed).
//
// The cleanup is a safety net rather than a path a plain drag can take: the
// degenerate-drop guard refuses "Sketch lines must have non-zero length (moving
// connected points)". That is why this test drives `remove_collapsed_line_records`
// directly instead of trying to collapse a line through the command layer.
#include <iostream>
#include <string>

#include "core/sketch/sketch_feature.h"

namespace {

using polysmith::core::CollapsedLineCleanupReport;
using polysmith::core::SketchConstraint;
using polysmith::core::SketchDimension;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchLineRelation;
using polysmith::core::format_collapsed_line_warning;
using polysmith::core::remove_collapsed_line_records;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchLine make_line(const std::string& id) {
  SketchLine line;
  line.id = id;
  line.start_vertex_id = "vertex-" + id + "-a";
  line.end_vertex_id = "vertex-" + id + "-b";
  return line;
}

bool test_removes_the_line_and_its_records() {
  SketchFeatureParameters parameters;
  parameters.lines.push_back(make_line("line-1"));
  parameters.lines.push_back(make_line("line-2"));

  SketchDimension dimension;
  dimension.id = "dim-line-line-1";
  dimension.kind = "line_length";
  dimension.entity_id = "line-1";
  parameters.dimensions.push_back(dimension);

  SketchDimension other_dimension;
  other_dimension.id = "dim-line-line-2";
  other_dimension.kind = "line_length";
  other_dimension.entity_id = "line-2";
  parameters.dimensions.push_back(other_dimension);

  SketchConstraint constraint;
  constraint.constraint_id = "constraint-line-1";
  constraint.kind = "horizontal";
  constraint.target_ids = {"line-1"};
  parameters.constraints.push_back(constraint);

  SketchConstraint other_constraint;
  other_constraint.constraint_id = "constraint-line-2";
  other_constraint.kind = "horizontal";
  other_constraint.target_ids = {"line-2"};
  parameters.constraints.push_back(other_constraint);

  SketchLineRelation relation;
  relation.id = "rel-1";
  relation.kind = "parallel";
  relation.first_line_id = "line-1";
  relation.second_line_id = "line-2";
  parameters.line_relations.push_back(relation);

  const CollapsedLineCleanupReport report =
      remove_collapsed_line_records(parameters, "line-1");

  return expect(report.lines == 1, "cleanup: one line removed") &&
         expect(report.dimensions == 1, "cleanup: one dimension removed") &&
         expect(report.constraints == 1, "cleanup: one constraint removed") &&
         expect(report.relations == 1, "cleanup: one relation removed") &&
         expect(report.total_removed() == 3, "cleanup: total excludes the line") &&
         expect(parameters.lines.size() == 1 && parameters.lines[0].id == "line-2",
                "cleanup: the surviving line is untouched") &&
         expect(parameters.dimensions.size() == 1 &&
                    parameters.dimensions[0].entity_id == "line-2",
                "cleanup: the other line keeps its dimension") &&
         expect(parameters.constraints.size() == 1 &&
                    parameters.constraints[0].target_ids[0] == "line-2",
                "cleanup: the other line keeps its constraint") &&
         expect(parameters.line_relations.empty(),
                "cleanup: the relation referencing the line is gone");
}

bool test_warning_names_the_counts() {
  CollapsedLineCleanupReport report;
  report.lines = 1;
  report.dimensions = 2;
  report.constraints = 3;
  report.relations = 4;
  const std::string message = format_collapsed_line_warning("line-9", report);
  return expect(message ==
                    "collapsed line line-9 removed 2 dimension(s), 3 "
                    "constraint(s) and 4 relation(s)",
                ("cleanup: warning text - got: " + message).c_str());
}

bool test_no_reference_means_no_warning() {
  SketchFeatureParameters parameters;
  parameters.lines.push_back(make_line("line-1"));
  const CollapsedLineCleanupReport report =
      remove_collapsed_line_records(parameters, "line-1");
  return expect(report.total_removed() == 0,
                "cleanup: a line with no dependent records reports zero") &&
         expect(parameters.lines.empty(), "cleanup: the line itself is removed");
}

bool test_unknown_line_is_a_noop() {
  SketchFeatureParameters parameters;
  parameters.lines.push_back(make_line("line-1"));
  const CollapsedLineCleanupReport report =
      remove_collapsed_line_records(parameters, "line-missing");
  return expect(report.total_removed() == 0 && report.lines == 0,
                "cleanup: an unknown id changes nothing") &&
         expect(parameters.lines.size() == 1, "cleanup: the line survives");
}

}  // namespace

int main() {
  if (!test_removes_the_line_and_its_records()) return 1;
  if (!test_warning_names_the_counts()) return 1;
  if (!test_no_reference_means_no_warning()) return 1;
  if (!test_unknown_line_is_a_noop()) return 1;

  std::cout << "collapsed_line_cleanup_test passed\n";
  return 0;
}
