  // Imports one PCB fabrication file (Gerber or Excellon) as a `pcb_layer`
  // feature.  The role is resolved from the file's own attributes and, failing
  // that, its name; `role` overrides that (the layer table's explicit choice).
  // Parses BEFORE mutating, so a bad file leaves the document untouched.
  // Throws only for an empty path or a missing file; an unreadable *content*
  // is reported through the returned state's feature summary and the log.
  DocumentState import_pcb_layer(const std::string& file_path,
                                 const std::optional<std::string>& role = std::nullopt);

  // Re-resolves one imported layer: an explicit role ("" clears the override,
  // returning to the file's own attributes) and the bottom-side mirror flag.
  DocumentState update_pcb_layer(const std::string& feature_id,
                                 const std::optional<std::string>& role,
                                 const std::optional<bool>& mirror_x);

  // Removes an imported layer from the timeline.
  DocumentState remove_pcb_layer(const std::string& feature_id);
