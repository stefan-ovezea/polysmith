#include "core/cam/cam_generator.h"

#include <cassert>
#include <unordered_map>

#include "core/cam/face_milling.h"
#include "core/cam/laser_cut.h"

namespace polysmith::core {

namespace {

std::unordered_map<std::string, CamGenerator>& registry() {
  static std::unordered_map<std::string, CamGenerator> generators;
  return generators;
}

}  // namespace

void register_cam_generator(CamGenerator generator) {
  assert(!generator.type.empty());
  assert(generator.generate != nullptr);
  // Duplicate registrations are a programming error, not user input.
  const auto [it, inserted] =
      registry().emplace(generator.type, std::move(generator));
  (void)it;
  (void)inserted;
  assert(inserted);
}

const CamGenerator* find_cam_generator(const std::string& type) {
  const auto found = registry().find(type);
  if (found == registry().end()) {
    return nullptr;
  }
  return &found->second;
}

void register_builtin_cam_generators() {
  // Called once from CadCoreApp::run() after OCCT initialization.
  register_laser_cut_generator();
  register_face_milling_generator();
}

}  // namespace polysmith::core
