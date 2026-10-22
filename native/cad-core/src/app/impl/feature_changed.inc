// Builds the phase-D2 reply for a command that changed ONE feature's stored
// parameters in place (extrude depth, hole size, fillet radius, ...).  Downstream
// geometry is derived and recomputed by the core on every refresh; the wire only
// needs the single feature whose parameters actually changed, so a parameter edit
// on a large document stops shipping the whole timeline.
//
// `after` is the document the command returned.  Returns true when the event was
// written -- the caller then stops; false means "send the full state".
inline bool emit_feature_changed(
    const std::string& command_id,
    const std::string& feature_id,
    const polysmith::core::DocumentState& after) {
  const polysmith::core::FeatureEntry* after_feature = nullptr;
  for (const auto& feature : after.feature_history) {
    if (feature.id == feature_id) {
      after_feature = &feature;
      break;
    }
  }
  if (after_feature == nullptr) {
    // The feature was deleted (or the id is unknown): fall back to the full
    // state so the client resyncs rather than invent an entry.
    return false;
  }
  polysmith::protocol::write_message(
      polysmith::protocol::make_feature_changed_event(
          command_id,
          polysmith::protocol::json{
              {"revision", after.revision},
              {"feature_id", feature_id},
              {"feature",
               polysmith::protocol::to_payload(*after_feature,
                                               /*include_opaque=*/false,
                                               /*include_vertices=*/false,
                                               /*include_profiles=*/false)},
              {"removed_feature_ids", polysmith::protocol::json::array()},
          }));
  return true;
}
