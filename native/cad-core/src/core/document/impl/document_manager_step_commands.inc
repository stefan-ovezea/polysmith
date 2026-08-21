  // Reads a STEP file and creates a NEW step_import body feature. The
  // file is parsed once (units converted to mm); the translated shape
  // lives in the feature parameters as a live handle plus a B-rep
  // snapshot persisted into the part file — the source .step file is
  // not needed afterwards. Throws before any document mutation when
  // the file is missing or invalid (parse-before-mutate).
  DocumentState import_step(const std::string& file_path);
