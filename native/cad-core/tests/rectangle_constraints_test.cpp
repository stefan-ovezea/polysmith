// What a rectangle IS, expressed as constraints.
//
// A rectangle is a four-sided shape whose opposite sides are parallel and whose
// corners are right angles. Constraining every side horizontal/vertical is one
// way to say that, and it is what this tool used to do - but it says something
// stronger (the shape is axis-locked) and it never states the thing the user
// actually means: the angles are 90 degrees. Fusion 360 constrains a rectangle
// with one horizontal, two parallel pairs and one perpendicular (90 degrees);
// the remaining freedom is exactly x, y, width and height.
//
// This suite pins that contract, and it pins the two invariants that make it
// safe:
//
//  - the solver accepts it without conflict or redundancy (dofs == 4), i.e. the
//    four constraints are independent and describe a rectangle exactly;
//  - enforcing them on a rectangle that already satisfies them changes nothing,
//    not even in the last bit. A relation pass that rewrites coordinates it does
//    not need to change corrupts every sketch that merely contains a rectangle.
#include <array>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/sketch_feature.h"
#include "core/viewport/viewport.h"
#include "core/sketch/sketch_feature.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::SketchDimension;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchLineRelation;

bool expect(bool condition, const std::string& message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

FeatureEntry* sketch_feature(DocumentState& document) {
  for (auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return &feature;
  }
  return nullptr;
}

const SketchLine* find_line(const SketchFeatureParameters& parameters,
                            const std::string& id) {
  for (const auto& line : parameters.lines) {
    if (line.id == id) return &line;
  }
  return nullptr;
}

bool is_horizontal_side(const SketchLine& line) {
  return std::abs(line.start_y - line.end_y) <= 1e-9;
}

bool is_vertical_side(const SketchLine& line) {
  return std::abs(line.start_x - line.end_x) <= 1e-9;
}

std::vector<const SketchLineRelation*> relations_of_kind(
    const SketchFeatureParameters& parameters, const std::string& kind) {
  std::vector<const SketchLineRelation*> found;
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind == kind) found.push_back(&relation);
  }
  return found;
}

bool relation_joins(const SketchLineRelation& relation,
                    const std::string& a,
                    const std::string& b) {
  return (relation.first_line_id == a && relation.second_line_id == b) ||
         (relation.first_line_id == b && relation.second_line_id == a);
}

// A rectangle as the tool draws it: two clicks, axis-aligned, 40 wide, 20 tall.
DocumentState build_rectangle(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  return manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
}

// ---------------------------------------------------------------------------
// 1. The constraint set states what a rectangle is.
// ---------------------------------------------------------------------------
bool test_rectangle_states_parallel_angle_and_one_horizontal() {
  DocumentManager manager;
  DocumentState document = build_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "relations: the sketch exists")) return false;
  const auto& parameters = *feature->sketch_parameters;

  if (!expect(parameters.lines.size() == 4,
              "relations: a rectangle is four lines")) {
    return false;
  }

  // Exactly one axis statement: the bottom side is horizontal. The other three
  // sides are held by the relations, not by three more axis constraints - a V
  // on every side would over-specify the shape and clutter the viewport.
  int horizontal_count = 0;
  int vertical_count = 0;
  std::string horizontal_line_id;
  for (const auto& line : parameters.lines) {
    if (!line.constraint.has_value()) continue;
    if (line.constraint.value() == "horizontal") {
      ++horizontal_count;
      horizontal_line_id = line.id;
    } else if (line.constraint.value() == "vertical") {
      ++vertical_count;
    }
  }
  if (!expect(horizontal_count == 1,
              "relations: exactly one side carries a horizontal constraint - got " +
                  std::to_string(horizontal_count))) {
    return false;
  }
  if (!expect(vertical_count == 0,
              "relations: no side carries a vertical constraint - got " +
                  std::to_string(vertical_count))) {
    return false;
  }

  // Two parallel pairs (opposite sides) and one right angle. Nothing else: an
  // equal-length relation would be a different (and wrong) statement - opposite
  // sides of a 40x20 rectangle are patently not equal.
  const auto parallels = relations_of_kind(parameters, "parallel");
  const auto perpendiculars = relations_of_kind(parameters, "perpendicular");
  if (!expect(parallels.size() == 2,
              "relations: two parallel pairs - got " +
                  std::to_string(parallels.size()))) {
    return false;
  }
  if (!expect(perpendiculars.size() == 1,
              "relations: one 90 degree constraint - got " +
                  std::to_string(perpendiculars.size()))) {
    return false;
  }
  if (!expect(relations_of_kind(parameters, "equal_length").empty(),
              "relations: no equal-length constraint on a rectangle")) {
    return false;
  }

  // Each parallel pair joins opposite sides: both horizontal sides, or both
  // vertical sides. A pair joining two ADJACENT sides would be a contradiction.
  for (const auto* relation : parallels) {
    const SketchLine* a = find_line(parameters, relation->first_line_id);
    const SketchLine* b = find_line(parameters, relation->second_line_id);
    if (!expect(a != nullptr && b != nullptr,
                "relations: the parallel pair names two live lines")) {
      return false;
    }
    const bool opposite =
        (is_horizontal_side(*a) && is_horizontal_side(*b)) ||
        (is_vertical_side(*a) && is_vertical_side(*b));
    if (!expect(opposite,
                "relations: a parallel pair joins opposite sides (" +
                    a->id + ", " + b->id + ")")) {
      return false;
    }
  }

  // The right angle sits on a corner: one horizontal side, one vertical side.
  {
    const auto& relation = *perpendiculars.front();
    const SketchLine* a = find_line(parameters, relation.first_line_id);
    const SketchLine* b = find_line(parameters, relation.second_line_id);
    if (!expect(a != nullptr && b != nullptr,
                "relations: the perpendicular names two live lines")) {
      return false;
    }
    const bool corner =
        (is_horizontal_side(*a) && is_vertical_side(*b)) ||
        (is_vertical_side(*a) && is_horizontal_side(*b));
    if (!expect(corner,
                "relations: the right angle joins a horizontal and a vertical side")) {
      return false;
    }
    // The horizontal statement and the right angle must not be redundant: the
    // 90 degree constraint has to involve the horizontal side at one end, or the
    // pair says nothing the axis constraint does not already say.
    if (!expect(relation.first_line_id == horizontal_line_id ||
                    relation.second_line_id == horizontal_line_id,
                "relations: the right angle is anchored on the horizontal side")) {
      return false;
    }
  }

  // Relation ids are the handles the delete command takes: they must exist and
  // be distinct, or deleting one glyph would remove two constraints.
  std::set<std::string> ids;
  for (const auto& relation : parameters.line_relations) {
    if (!expect(!relation.id.empty(), "relations: every relation has an id")) {
      return false;
    }
    if (!expect(ids.insert(relation.id).second,
                "relations: relation ids are unique - duplicate " + relation.id)) {
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// 2. The solver agrees the set is minimal and complete for a rectangle.
// ---------------------------------------------------------------------------
bool test_relations_leave_exactly_four_degrees_of_freedom() {
  DocumentManager manager;
  DocumentState document = build_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "dof: the sketch exists")) return false;
  const auto& parameters = *feature->sketch_parameters;

  if (!expect(parameters.solver_dofs == 4,
              "dof: a rectangle has x, y, width and height free - got " +
                  std::to_string(parameters.solver_dofs))) {
    return false;
  }
  if (!expect(parameters.solver_ok, "dof: the solver converged")) return false;
  if (!expect(parameters.solver_conflicting_count == 0,
              "dof: no conflicting constraint - got " +
                  std::to_string(parameters.solver_conflicting_count))) {
    return false;
  }
  if (!expect(parameters.solver_redundant_count == 0,
              "dof: no redundant constraint - got " +
                  std::to_string(parameters.solver_redundant_count))) {
    return false;
  }

  // The viewport publishes what the solver measured; the badge shows this.
  const auto viewport = polysmith::core::build_viewport_state(document);
  return expect(viewport.solver_dofs == 4,
                "dof: the viewport publishes the solver's 4 DOF - got " +
                    std::to_string(viewport.solver_dofs));
}

// ---------------------------------------------------------------------------
// 3. Enforcing the relations on an exact rectangle is a no-op, bit for bit.
// ---------------------------------------------------------------------------
bool test_refresh_leaves_an_exact_rectangle_untouched() {
  FeatureEntry feature = polysmith::core::create_sketch_feature(1, "ref-plane-xy");
  int next_line_index = 1;
  polysmith::core::add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  // Everything the entity records say, captured before a second recompute.
  std::vector<std::array<double, 4>> before;
  before.reserve(feature.sketch_parameters->lines.size());
  for (const auto& line : feature.sketch_parameters->lines) {
    before.push_back({line.start_x, line.start_y, line.end_x, line.end_y});
  }
  const size_t relation_count = feature.sketch_parameters->line_relations.size();

  // A second recompute is what every edit runs. It must be idempotent.
  polysmith::core::refresh_sketch_derived_state(feature);

  if (!expect(feature.sketch_parameters->line_relations.size() == relation_count,
              "idempotence: a refresh neither adds nor drops relations (" +
                  std::to_string(before.size()) + " lines)")) {
    return false;
  }
  for (size_t i = 0; i < before.size(); ++i) {
    const auto& line = feature.sketch_parameters->lines[i];
    if (!expect(line.start_x == before[i][0] && line.start_y == before[i][1] &&
                    line.end_x == before[i][2] && line.end_y == before[i][3],
                "idempotence: " + line.id +
                    " keeps its exact coordinates across a refresh (x drift " +
                    std::to_string(line.start_x - before[i][0]) + ", " +
                    std::to_string(line.end_x - before[i][2]) + ")")) {
      return false;
    }
  }

  // The short sides are 20 long and the long sides 40: a relation pass that
  // "helpfully" equalised them would move a corner.
  const SketchLine* top = find_line(*feature.sketch_parameters, "line-1");
  const SketchLine* right = find_line(*feature.sketch_parameters, "line-2");
  if (!expect(top != nullptr && right != nullptr,
              "idempotence: the sides exist")) {
    return false;
  }
  return expect(std::abs(std::hypot(top->end_x - top->start_x,
                                    top->end_y - top->start_y) - 40.0) < 1e-12 &&
                    std::abs(std::hypot(right->end_x - right->start_x,
                                        right->end_y - right->start_y) - 20.0) < 1e-12,
                "idempotence: the rectangle keeps its 40x20 proportions");
}

// ---------------------------------------------------------------------------
// 4. Driving one side still works: the relations are a floor, not a cage.
// ---------------------------------------------------------------------------
bool test_driving_the_width_keeps_the_shape_rectangular() {
  DocumentManager manager;
  DocumentState document = build_rectangle(manager);
  FeatureEntry* feature = sketch_feature(document);
  if (!expect(feature != nullptr, "drive: the sketch exists")) return false;

  // line-1 is the top side (add_sketch_rectangle order). Widen it to 60.
  polysmith::core::update_sketch_dimension(*feature, "dim-line-line-1", 60.0);

  const auto& parameters = *feature->sketch_parameters;
  const SketchLine* top = find_line(parameters, "line-1");
  const SketchLine* right = find_line(parameters, "line-2");
  const SketchLine* bottom = find_line(parameters, "line-3");
  const SketchLine* left = find_line(parameters, "line-4");
  if (!expect(top != nullptr && right != nullptr && bottom != nullptr &&
                  left != nullptr,
              "drive: the four sides still exist")) {
    return false;
  }

  const double width =
      std::hypot(top->end_x - top->start_x, top->end_y - top->start_y);
  const double height =
      std::hypot(right->end_x - right->start_x, right->end_y - right->start_y);
  if (!expect(std::abs(width - 60.0) < 1e-6,
              "drive: the top side took the new length - got " +
                  std::to_string(width))) {
    return false;
  }
  // The height is not dimensioned, so nothing *pins* it: what keeps it at 20 is
  // that the ad-hoc length drive translates the perpendicular neighbours instead
  // of rotating them, which leaves the solver nothing to repair. Before that fix
  // the same edit shrank the height to 19.77 mm (the right side had been left
  // 28.28 mm long and diagonal).
  if (!expect(std::abs(height - 20.0) < 1e-6,
              "drive: the height is unchanged - got " + std::to_string(height))) {
    return false;
  }
  // The opposite side follows: a rectangle whose bottom stayed 40 wide is not a
  // rectangle, and that is exactly what the parallel pair is there to prevent.
  const double bottom_width =
      std::hypot(bottom->end_x - bottom->start_x, bottom->end_y - bottom->start_y);
  if (!expect(std::abs(bottom_width - width) < 1e-6,
              "drive: the opposite side matches - top " + std::to_string(width) +
                  ", bottom " + std::to_string(bottom_width))) {
    return false;
  }
  // Corners still coincide, so the shape closes into a profile.
  if (!expect(!parameters.profiles.empty(),
              "drive: the widened rectangle is still a closed profile")) {
    return false;
  }
  // One driving dimension now pins the width, so one DOF is gone.
  if (!expect(parameters.solver_dofs == 3,
              "drive: the width dimension costs exactly one DOF - got " +
                  std::to_string(parameters.solver_dofs))) {
    return false;
  }

  // Repeated edits must not walk the free height away. Each edit re-solves from
  // the previous result, so if the drive were biased the bias would compound
  // here.
  polysmith::core::update_sketch_dimension(*feature, "dim-line-line-1", 80.0);
  polysmith::core::update_sketch_dimension(*feature, "dim-line-line-1", 100.0);
  const SketchLine* right_after = find_line(*feature->sketch_parameters, "line-2");
  if (!expect(right_after != nullptr, "drive: the right side survives two edits")) {
    return false;
  }
  const double height_after = std::hypot(right_after->end_x - right_after->start_x,
                                         right_after->end_y - right_after->start_y);
  return expect(std::abs(height_after - 20.0) < 1e-6,
                "drive: two more edits keep the height exact - got " +
                    std::to_string(height_after));
}

// ---------------------------------------------------------------------------
// 5. An anchored line inside the rectangle follows a height drive, and nothing
//    else moves.
//
// This is the case that showed why the anchor passes have to run BEFORE the
// solver: a midpoint-anchored point is one of the solver's unknowns, so the
// solver expects it to already sit on its host. After a drive moved the host and
// left the anchored point behind, the solver absorbed the mismatch by translating
// the whole rectangle (5.76 mm up for a 10 mm height change) — a valid solution
// that silently moved everything the user did not ask to move.
// ---------------------------------------------------------------------------
bool test_anchored_line_follows_a_height_drive() {
  FeatureEntry feature = polysmith::core::create_sketch_feature(11, "ref-plane-xy");
  int next_line_index = 1;
  polysmith::core::add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);
  polysmith::core::add_sketch_line(feature, next_line_index++, 20.0, 0.0, 20.0, 20.0);
  const std::string start_id = feature.sketch_parameters->lines.back().start_vertex_id;
  const std::string end_id = feature.sketch_parameters->lines.back().end_vertex_id;
  polysmith::core::set_sketch_midpoint_anchor(feature, start_id, "line-1");
  polysmith::core::set_sketch_midpoint_anchor(feature, end_id, "line-3");

  // Halve the height by driving the right side from 20 to 10.
  polysmith::core::update_sketch_dimension(feature, "dim-line-line-2", 10.0);

  const auto& parameters = *feature.sketch_parameters;
  const SketchLine* top = find_line(parameters, "line-1");
  const SketchLine* bottom = find_line(parameters, "line-3");
  const SketchLine* anchored = find_line(parameters, "line-5");
  if (!expect(top != nullptr && bottom != nullptr && anchored != nullptr,
              "anchor drive: the rectangle and the line exist")) {
    return false;
  }

  // The rectangle keeps its position: the top edge stays on y = 0.
  if (!expect(std::abs(top->start_y) < 1e-6 && std::abs(top->end_y) < 1e-6,
              "anchor drive: the rectangle did not move - top edge y " +
                  std::to_string(top->start_y))) {
    return false;
  }
  if (!expect(std::abs(bottom->start_y - 10.0) < 1e-6,
              "anchor drive: the bottom edge took the new height - got y " +
                  std::to_string(bottom->start_y))) {
    return false;
  }
  // The anchored line spans the new height, still vertical, still centred.
  return expect(std::abs(anchored->start_x - 20.0) < 1e-6 &&
                    std::abs(anchored->end_x - 20.0) < 1e-6 &&
                    std::abs(anchored->start_y) < 1e-6 &&
                    std::abs(anchored->end_y - 10.0) < 1e-6,
                "anchor drive: the anchored line follows both edges - got (" +
                    std::to_string(anchored->start_x) + "," +
                    std::to_string(anchored->start_y) + ")->(" +
                    std::to_string(anchored->end_x) + "," +
                    std::to_string(anchored->end_y) + ")");
}

// ---------------------------------------------------------------------------
// 6. Applying a constraint must not silently delete or duplicate one.
//
// Reported from the app as "before constraint is dof 4 and after is 2" — the
// badge collapsed as soon as a constraint was applied to a rectangle side. Two
// separate causes, both fixed:
//
//  - the axis command stored a Horizontal/Vertical that the parallel and
//    perpendicular relations already implied (a redundant constraint), and
//  - the parallel/perpendicular commands wiped the driven line's axis constraint
//    even when the relation already held, deleting the rectangle's one H and
//    making the count go UP instead.
//
// Neither may change the DOF, because neither changes what the sketch can do.
// ---------------------------------------------------------------------------
bool test_applying_a_constraint_keeps_the_degree_count() {
  struct Case {
    const char* label;
    void (*apply)(FeatureEntry&);
    const char* must_stay_horizontal;  // bottom is the one H side
  };
  const Case cases[] = {
      {"horizontal on the top side",
       [](FeatureEntry& feature) {
         polysmith::core::set_sketch_line_constraint(feature, "line-1",
                                                     "horizontal");
       },
       "line-3"},
      {"vertical on the right side",
       [](FeatureEntry& feature) {
         polysmith::core::set_sketch_line_constraint(feature, "line-2",
                                                     "vertical");
       },
       "line-3"},
      {"perpendicular on the square corner",
       [](FeatureEntry& feature) {
         polysmith::core::set_sketch_perpendicular_constraint(feature, "line-3",
                                                             "line-2");
       },
       "line-3"},
      {"parallel on the opposite sides",
       [](FeatureEntry& feature) {
         polysmith::core::set_sketch_parallel_constraint(feature, "line-1",
                                                         "line-3");
       },
       "line-3"},
  };

  for (const auto& test_case : cases) {
    FeatureEntry feature =
        polysmith::core::create_sketch_feature(1, "ref-plane-xy");
    int next_line_index = 1;
    polysmith::core::add_sketch_rectangle(feature, next_line_index, 0.0, 0.0,
                                          40.0, 20.0);
    if (!expect(feature.sketch_parameters->solver_dofs == 4,
                std::string(test_case.label) + ": fixture starts at 4 DOF")) {
      return false;
    }

    test_case.apply(feature);
    const auto& parameters = *feature.sketch_parameters;

    if (!expect(parameters.solver_dofs == 4,
                std::string(test_case.label) +
                    ": the DOF count is unchanged - got " +
                    std::to_string(parameters.solver_dofs))) {
      return false;
    }
    // The rectangle's single H must survive: it is the constraint that keeps the
    // shape's orientation, and it is what the viewport's H glyph shows.
    const SketchLine* horizontal =
        find_line(parameters, test_case.must_stay_horizontal);
    if (!expect(horizontal != nullptr && horizontal->constraint.has_value() &&
                    horizontal->constraint.value() == "horizontal",
                std::string(test_case.label) +
                    ": the horizontal constraint is still there")) {
      return false;
    }
    // And no duplicate was invented on another side.
    int axis_constraint_count = 0;
    for (const auto& line : parameters.lines) {
      if (line.constraint.has_value()) ++axis_constraint_count;
    }
    if (!expect(axis_constraint_count == 1,
                std::string(test_case.label) +
                    ": exactly one axis constraint remains - got " +
                    std::to_string(axis_constraint_count))) {
      return false;
    }
    if (!expect(parameters.solver_conflicting_count == 0 &&
                    parameters.solver_redundant_count == 0,
                std::string(test_case.label) +
                    ": no conflict or redundancy was introduced")) {
      return false;
    }
  }
  return true;
}


// ---------------------------------------------------------------------------
// 7. A contradictory relation must not collapse geometry.
//
// Applying Parallel between two sides of a rectangle that are already
// Perpendicular makes the system unsatisfiable, and planegcs "solves" it by
// shrinking one line to zero length - a zero-length line is trivially parallel
// AND perpendicular to everything. The cleanup then deleted that line with its
// two relations, and the next command failed with "Sketch dimension not found":
// the report in the field was "collapsed line line-2 removed 0 dimension(s), 0
// constraint(s) and 2 relation(s)". A degenerate solution is not a solution, so
// it is rejected and the previous geometry kept.
bool test_a_contradictory_relation_does_not_collapse_a_line() {
  FeatureEntry feature = polysmith::core::create_sketch_feature(1, "ref-plane-xy");
  int next_line_index = 1;
  polysmith::core::add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  try {
    polysmith::core::set_sketch_parallel_constraint(feature, "line-3", "line-2");
  } catch (const std::exception&) {
    // Refusing the contradiction outright is also a fine outcome.
    return true;
  }

  const auto& parameters = *feature.sketch_parameters;
  if (!expect(parameters.lines.size() == 4,
              ("contradiction: all four sides survive - got " +
               std::to_string(parameters.lines.size()))
                  .c_str())) {
    return false;
  }
  for (const auto& line : parameters.lines) {
    const double length = std::hypot(line.end_x - line.start_x,
                                     line.end_y - line.start_y);
    if (!expect(length > 1e-3,
                ("contradiction: " + line.id + " kept its length - got " +
                 std::to_string(length))
                    .c_str())) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!test_rectangle_states_parallel_angle_and_one_horizontal()) return 1;
  if (!test_relations_leave_exactly_four_degrees_of_freedom()) return 1;
  if (!test_refresh_leaves_an_exact_rectangle_untouched()) return 1;
  if (!test_driving_the_width_keeps_the_shape_rectangular()) return 1;
  if (!test_anchored_line_follows_a_height_drive()) return 1;
  if (!test_applying_a_constraint_keeps_the_degree_count()) return 1;
  if (!test_a_contradictory_relation_does_not_collapse_a_line()) return 1;

  std::cout << "rectangle_constraints_test passed\n";
  return 0;
}

