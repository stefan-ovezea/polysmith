  for (auto& face : solid_faces) {
    if (!face.appearance_color.has_value()) {
      face.appearance_color =
          face_appearance_color(*view, face.face_id, "semantic:" + face.face_id);
    }
  }
