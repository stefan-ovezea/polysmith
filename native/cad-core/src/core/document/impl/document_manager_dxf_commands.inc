  // Reads a DXF file and creates a NEW sketch feature on the given
  // reference plane (default ref-plane-xy) filled with the imported
  // 2D geometry. Unsupported entities are skipped and reported via
  // the structured log and the feature's parameters_summary.
  DocumentState import_dxf(const std::string& file_path,
                           const std::optional<std::string>& plane_id =
                               std::nullopt);

  // Writes the ACTIVE sketch to an ASCII DXF file. Throws when no
  // sketch is active.
  ExportResult export_document_as_dxf(const std::string& file_path) const;
