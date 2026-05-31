#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct SvgLineSegment {
  double start_x = 0.0;
  double start_y = 0.0;
  double end_x = 0.0;
  double end_y = 0.0;
};

struct SvgImportResult {
  double width = 100.0;
  double height = 100.0;
  std::vector<SvgLineSegment> segments;
  std::vector<std::string> warnings;
};

SvgImportResult flatten_svg_file(const std::string& file_path);

}  // namespace polysmith::core
