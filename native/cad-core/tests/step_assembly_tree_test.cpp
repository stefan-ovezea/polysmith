// Regression test for the imported-assembly component tree.
//
// A STEP assembly is not a list of parts: it is a tree of component OCCURRENCES,
// where the same part appears many times under different names ("Bracket:1",
// "Bracket:2"). The importer must carry that structure into the document as
// `ComponentNode`s, one per occurrence, each owning the bodies it places — that
// is what the Browser renders instead of a thousand-row body list.
//
// Covered here:
//   - the reader's product-structure walk (nesting, occurrence names, and which
//     solids each occurrence owns),
//   - the document's component nodes after `import_step` (parents resolve, ids
//     are feature ids, every body is placed exactly once),
//   - the tree surviving save/load,
//   - deleting the import removing its whole tree.
//
// The fixture is generated at runtime through XCAF so the test owns the exact
// hierarchy it asserts on: a root assembly with two placements of one bracket, a
// placed pin, and a sub-assembly that holds the same pin again.

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "core/document/document.h"
#include "core/geometry/step_import_helpers.h"

namespace {

using polysmith::core::ComponentNode;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::StepAssemblyNode;
using polysmith::core::StepImportResult;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

std::string temp_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() /
          ("polysmith_step_assembly_" + name + ".step"))
      .string();
}

std::string document_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() /
          ("polysmith_step_assembly_" + name + ".json"))
      .string();
}

// Writes the fixture assembly:
//
//   Root
//     Bracket:1        (box)
//     Bracket:2        (the same box, translated)
//     Wheel:1          (sub-assembly)
//       Pin:1          (cylinder)
//
// Two placements of one part, one nested level: exactly the shape of structure a
// real board file has, and the two things a flat body list cannot express.
std::string write_assembly_fixture(const std::string& name) {
  occ::handle<TDocStd_Document> document;
  const occ::handle<XCAFApp_Application> application =
      XCAFApp_Application::GetApplication();
  application->NewDocument("MDTV-XCAF", document);
  const occ::handle<XCAFDoc_ShapeTool> shape_tool =
      XCAFDoc_DocumentTool::ShapeTool(document->Main());

  const TDF_Label bracket_part =
      shape_tool->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
  TDataStd_Name::Set(bracket_part, "Bracket");
  const TDF_Label pin_part =
      shape_tool->AddShape(BRepPrimAPI_MakeCylinder(3.0, 20.0).Shape(), false);
  TDataStd_Name::Set(pin_part, "Pin");

  const TDF_Label wheel = shape_tool->NewShape();
  TDataStd_Name::Set(wheel, "Wheel");
  const TDF_Label wheel_pin =
      shape_tool->AddComponent(wheel, pin_part, TopLoc_Location());
  TDataStd_Name::Set(wheel_pin, "Pin:1");

  const TDF_Label root = shape_tool->NewShape();
  TDataStd_Name::Set(root, "Root");
  const TDF_Label bracket_1 =
      shape_tool->AddComponent(root, bracket_part, TopLoc_Location());
  TDataStd_Name::Set(bracket_1, "Bracket:1");
  gp_Trsf shift;
  shift.SetTranslation(gp_Vec(40.0, 0.0, 0.0));
  const TDF_Label bracket_2 =
      shape_tool->AddComponent(root, bracket_part, TopLoc_Location(shift));
  TDataStd_Name::Set(bracket_2, "Bracket:2");
  const TDF_Label wheel_1 =
      shape_tool->AddComponent(root, wheel, TopLoc_Location());
  TDataStd_Name::Set(wheel_1, "Wheel:1");
  // The assembly compounds are built from the component placements; without this
  // the document has components but no shape to transfer.
  shape_tool->UpdateAssemblies();

  STEPCAFControl_Writer writer;
  writer.SetNameMode(true);
  writer.SetColorMode(true);
  if (writer.Transfer(document, STEPControl_AsIs) != IFSelect_RetDone) {
    throw std::runtime_error("fixture: XCAF -> STEP transfer failed");
  }
  const std::string path = temp_path(name);
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    throw std::runtime_error("fixture: STEP write failed");
  }
  return path;
}

const StepAssemblyNode* find_node(const std::vector<StepAssemblyNode>& nodes,
                                  const std::string& name) {
  for (const StepAssemblyNode& node : nodes) {
    if (node.name == name) {
      return &node;
    }
  }
  return nullptr;
}

const ComponentNode* find_component(const DocumentState& document,
                                    const std::string& name) {
  for (const ComponentNode& component : document.components) {
    if (component.name == name) {
      return &component;
    }
  }
  return nullptr;
}

int count_import_bodies(const DocumentState& document) {
  return static_cast<int>(std::count_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const polysmith::core::FeatureEntry& feature) {
        return feature.kind == "step_import";
      }));
}

// The reader must see the file's structure: three top-level occurrences, one of
// them an assembly holding its own part, and every solid placed exactly once.
bool test_reader_walks_the_product_structure() {
  const std::string path = write_assembly_fixture("reader");
  const StepImportResult parsed = polysmith::core::read_step_file(path);

  if (!expect(parsed.solids.size() == 3,
              "reader: three solids expected, got " +
                  std::to_string(parsed.solids.size()))) {
    return false;
  }
  if (!expect(parsed.assembly.size() == 4,
              "reader: four occurrences expected, got " +
                  std::to_string(parsed.assembly.size()))) {
    return false;
  }

  // Pre-order: the root's components first, then what the sub-assembly holds.
  if (!expect(parsed.assembly[0].name == "Bracket:1",
              "reader: first occurrence must be Bracket:1, got '" +
                  parsed.assembly[0].name + "'") ||
      !expect(parsed.assembly[1].name == "Bracket:2",
              "reader: second occurrence must be Bracket:2, got '" +
                  parsed.assembly[1].name + "'") ||
      !expect(parsed.assembly[2].name == "Wheel:1",
              "reader: third occurrence must be Wheel:1, got '" +
                  parsed.assembly[2].name + "'") ||
      !expect(parsed.assembly[3].name == "Pin:1",
              "reader: nested occurrence must be Pin:1, got '" +
                  parsed.assembly[3].name + "'")) {
    return false;
  }
  if (!expect(parsed.assembly[0].parent == -1 &&
                  parsed.assembly[1].parent == -1 &&
                  parsed.assembly[2].parent == -1,
              "reader: the root's components must be top level")) {
    return false;
  }
  if (!expect(parsed.assembly[3].parent == 2,
              "reader: the pin must be nested under Wheel:1")) {
    return false;
  }
  // The two brackets are two placements of ONE part, and each owns one body;
  // the assembly above them owns none (its shape contains its children's).
  if (!expect(parsed.assembly[0].solid_indices.size() == 1 &&
                  parsed.assembly[1].solid_indices.size() == 1,
              "reader: each bracket occurrence must own exactly one solid")) {
    return false;
  }
  if (!expect(parsed.assembly[2].solid_indices.empty(),
              "reader: a sub-assembly must not own the bodies of its children")) {
    return false;
  }
  if (!expect(parsed.assembly[3].solid_indices.size() == 1,
              "reader: the pin occurrence must own exactly one solid")) {
    return false;
  }
  if (!expect(parsed.assembly[0].solid_indices[0] !=
                  parsed.assembly[1].solid_indices[0],
              "reader: the two bracket placements must be different solids")) {
    return false;
  }
  return true;
}

// The document must carry the same tree: occurrences with resolvable parents and
// body ids that are the imported bodies, each body placed exactly once.
bool test_import_builds_component_nodes() {
  const std::string path = write_assembly_fixture("document");
  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

  const int bodies = count_import_bodies(document);
  if (!expect(bodies == 3, "document: three imported bodies expected, got " +
                               std::to_string(bodies))) {
    return false;
  }
  if (!expect(document.components.size() == 4,
              "document: four component nodes expected, got " +
                  std::to_string(document.components.size()))) {
    return false;
  }

  const ComponentNode* wheel = find_component(document, "Wheel:1");
  const ComponentNode* pin = find_component(document, "Pin:1");
  const ComponentNode* bracket_1 = find_component(document, "Bracket:1");
  const ComponentNode* bracket_2 = find_component(document, "Bracket:2");
  if (!expect(wheel != nullptr && pin != nullptr && bracket_1 != nullptr &&
                  bracket_2 != nullptr,
              "document: every occurrence must have a node")) {
    return false;
  }
  if (!expect(pin->parent_id == wheel->id,
              "document: the pin's parent must be Wheel:1")) {
    return false;
  }
  if (!expect(bracket_1->parent_id.empty() && bracket_2->parent_id.empty() &&
                  wheel->parent_id.empty(),
              "document: the root's occurrences must have no parent")) {
    return false;
  }

  // Body ids are feature ids, and every imported body is placed exactly once.
  std::vector<std::string> placed;
  for (const ComponentNode& component : document.components) {
    placed.insert(placed.end(), component.body_ids.begin(),
                  component.body_ids.end());
  }
  if (!expect(static_cast<int>(placed.size()) == bodies,
              "document: every body must be placed exactly once, placed " +
                  std::to_string(placed.size()) + " of " +
                  std::to_string(bodies))) {
    return false;
  }
  for (const std::string& body_id : placed) {
    const bool exists = std::any_of(
        document.feature_history.begin(), document.feature_history.end(),
        [&](const polysmith::core::FeatureEntry& feature) {
          return feature.id == body_id && feature.kind == "step_import";
        });
    if (!expect(exists, "document: body id '" + body_id +
                            "' must be an imported body feature")) {
      return false;
    }
  }
  if (!expect(bracket_1->body_ids.size() == 1 && bracket_2->body_ids.size() == 1 &&
                  pin->body_ids.size() == 1,
              "document: each leaf occurrence must own exactly one body")) {
    return false;
  }
  if (!expect(bracket_1->body_ids[0] != bracket_2->body_ids[0],
              "document: the two bracket placements must own different bodies")) {
    return false;
  }
  if (!expect(wheel->body_ids.empty(),
              "document: a sub-assembly must own no bodies itself")) {
    return false;
  }
  return true;
}

// The tree is document content: it must come back from the saved file, with the
// same ids, so the Browser rebuilds the hierarchy after a reload.
bool test_components_survive_save_and_load() {
  const std::string path = write_assembly_fixture("roundtrip");
  DocumentManager manager;
  manager.create_document();
  const DocumentState imported = manager.import_step(path);
  const std::string saved = document_path("roundtrip");
  manager.save_document_to_path(saved);
  const DocumentState loaded = manager.load_document_from_path(saved);

  if (!expect(loaded.components.size() == imported.components.size(),
              "save/load: component count must survive (expected " +
                  std::to_string(imported.components.size()) + ", got " +
                  std::to_string(loaded.components.size()) + ")")) {
    return false;
  }
  for (const ComponentNode& before : imported.components) {
    const ComponentNode* after = find_component(loaded, before.name);
    if (!expect(after != nullptr, "save/load: missing occurrence '" +
                                      before.name + "'")) {
      return false;
    }
    if (!expect(after->id == before.id && after->parent_id == before.parent_id &&
                    after->body_ids == before.body_ids,
                "save/load: occurrence '" + before.name + "' must round-trip")) {
      return false;
    }
  }
  return true;
}

// Removing the import removes its tree: a component is provenance of the file,
// not something that outlives the bodies it places.
bool test_deleting_the_import_drops_its_components() {
  const std::string path = write_assembly_fixture("delete");
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_step(path);
  if (!expect(!document.components.empty(),
              "delete: the import must have created components")) {
    return false;
  }
  document = manager.delete_feature(document.feature_history.back().id);
  if (!expect(document.components.empty(),
              "delete: deleting the import must drop every component, " +
                  std::to_string(document.components.size()) + " left")) {
    return false;
  }
  if (!expect(document.feature_history.size() == 1,
              "delete: only the root feature must be left")) {
    return false;
  }
  return true;
}

}  // namespace

int main() {
  std::cerr << "[step_assembly_tree_test] test 1: reader walks the product "
               "structure\n";
  if (!test_reader_walks_the_product_structure()) return 1;
  std::cerr << "[step_assembly_tree_test] test 2: import builds component nodes\n";
  if (!test_import_builds_component_nodes()) return 1;
  std::cerr << "[step_assembly_tree_test] test 3: components survive save/load\n";
  if (!test_components_survive_save_and_load()) return 1;
  std::cerr << "[step_assembly_tree_test] test 4: deleting the import drops its "
               "components\n";
  if (!test_deleting_the_import_drops_its_components()) return 1;
  std::cerr << "[step_assembly_tree_test] all tests passed\n";
  return 0;
}
