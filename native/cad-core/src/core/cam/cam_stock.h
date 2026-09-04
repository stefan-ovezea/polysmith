#pragma once

// Stock geometry helper — the CORE half of the stock-extent contract.
//
// The UI draws the stock box around the part (camSceneObjects.ts
// addStockBoundingBox) and the core needs the SAME extents to anchor
// WCS origins to stock faces and to plan multi-pass levels from the
// stock top.  The contract lives in exactly two places — this file and
// camSceneObjects.ts — keep them in parity:
//
//   * stock center = bounding-box center of the compiled body shapes
//     (the UI's modelCenterFromBodies).
//   * bounding_box: dims = size + 2*margin — `size` is REQUIRED (the
//     UI's [120,120,20] fallback is a rendering default, not stock
//     state; the core never invents stock geometry).
//   * cylinder: width/height = diameter + 2*margin, depth =
//     (length ?? 20) + 2*margin (length default matches the UI).
//   * from_solid / from_mesh: unsupported here (invalid extents).
//
// Face names use CAD convention: right +X, left -X, front +Y, back -Y,
// top +Z, bottom -Z.

#include <array>
#include <string>

namespace polysmith::core {

struct DocumentState;
struct StockDefinition;

namespace cam_stock {

struct StockBoxExtents {
  bool valid = false;
  std::array<double, 3> center{};
  std::array<double, 3> size{};
};

// The stock box as the UI draws it, or invalid when the document has no
// bodies or the stock definition cannot produce extents.
StockBoxExtents stock_box_extents(const DocumentState& document,
                                  const StockDefinition& stock);

// Center of the named stock face ("top"|"bottom"|"front"|"back"|"left"
// |"right") in world coordinates.  A cylinder stock only has real top
// and bottom faces — every other name returns false.
bool stock_face_center(const DocumentState& document,
                       const StockDefinition& stock, const std::string& face,
                       std::array<double, 3>& out);

// Z of the stock top face (world), for multi-pass level planning.
bool stock_top_z(const DocumentState& document, const StockDefinition& stock,
                 double& out);

}  // namespace cam_stock
}  // namespace polysmith::core
