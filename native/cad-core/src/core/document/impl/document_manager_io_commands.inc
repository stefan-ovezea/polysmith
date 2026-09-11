  ExportResult export_document_as_step(const std::string& file_path) const;
  ExportResult export_document_as_iges(const std::string& file_path) const;
  ExportResult export_document_as_stl(const std::string& file_path) const;
  ExportResult export_cam_gcode(const std::string& file_path) const;
  CamGcodeTextResult export_cam_gcode_text() const;
  ExportResult export_body_as_stl(const std::string& file_path,
                                  const std::string& body_id) const;
  ExportResult export_body_as_step(const std::string& file_path,
                                   const std::string& body_id) const;
