#include "core/geometry/import_runtime.h"

namespace polysmith::core {
namespace {

std::unordered_map<std::string, TopoDS_Shape>& imported_shapes() {
  static std::unordered_map<std::string, TopoDS_Shape> cache;
  return cache;
}

}  // namespace

void store_imported_shape(const std::string& feature_id,
                          const TopoDS_Shape& shape) {
  imported_shapes()[feature_id] = shape;
}

const TopoDS_Shape* find_imported_shape(const std::string& feature_id) {
  const auto& cache = imported_shapes();
  const auto it = cache.find(feature_id);
  return it == cache.end() ? nullptr : &it->second;
}

void clear_imported_shapes() {
  imported_shapes().clear();
}

}  // namespace polysmith::core
