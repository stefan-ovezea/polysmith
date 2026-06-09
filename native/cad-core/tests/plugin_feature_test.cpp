#include <exception>
#include <iostream>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopoDS_Shape.hxx>

#include "core/document/document_manager.h"
#include "core/geometry/body_compiler.h"
#include "core/geometry/feature_shape.h"
#include "core/plugin/plugin_feature.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::PluginFeatureParameters;
using polysmith::core::PluginGeometryOperation;
using polysmith::core::build_plugin_feature_shape;
using polysmith::core::compile_bodies;
using polysmith::core::create_plugin_feature;
using polysmith::core::validate_plugin_feature_parameters;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

PluginFeatureParameters make_plugin_parameters() {
  PluginFeatureParameters parameters{};
  parameters.plugin_id = "example.plugin";
  parameters.feature_type = "example_box";
  parameters.display_name = "Example Plugin Body";
  parameters.parameters_summary = "Example 2 x 1";
  parameters.parameters_json = R"({"example":true})";
  parameters.geometry.push_back(PluginGeometryOperation{
      .operation = "add",
      .primitive = "tapered_rounded_box",
      .x = 0.0,
      .y = 0.0,
      .z = 0.0,
      .width = 34.0,
      .depth = 24.0,
      .height = 10.0,
      .radius = 3.0,
      .top_width = 40.0,
      .top_depth = 30.0,
      .top_radius = 4.0,
      .top_offset_x = 1.0,
      .top_offset_y = -1.0,
  });
  parameters.geometry.push_back(PluginGeometryOperation{
      .operation = "subtract",
      .primitive = "box",
      .x = 5.0,
      .y = 5.0,
      .z = 2.0,
      .width = 10.0,
      .depth = 10.0,
      .height = 8.0,
      .radius = 0.0,
  });
  return parameters;
}

bool test_creates_generic_plugin_feature() {
  const auto parameters = make_plugin_parameters();
  const auto feature = create_plugin_feature(1, parameters);
  return expect(feature.kind == "plugin_feature",
                "expected generic plugin feature kind") &&
         expect(feature.plugin_parameters.has_value(),
                "expected generic plugin parameters") &&
         expect(feature.name == "Example Plugin Body",
                "expected plugin display name to be stored");
}

bool test_builds_shape_from_generic_recipe() {
  const auto shape = build_plugin_feature_shape(make_plugin_parameters());
  if (!expect(!shape.IsNull(), "expected plugin recipe to build a shape")) {
    return false;
  }

  Bnd_Box bounds;
  BRepBndLib::Add(shape, bounds);
  double xmin = 0.0;
  double ymin = 0.0;
  double zmin = 0.0;
  double xmax = 0.0;
  double ymax = 0.0;
  double zmax = 0.0;
  bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);

  return expect((xmax - xmin) > 35.0, "expected recipe width in X") &&
         expect((ymax - ymin) > 8.0, "expected recipe height in Y") &&
         expect((zmax - zmin) > 25.0, "expected recipe depth in Z");
}

bool test_compiles_generic_plugin_body() {
  DocumentManager manager;
  manager.create_document();
  const DocumentState document =
      manager.create_plugin_feature(make_plugin_parameters());
  const auto bodies = compile_bodies(document);

  return expect(!bodies.bodies.empty(), "expected compiled plugin body") &&
         expect(!bodies.bodies.front().shape.IsNull(),
                "expected compiled plugin shape");
}

bool test_rejects_empty_recipe() {
  auto parameters = make_plugin_parameters();
  parameters.geometry.clear();
  try {
    validate_plugin_feature_parameters(parameters);
  } catch (const std::exception&) {
    return true;
  }
  return expect(false, "expected empty plugin geometry recipe to be rejected");
}

}  // namespace

int main() {
  if (!test_creates_generic_plugin_feature() ||
      !test_builds_shape_from_generic_recipe() ||
      !test_compiles_generic_plugin_body() ||
      !test_rejects_empty_recipe()) {
    return 1;
  }

  std::cout << "plugin_feature_test passed\n";
  return 0;
}
