#include "core/cam/cam_stock.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include "core/cam/cam_types.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core::cam_stock {

namespace {

// Union bounds of every compiled body shape — the UI's
// modelCenterFromBodies.  Returns false when the document has no
// shapes (or an empty bounds box).
bool body_union_bounds(const DocumentState& document,
                       std::array<double, 3>& center) {
  const CompiledBodies compiled =
      compile_bodies(document, /*include_meshes=*/false);
  Bnd_Box bounds;
  for (const auto& body : compiled.bodies) {
    BRepBndLib::Add(body.shape, bounds);
  }
  if (bounds.IsVoid()) {
    return false;
  }
  const double min_x = bounds.CornerMin().X();
  const double min_y = bounds.CornerMin().Y();
  const double min_z = bounds.CornerMin().Z();
  const double max_x = bounds.CornerMax().X();
  const double max_y = bounds.CornerMax().Y();
  const double max_z = bounds.CornerMax().Z();
  center = {(min_x + max_x) / 2.0, (min_y + max_y) / 2.0,
            (min_z + max_z) / 2.0};
  return true;
}

}  // namespace

StockBoxExtents stock_box_extents(const DocumentState& document,
                                  const StockDefinition& stock) {
  StockBoxExtents result;
  if (!body_union_bounds(document, result.center)) {
    return result;  // invalid — no part to wrap
  }

  const double margin = stock.margin;
  if (stock.type == "bounding_box") {
    // `size` is required — the UI's [120,120,20] fallback is a
    // rendering default, not stock state.
    if (!stock.size.has_value()) {
      return result;
    }
    const auto& size = stock.size.value();
    result.size = {size[0] + 2.0 * margin, size[1] + 2.0 * margin,
                   size[2] + 2.0 * margin};
  } else if (stock.type == "cylinder") {
    if (!stock.diameter.has_value()) {
      return result;
    }
    const double diameter = stock.diameter.value() + 2.0 * margin;
    const double length = stock.length.value_or(20.0) + 2.0 * margin;
    result.size = {diameter, diameter, length};
  } else {
    return result;  // from_solid / from_mesh: no analytic extents yet
  }

  for (const double dimension : result.size) {
    if (dimension <= 0.0) {
      result = StockBoxExtents{};
      return result;
    }
  }
  result.valid = true;
  return result;
}

bool stock_face_center(const DocumentState& document,
                       const StockDefinition& stock, const std::string& face,
                       std::array<double, 3>& out) {
  const StockBoxExtents extents = stock_box_extents(document, stock);
  if (!extents.valid) {
    return false;
  }

  // A cylinder stock renders as a cylinder with only two real faces —
  // the side "faces" of the box artifact don't exist.
  if (stock.type == "cylinder" && face != "top" && face != "bottom") {
    return false;
  }

  const double half_x = extents.size[0] / 2.0;
  const double half_y = extents.size[1] / 2.0;
  const double half_z = extents.size[2] / 2.0;
  std::array<double, 3> offset{};
  if (face == "top") {
    offset = {0.0, 0.0, half_z};
  } else if (face == "bottom") {
    offset = {0.0, 0.0, -half_z};
  } else if (face == "front") {
    offset = {0.0, half_y, 0.0};
  } else if (face == "back") {
    offset = {0.0, -half_y, 0.0};
  } else if (face == "right") {
    offset = {half_x, 0.0, 0.0};
  } else if (face == "left") {
    offset = {-half_x, 0.0, 0.0};
  } else {
    return false;
  }

  out = {extents.center[0] + offset[0], extents.center[1] + offset[1],
         extents.center[2] + offset[2]};
  return true;
}

bool stock_top_z(const DocumentState& document, const StockDefinition& stock,
                 double& out) {
  std::array<double, 3> top{};
  if (!stock_face_center(document, stock, "top", top)) {
    return false;
  }
  out = top[2];
  return true;
}

}  // namespace polysmith::core::cam_stock
