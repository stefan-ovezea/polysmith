// Machine library test.
//
// Covers the machine-definition library end to end: built-in seeding
// (idempotent, user files win), save/load round trip, validation,
// directory override via POLYSMITH_MACHINES_DIR, and that the built-in
// seeds parse into well-formed definitions.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/machine_library.h"

namespace {

using polysmith::core::MachineDefinition;
using polysmith::core::load_machine_library;
using polysmith::core::save_machine_definition;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

// Points POLYSMITH_MACHINES_DIR at `dir` (empty string = unset).
void set_machines_dir(const std::filesystem::path& dir) {
#ifdef _WIN32
  _putenv_s("POLYSMITH_MACHINES_DIR", dir.string().c_str());
#else
  if (dir.empty()) {
    unsetenv("POLYSMITH_MACHINES_DIR");
  } else {
    setenv("POLYSMITH_MACHINES_DIR", dir.string().c_str(), 1);
  }
#endif
}

const MachineDefinition* find_machine(
    const std::vector<MachineDefinition>& machines, const std::string& name) {
  for (const auto& machine : machines) {
    if (machine.name == name) {
      return &machine;
    }
  }
  return nullptr;
}

MachineDefinition make_test_laser() {
  MachineDefinition machine;
  machine.name = "Test Laser";
  machine.machine_type = "laser";
  machine.post_processor.type = "marlin";
  machine.work_area_x_mm = 300.0;
  machine.work_area_y_mm = 200.0;
  machine.pointer_offset_x_mm = 5.5;
  machine.pointer_offset_y_mm = -6.5;
  return machine;
}

std::filesystem::path make_temp_dir() {
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_machines_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

bool test_seed_idempotent_and_respects_user_files() {
  const auto dir = make_temp_dir();
  set_machines_dir(dir);

  // First load seeds the built-ins.
  const auto first = load_machine_library();
  if (!expect(find_machine(first, "GRBL Laser") != nullptr,
              "seed: built-in lasers present after first load")) {
    return false;
  }
  if (!expect(std::filesystem::exists(dir / "grbl-laser.json"),
              "seed: grbl-laser.json written to the directory")) {
    return false;
  }

  // A user edit overrides the built-in with the same name…
  {
    std::ofstream stream(dir / "grbl-laser.json", std::ios::trunc);
    stream << R"({
      "name": "GRBL Laser",
      "machine_type": "laser",
      "post_processor": { "type": "grbl", "filename": "" },
      "work_area_x_mm": 1234.0,
      "work_area_y_mm": 567.0,
      "pointer_offset_x_mm": 0.0,
      "pointer_offset_y_mm": 0.0
    })";
  }
  const auto edited = load_machine_library();
  const auto* grbl = find_machine(edited, "GRBL Laser");
  if (!expect(grbl != nullptr && grbl->work_area_x_mm == 1234.0,
              "user file overrides the built-in")) {
    return false;
  }

  // …and the seed never clobbers it on later loads.
  const auto again = load_machine_library();
  const auto* grbl_again = find_machine(again, "GRBL Laser");
  return expect(grbl_again != nullptr && grbl_again->work_area_x_mm == 1234.0,
                "seed: user edit survives repeated loads");
}

bool test_save_load_round_trip() {
  const auto dir = make_temp_dir();
  set_machines_dir(dir);

  std::string error;
  const std::string slug = save_machine_definition(make_test_laser(), error);
  if (!expect(error.empty(), "save: no error") ||
      !expect(slug == "test-laser", "save: name slugged to test-laser")) {
    return false;
  }
  if (!expect(std::filesystem::exists(dir / "test-laser.json"),
              "save: file written to the machines directory")) {
    return false;
  }

  const auto machines = load_machine_library();
  const auto* saved = find_machine(machines, "Test Laser");
  if (!expect(saved != nullptr, "load: saved machine listed")) {
    return false;
  }
  bool ok = expect(saved->machine_type == "laser", "load: machine_type") &&
            expect(saved->post_processor.type == "marlin", "load: post type") &&
            expect(saved->work_area_x_mm == 300.0, "load: work area x") &&
            expect(saved->work_area_y_mm == 200.0, "load: work area y") &&
            expect(saved->pointer_offset_x_mm == 5.5, "load: pointer offset x") &&
            expect(saved->pointer_offset_y_mm == -6.5, "load: pointer offset y");

  // Re-saving updates the file in place (save = user intent).
  MachineDefinition updated = make_test_laser();
  updated.work_area_x_mm = 999.0;
  std::string second_error;
  save_machine_definition(updated, second_error);
  const auto after = load_machine_library();
  const auto* re_saved = find_machine(after, "Test Laser");
  ok = expect(second_error.empty() && re_saved != nullptr &&
                  re_saved->work_area_x_mm == 999.0,
              "save: re-save overwrites the file") &&
       ok;
  return ok;
}

bool test_save_rejects_invalid() {
  const auto dir = make_temp_dir();
  set_machines_dir(dir);
  load_machine_library();  // seed first so the count check is exact

  const auto file_count = [&dir]() {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      (void)entry;
      ++count;
    }
    return count;
  };
  const int before = file_count();

  MachineDefinition empty_name = make_test_laser();
  empty_name.name = "";
  std::string error;
  save_machine_definition(empty_name, error);
  if (!expect(!error.empty(), "validation: empty name rejected")) {
    return false;
  }

  MachineDefinition bad_type = make_test_laser();
  bad_type.machine_type = "disco_ball";
  error.clear();
  save_machine_definition(bad_type, error);
  if (!expect(!error.empty(), "validation: unknown machine type rejected")) {
    return false;
  }

  MachineDefinition zero_area = make_test_laser();
  zero_area.work_area_x_mm = 0.0;
  error.clear();
  save_machine_definition(zero_area, error);
  if (!expect(!error.empty(), "validation: laser with zero work area rejected")) {
    return false;
  }

  // Mills carry no work-area requirement.
  MachineDefinition mill;
  mill.name = "Tiny Mill";
  mill.machine_type = "3_axis_mill";
  error.clear();
  save_machine_definition(mill, error);
  if (!expect(error.empty(), "validation: mill without work area accepted")) {
    return false;
  }

  return expect(file_count() == before + 1,
                "validation: only the valid machine was written");
}

bool test_directory_env_override() {
  const auto dir_a = make_temp_dir();
  const auto dir_b = std::filesystem::temp_directory_path() /
                     "polysmith_machines_test_b";
  std::filesystem::remove_all(dir_b);
  std::filesystem::create_directories(dir_b);

  set_machines_dir(dir_a);
  std::string error;
  save_machine_definition(make_test_laser(), error);
  if (!expect(error.empty(), "override: saved into dir A")) {
    return false;
  }

  // Switching the env var switches the library — dir B is seeded with
  // the built-ins but has no user machines.
  set_machines_dir(dir_b);
  const auto machines = load_machine_library();
  const bool absent = find_machine(machines, "Test Laser") == nullptr;
  const bool builtins_present = find_machine(machines, "GRBL Laser") != nullptr;
  return expect(absent, "override: user machine from dir A not listed") &&
         expect(builtins_present, "override: dir B seeded with built-ins");
}

bool test_builtin_seeds_parse() {
  // No directory configured — the built-ins come from the compiled-in
  // JSON and must still list and be well-formed.
  set_machines_dir({});
  const auto machines = load_machine_library();
  const auto* grbl = find_machine(machines, "GRBL Laser");
  const auto* smoothie = find_machine(machines, "Smoothieware Laser");
  const auto* mill = find_machine(machines, "Generic 3-Axis Mill");
  if (!expect(grbl != nullptr && smoothie != nullptr && mill != nullptr,
              "seeds: all three built-ins list without a directory")) {
    return false;
  }
  return expect(grbl->machine_type == "laser" &&
                    grbl->post_processor.type == "grbl" &&
                    grbl->work_area_x_mm > 0.0 &&
                    smoothie->post_processor.type == "smoothieware" &&
                    mill->machine_type == "3_axis_mill",
                "seeds: built-in fields are well-formed");
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_seed_idempotent_and_respects_user_files() && ok;
  ok = test_save_load_round_trip() && ok;
  ok = test_save_rejects_invalid() && ok;
  ok = test_directory_env_override() && ok;
  ok = test_builtin_seeds_parse() && ok;
  // Leave the env clean for whatever runs after the test process.
  set_machines_dir({});
  if (ok) {
    std::cout << "cam_machine_library_test: all tests passed\n";
    return 0;
  }
  std::cerr << "cam_machine_library_test: FAILURES\n";
  return 1;
}
