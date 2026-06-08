  // Drop legacy named-suffix analytical faces for extrude features
  // (e.g. "<id>:face:top", "<id>:face:base", "<id>:face:side-N"). The
  // per-feature loop above still emits those for backwards
  // compatibility with the old tests / serialization paths, but every
  // extrude body now also gets accurate body-derived faces from
  // `enumerate_body_faces` (with numeric suffixes "<id>:face:0",
  // "<id>:face:1", ...). When both are present they overlap as
  // transparent meshes at nearly-identical world positions, producing
  // a "ghost plane" the user can't easily click through. Body-derived
  // faces always win because they handle filleting, plane-frame
  // rotations, and booleaned topology correctly — analytical ones
  // only ever matched on the simple new-body case.
  {
    auto is_named_suffix_for_extrude =
        [&](const ViewportSolidFace& face) -> bool {
      if (face.owner_kind != "extrude") {
        return false;
      }
      // Find the suffix after the last ":face:" delimiter and check
      // whether it parses as a non-negative integer. Numeric -> body-
      // derived (keep). Non-numeric -> legacy analytical (drop).
      const std::string separator = ":face:";
      const auto pos = face.face_id.rfind(separator);
      if (pos == std::string::npos) {
        return false;
      }
      const std::string suffix =
          face.face_id.substr(pos + separator.size());
      if (suffix.empty()) {
        return false;
      }
      for (const char ch : suffix) {
        if (ch < '0' || ch > '9') {
          return true;
        }
      }
      return false;
    };
    solid_faces.erase(std::remove_if(solid_faces.begin(),
                                     solid_faces.end(),
                                     is_named_suffix_for_extrude),
                      solid_faces.end());
  }
