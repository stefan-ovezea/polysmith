      for (const auto& rectangle : profiles.polygons) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      rectangle.id) != view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_rectangle_profile_primitive(rectangle, is_selected_profile));
      }

      for (const auto& circle_profile : profiles.circles) {
        const bool is_selected_profile =
            std::find(view->selected_sketch_profile_ids.begin(),
                      view->selected_sketch_profile_ids.end(),
                      circle_profile.id) !=
            view->selected_sketch_profile_ids.end();
        sketch_profiles.push_back(
            make_circle_profile_primitive(circle_profile, is_selected_profile));
      }
