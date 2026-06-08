  CompiledBodies result;

  // Body root id, in insertion order. We use a parallel vector instead of
  // an ordered map because we need stable "most recent body" semantics:
  // when an extrude is in cut/join mode it targets the body that was the
  // most-recently created or modified.
  std::vector<std::string> body_order;
  std::unordered_map<std::string, TopoDS_Shape> body_shapes;
  std::unordered_map<std::string, BodyLocalFrame> body_frames;
  // When a fillet/chamfer feature with `is_pending=true` is replayed
  // we capture the body shape *before* the op was applied. The
  // viewport later enumerates body edges from this shape so edge ids
  // stay stable for the duration of the panel session, even though
  // the post-op shape (used for visual rendering) keeps mutating as
  // the user toggles edges. Cleared per-body when a non-pending op
  // overwrites that body. See FilletFeatureParameters::is_pending.
  std::unordered_map<std::string, TopoDS_Shape> body_pick_shapes;


#include "core/geometry/impl/compile_bodies_mesh_requirement.inc"

#include "core/geometry/impl/compile_bodies_modifier_replay.inc"

#include "core/geometry/impl/compile_bodies_feature_insert.inc"

#include "core/geometry/impl/compile_bodies_finalize.inc"