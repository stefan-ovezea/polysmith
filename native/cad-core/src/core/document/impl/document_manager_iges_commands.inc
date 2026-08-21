  // Reads an IGES file and creates a NEW iges_import body feature. The
  // file is parsed once (units converted to mm); the translated shape
  // lives in the feature parameters as a live handle plus a B-rep
  // snapshot persisted into the part file — the source .iges file is
  // not needed afterwards. Throws before any document mutation when
  // the file is missing or invalid (parse-before-mutate).
  DocumentState import_iges(const std::string& file_path);
