// Document format version + migration regression test (Stage 2, A.3).
//
// Verifies the saved envelope, the v0 -> v1 migration, and that damaged or
// future-version files error cleanly instead of crashing.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "core/document/document.h"
#include "core/document/format/format_version.h"
#include "core/document/format/journal.h"
#include "core/geometry/body_compiler.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::BoxFeatureParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

std::filesystem::path write_temp(const std::string& name,
                                 const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream stream(path);
  stream << content;
  return path;
}

BoxFeatureParameters make_box() {
  BoxFeatureParameters box;
  box.width = 40.0;
  box.height = 30.0;
  box.depth = 20.0;
  return box;
}

bool test_journal() {
  const std::string doc = "format_version_test_journal";
  polysmith::core::journal_truncate(doc);
  polysmith::core::journal_append(doc, R"({"type":"create_document"})");
  polysmith::core::journal_append(doc, R"({"type":"add_box_feature"})");
  const auto lines = polysmith::core::journal_lines(doc);
  bool ok = expect(lines.size() == 2, "journal records two lines");
  polysmith::core::journal_truncate(doc);
  ok &= expect(polysmith::core::journal_lines(doc).empty(),
               "journal truncates on save");
  return ok;
}

}  // namespace

int main() {
  bool ok = true;

  DocumentManager source;
  source.create_document();
  source.add_box_feature(make_box());
  const DocumentState document = source.get_document().value();

  // Fixtures.
  const auto v1_path =
      std::filesystem::temp_directory_path() / "format_v1.polysmith";
  source.save_document_to_path(v1_path.string());

  const nlohmann::json v0 = polysmith::protocol::to_payload(document, true);
  const auto v0_path = write_temp("format_v0.polysmith", v0.dump(2));
  const auto trunc_path =
      write_temp("format_trunc.polysmith", v0.dump(2).substr(0, 40));
  const auto garbage_path = write_temp("format_garbage.polysmith", "not json");
  const nlohmann::json future = {{"format", "polysmith/99"}, {"document", v0}};
  const auto future_path =
      write_temp("format_future.polysmith", future.dump(2));

  // Test 1: v1 envelope round-trips.
  DocumentManager m1;
  m1.create_document();
  const DocumentState loaded1 = m1.load_document_from_path(v1_path.string());
  ok &= expect(!loaded1.feature_history.empty(),
               "v1 envelope loads with feature history");

  // Test 2: v0 (raw, no format field) migrates to v1 and loads.
  DocumentManager m0;
  m0.create_document();
  const DocumentState loaded0 = m0.load_document_from_path(v0_path.string());
  ok &= expect(!loaded0.feature_history.empty(),
               "v0 payload migrates and loads");

  // Test 3: the migrated document still evaluates (compiles a body).
  const auto compiled = polysmith::core::compile_bodies(loaded0, false);
  ok &= expect(!compiled.bodies.empty(),
               "migrated document compiles a body");

  // Test 4: truncated JSON errors (does not crash).
  DocumentManager m2;
  m2.create_document();
  bool truncated_error = false;
  try {
    m2.load_document_from_path(trunc_path.string());
  } catch (const std::exception&) {
    truncated_error = true;
  }
  ok &= expect(truncated_error, "truncated file errors cleanly");

  // Test 5: garbage input errors (does not crash).
  DocumentManager m3;
  m3.create_document();
  bool garbage_error = false;
  try {
    m3.load_document_from_path(garbage_path.string());
  } catch (const std::exception&) {
    garbage_error = true;
  }
  ok &= expect(garbage_error, "garbage file errors cleanly");

  // Test 6: a newer format version is rejected (not silently misread).
  DocumentManager m4;
  m4.create_document();
  bool future_error = false;
  try {
    m4.load_document_from_path(future_path.string());
  } catch (const std::exception&) {
    future_error = true;
  }
  ok &= expect(future_error, "future format version is rejected");

  // Test 7: the recovery journal records + truncates.
  ok &= test_journal();

  std::cout << (ok ? "format_version_test passed"
                   : "format_version_test FAILED")
            << "\n";
  return ok ? 0 : 1;
}
