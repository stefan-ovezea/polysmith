  if (payload.contains("dimensions") && payload.at("dimensions").is_array()) {
    for (const auto& dim_payload : payload.at("dimensions")) {
      polysmith::core::SketchDimension dimension{};
      dimension.id = read_string(dim_payload, "dimension_id");
      dimension.kind = read_string(dim_payload, "kind");
      dimension.entity_id = read_string(dim_payload, "entity_id");
      // Older saves predate angle dimensions; fall back to "" so the
      // unary dimensions round-trip without churn.
      if (dim_payload.contains("secondary_entity_id") &&
          dim_payload.at("secondary_entity_id").is_string()) {
        dimension.secondary_entity_id =
            dim_payload.at("secondary_entity_id").get<std::string>();
      }
      dimension.value = read_number(dim_payload, "value");
      // Expression field — absent in older saves, default to ""
      if (dim_payload.contains("expression") &&
          dim_payload.at("expression").is_string()) {
        dimension.expression =
            dim_payload.at("expression").get<std::string>();
      }
      // Driven field — absent in older saves, default to false (driving)
      if (dim_payload.contains("driven") && dim_payload.at("driven").is_boolean()) {
        dimension.driven = dim_payload.at("driven").get<bool>();
      }
      // is_auto field — absent in older saves, default to false.
      // A post-load fixup (below) sets it to true for dimensions whose
      // IDs match the auto-dimension naming pattern — but ONLY when the payload
      // did not say, see `is_auto_was_present`.
      const bool is_auto_was_present =
          dim_payload.contains("is_auto") &&
          dim_payload.at("is_auto").is_boolean();
      if (is_auto_was_present) {
        dimension.is_auto = dim_payload.at("is_auto").get<bool>();
      }
      if (!is_auto_was_present) {
        // Legacy save: guess from the AUTO-DIMENSION ID PATTERN.
        if (dimension.id.rfind("dim-line-", 0) == 0 ||
            dimension.id.rfind("dim-line-angle-", 0) == 0 ||
            dimension.id.rfind("dim-circle-", 0) == 0 ||
            dimension.id.rfind("dim-polygon-", 0) == 0) {
          dimension.is_auto = true;
        }
      }
      // Display_as field — absent in older saves, default to "" (diameter)
      if (dim_payload.contains("display_as") &&
          dim_payload.at("display_as").is_string()) {
        dimension.display_as =
            dim_payload.at("display_as").get<std::string>();
      }
      if (dimension.kind == "circle_radius" &&
          dimension.display_as != "radius") {
        // The payload carries the displayed diameter; store the
        // radius for the solver and the sync passes.
        dimension.value /= 2.0;
      }
      if (dim_payload.contains("label_x") &&
          dim_payload.at("label_x").is_number()) {
        dimension.label_x = dim_payload.at("label_x").get<double>();
      }
      if (dim_payload.contains("label_y") &&
          dim_payload.at("label_y").is_number()) {
        dimension.label_y = dim_payload.at("label_y").get<double>();
      }
      if (dim_payload.contains("pivot_x") &&
          dim_payload.at("pivot_x").is_number()) {
        dimension.pivot_x = dim_payload.at("pivot_x").get<double>();
      }
      if (dim_payload.contains("pivot_y") &&
          dim_payload.at("pivot_y").is_number()) {
        dimension.pivot_y = dim_payload.at("pivot_y").get<double>();
      }
      if (dim_payload.contains("constraint_id") &&
          dim_payload.at("constraint_id").is_string()) {
        dimension.constraint_id = dim_payload.at("constraint_id").get<std::string>();
      }
      params.dimensions.push_back(dimension);
    }
    // The `is_auto` fallback for legacy saves now runs per dimension, inside the
    // loop, and only when the payload did not carry the field.
    //
    // It used to run unconditionally over every loaded dimension: any id starting
    // with `dim-line-` was re-marked AUTO, even when the file said
    // `"is_auto": false`. A dimension the user creates with the Dimension tool is
    // exactly that — the auto one promoted in place, keeping its
    // `dim-line-line-1` id — so every reload silently demoted the user's driving
    // length dimensions to display-only readouts. Measured 2026-11-06 on the
    // user's fully-constrained sketch: the file holds `is_auto: false` and
    // `value: 223.0223`, and after `load_document` the same dimensions read
    // `is_auto: true`, which removed them from the solver and let a drag change
    // the length to 233.158 with the readouts following.
  }
