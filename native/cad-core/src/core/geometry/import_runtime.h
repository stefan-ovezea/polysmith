#pragma once

#include <string>
#include <unordered_map>

#include <TopoDS_Shape.hxx>

namespace polysmith::core {

// Runtime cache for live imported STEP/IGES shapes, keyed by feature id. The
// live handle is stored at import time and consumed by the body compiler; it is
// never serialized (the persisted `serialized_shape` B-rep snapshot is what
// survives a save/load). Cleared on document create/load so stale handles from
// a previous document can never collide with the restored feature ids.
void store_imported_shape(const std::string& feature_id, const TopoDS_Shape& shape);
const TopoDS_Shape* find_imported_shape(const std::string& feature_id);
void clear_imported_shapes();

}  // namespace polysmith::core
