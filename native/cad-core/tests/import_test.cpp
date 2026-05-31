#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "core/document.h"
#include "core/feature.h"
#include "core/viewport.h"

namespace {

namespace fs = std::filesystem;

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::ImageImportFeatureParameters;
using polysmith::core::PlaneFrame;
using polysmith::core::SvgImportFeatureParameters;
using polysmith::core::build_viewport_state;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

fs::path test_dir() {
  const fs::path dir = fs::temp_directory_path() / "polysmith-import-test";
  std::error_code ignored;
  fs::remove_all(dir, ignored);
  fs::create_directories(dir);
  return dir;
}

void write_file(const fs::path& path, const std::string& contents) {
  std::ofstream stream(path, std::ios::binary);
  stream << contents;
}

PlaneFrame xy_frame() {
  return PlaneFrame{
      .origin_x = 0.0,
      .origin_y = 0.0,
      .origin_z = 0.0,
      .x_axis_x = 1.0,
      .x_axis_y = 0.0,
      .x_axis_z = 0.0,
      .y_axis_x = 0.0,
      .y_axis_y = 1.0,
      .y_axis_z = 0.0,
      .normal_x = 0.0,
      .normal_y = 0.0,
      .normal_z = 1.0,
  };
}

ImageImportFeatureParameters image_params() {
  ImageImportFeatureParameters params;
  params.plane_id = "ref-plane-xy";
  params.plane_frame = xy_frame();
  params.source_width = 400.0;
  params.source_height = 200.0;
  params.width_mm = 100.0;
  params.height_mm = 50.0;
  params.lock_aspect = true;
  return params;
}

SvgImportFeatureParameters svg_params() {
  SvgImportFeatureParameters params;
  params.plane_id = "ref-plane-xy";
  params.plane_frame = xy_frame();
  params.width_mm = 20.0;
  params.height_mm = 10.0;
  params.lock_aspect = true;
  return params;
}

const polysmith::core::FeatureEntry* find_kind(const DocumentState& document,
                                               const std::string& kind) {
  const auto it = std::find_if(document.feature_history.begin(),
                               document.feature_history.end(),
                               [&](const auto& feature) {
                                 return feature.kind == kind;
                               });
  return it == document.feature_history.end() ? nullptr : &(*it);
}

int count_kind(const DocumentState& document, const std::string& kind) {
  return static_cast<int>(
      std::count_if(document.feature_history.begin(),
                    document.feature_history.end(),
                    [&](const auto& feature) { return feature.kind == kind; }));
}

bool test_image_import_lifecycle_and_assets() {
  const fs::path dir = test_dir();
  const fs::path source = dir / "reference.png";
  write_file(source, "not-a-real-png-but-core-only-copies-it");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.create_image_import(image_params(), source.string());
  const auto* image = find_kind(document, "image_import");
  if (!expect(image != nullptr, "expected image import feature") ||
      !expect(image->image_import_parameters.has_value(),
              "expected image import parameters")) {
    return false;
  }
  const std::string staged_asset = image->image_import_parameters->asset_path;
  if (!expect(image->image_import_parameters->is_pending,
              "expected image import to start pending") ||
      !expect(fs::exists(staged_asset), "expected staged image asset")) {
    return false;
  }

  auto updated = image->image_import_parameters.value();
  updated.width_mm = 50.0;
  updated.height_mm = 25.0;
  document = manager.update_image_import(image->id, updated);
  image = find_kind(document, "image_import");
  if (!expect(image->image_import_parameters->width_mm == 50.0,
              "expected image width update")) {
    return false;
  }

  document = manager.confirm_image_import(image->id);
  image = find_kind(document, "image_import");
  if (!expect(image != nullptr && !image->image_import_parameters->is_pending,
              "expected confirmed image import")) {
    return false;
  }

  const fs::path save_path = dir / "Part.polysmith";
  manager.save_document_to_path(save_path.string());
  DocumentManager reloaded;
  document = reloaded.load_document_from_path(save_path.string());
  image = find_kind(document, "image_import");
  if (!expect(image != nullptr, "expected saved image import") ||
      !expect(!image->image_import_parameters->relative_asset_path.empty(),
              "expected relative image asset path") ||
      !expect(fs::exists(image->image_import_parameters->asset_path),
              "expected copied sidecar image asset")) {
    return false;
  }

  fs::remove(image->image_import_parameters->asset_path);
  document = reloaded.load_document_from_path(save_path.string());
  image = find_kind(document, "image_import");
  return expect(image != nullptr, "expected image after missing-asset load") &&
         expect(std::find(image->image_import_parameters->warnings.begin(),
                          image->image_import_parameters->warnings.end(),
                          "Imported asset file is missing") !=
                    image->image_import_parameters->warnings.end(),
                "expected missing asset warning");
}

bool test_image_cancel_removes_pending_feature_and_staged_asset() {
  const fs::path dir = test_dir();
  const fs::path source = dir / "cancel.png";
  write_file(source, "copied");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.create_image_import(image_params(), source.string());
  const auto* image = find_kind(document, "image_import");
  const std::string staged_asset = image->image_import_parameters->asset_path;
  document = manager.cancel_image_import(image->id);
  return expect(count_kind(document, "image_import") == 0,
                "expected canceled image feature to be removed") &&
         expect(!fs::exists(staged_asset),
                "expected canceled image staged asset to be removed");
}

bool test_svg_import_creates_one_sketch_without_asset_preview() {
  const fs::path dir = test_dir();
  const fs::path source = dir / "logo.svg";
  write_file(source,
             R"(<svg width="10" height="10" viewBox="0 0 10 10"><rect x="0" y="0" width="10" height="10"/></svg>)");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.create_svg_import(svg_params(), source.string());
  const auto viewport = build_viewport_state(document);
  const auto* sketch = find_kind(document, "sketch");
  if (!expect(count_kind(document, "svg_import") == 0,
              "expected SVG import to create no asset feature") ||
      !expect(viewport.svg_import_previews.empty(),
              "expected SVG import to create no asset preview") ||
      !expect(count_kind(document, "sketch") == 1,
              "expected SVG import to create exactly one sketch") ||
      !expect(sketch != nullptr && sketch->sketch_parameters.has_value(),
              "expected imported SVG sketch parameters") ||
      !expect(sketch->sketch_parameters->lines.size() == 4,
              "expected rectangle SVG to become four sketch lines")) {
    return false;
  }
  const auto& first = sketch->sketch_parameters->lines.front();
  return expect(std::abs(first.start_x + 10.0) < 1e-6,
                "expected SVG x transform start") &&
         expect(std::abs(first.start_y - 5.0) < 1e-6,
                "expected SVG y transform start") &&
         expect(std::abs(first.end_x - 10.0) < 1e-6,
                "expected SVG x transform end") &&
         expect(std::abs(first.end_y - 5.0) < 1e-6,
                "expected SVG y transform end");
}

bool test_svg_without_usable_geometry_fails_cleanly() {
  const fs::path dir = test_dir();
  const fs::path source = dir / "empty.svg";
  write_file(source,
             R"(<svg width="10" height="10"><filter id="f"></filter><text>Text</text></svg>)");

  DocumentManager manager;
  manager.create_document();
  try {
    manager.create_svg_import(svg_params(), source.string());
  } catch (const std::exception&) {
    return expect(count_kind(manager.get_document().value(), "sketch") == 0,
                  "expected failed SVG import to leave no sketch");
  }
  std::cerr << "expected SVG import to reject empty geometry\n";
  return false;
}

}  // namespace

int main() {
  if (!test_image_import_lifecycle_and_assets()) return 1;
  if (!test_image_cancel_removes_pending_feature_and_staged_asset()) return 1;
  if (!test_svg_import_creates_one_sketch_without_asset_preview()) return 1;
  if (!test_svg_without_usable_geometry_fails_cleanly()) return 1;

  std::cout << "import_test passed\n";
  return 0;
}
