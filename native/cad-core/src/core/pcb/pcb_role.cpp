#include "core/pcb/pcb_role.h"

#include <algorithm>
#include <cctype>

namespace polysmith::core::pcb {
namespace {

std::string lower(const std::string& text) {
  std::string copy = text;
  std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return copy;
}

bool has(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

/// Bottom-side marker.  Covers both tiers' spellings: ",bot" (KiCad's Gerber
/// attribute), "b_cu"/"b_mask"/"b_paste"/"b_silkscreen" (filenames) and
/// ",l4" (job-file copper layer numbering on a 4-layer board).
bool looks_bottom(const std::string& text) {
  return has(text, "bot") || has(text, "b_cu") || has(text, "b_mask") ||
         has(text, "b_paste") || has(text, "b_silk") || has(text, ",l4");
}

bool looks_top(const std::string& text) {
  return has(text, "top") || has(text, "f_cu") || has(text, "f_mask") ||
         has(text, "f_paste") || has(text, "f_silk") || has(text, ",l1");
}

PcbRole classify(const std::string& text) {
  // Drills first: "NonPlated" contains "Plated".
  if (has(text, "nonplated") || has(text, "npth")) {
    return PcbRole::DrillNonPlated;
  }
  if (has(text, "plated") || has(text, "pth") || has(text, ".drl")) {
    return PcbRole::DrillPlated;
  }
  if (has(text, "soldermask") || has(text, "mask")) {
    return looks_bottom(text) ? PcbRole::SolderMaskBottom : PcbRole::SolderMaskTop;
  }
  // "SolderPaste" (job file) and "Paste" (Gerber attribute) are the same layer.
  if (has(text, "solderpaste") || has(text, "paste")) {
    return looks_bottom(text) ? PcbRole::PasteBottom : PcbRole::PasteTop;
  }
  if (has(text, "silkscreen") || has(text, "silk") || has(text, "legend")) {
    return looks_bottom(text) ? PcbRole::SilkscreenBottom : PcbRole::SilkscreenTop;
  }
  if (has(text, "copper") || has(text, "_cu")) {
    if (looks_bottom(text)) {
      return PcbRole::CopperBottom;
    }
    if (looks_top(text)) {
      return PcbRole::CopperTop;
    }
    // An inner layer: KiCad's job file says ",L2,Inr", the filename "in1_cu".
    return PcbRole::CopperInner;
  }
  // "Profile" (attribute) and "edge_cuts" (filename) are the board outline;
  // check it after copper so "Edge_Cuts" cannot shadow anything.
  if (has(text, "profile") || has(text, "edge") || has(text, "outline")) {
    return PcbRole::Profile;
  }
  return PcbRole::Unknown;
}

}  // namespace

PcbRoleResolution resolve_pcb_role(const std::string& file_function,
                                   const std::string& file_name) {
  PcbRoleResolution resolution;
  const std::string attribute = lower(file_function);
  if (!attribute.empty()) {
    resolution.role = classify(attribute);
    if (resolution.role != PcbRole::Unknown) {
      resolution.from_attributes = true;
      resolution.note = "TF.FileFunction," + file_function;
      return resolution;
    }
  }
  const std::string name = lower(file_name);
  resolution.role = classify(name);
  if (resolution.role != PcbRole::Unknown) {
    resolution.note = "filename " + file_name;
    return resolution;
  }
  resolution.note = attribute.empty() ? "no role attribute and no name match"
                                      : "unrecognised role attribute: " + file_function;
  return resolution;
}

}  // namespace polysmith::core::pcb

namespace polysmith::core {

const char* pcb_role_id(PcbRole role) {
  switch (role) {
    case PcbRole::CopperTop:
      return "copper_top";
    case PcbRole::CopperInner:
      return "copper_inner";
    case PcbRole::CopperBottom:
      return "copper_bottom";
    case PcbRole::SolderMaskTop:
      return "solder_mask_top";
    case PcbRole::SolderMaskBottom:
      return "solder_mask_bottom";
    case PcbRole::PasteTop:
      return "paste_top";
    case PcbRole::PasteBottom:
      return "paste_bottom";
    case PcbRole::SilkscreenTop:
      return "silkscreen_top";
    case PcbRole::SilkscreenBottom:
      return "silkscreen_bottom";
    case PcbRole::Profile:
      return "profile";
    case PcbRole::DrillPlated:
      return "drill_plated";
    case PcbRole::DrillNonPlated:
      return "drill_non_plated";
    case PcbRole::Unknown:
      break;
  }
  return "unknown";
}

PcbRole parse_pcb_role(const std::string& id) {
  static const PcbRole kRoles[] = {
      PcbRole::CopperTop,        PcbRole::CopperInner,     PcbRole::CopperBottom,
      PcbRole::SolderMaskTop,    PcbRole::SolderMaskBottom, PcbRole::PasteTop,
      PcbRole::PasteBottom,      PcbRole::SilkscreenTop,   PcbRole::SilkscreenBottom,
      PcbRole::Profile,          PcbRole::DrillPlated,     PcbRole::DrillNonPlated,
  };
  for (const PcbRole role : kRoles) {
    if (id == pcb_role_id(role)) {
      return role;
    }
  }
  return PcbRole::Unknown;
}

bool pcb_role_is_drill(PcbRole role) {
  return role == PcbRole::DrillPlated || role == PcbRole::DrillNonPlated;
}

bool pcb_role_is_bottom(PcbRole role) {
  switch (role) {
    case PcbRole::CopperBottom:
    case PcbRole::SolderMaskBottom:
    case PcbRole::PasteBottom:
    case PcbRole::SilkscreenBottom:
      return true;
    default:
      return false;
  }
}

const char* pcb_role_label(PcbRole role) {
  switch (role) {
    case PcbRole::CopperTop:
      return "Top copper";
    case PcbRole::CopperInner:
      return "Inner copper";
    case PcbRole::CopperBottom:
      return "Bottom copper";
    case PcbRole::SolderMaskTop:
      return "Top solder mask";
    case PcbRole::SolderMaskBottom:
      return "Bottom solder mask";
    case PcbRole::PasteTop:
      return "Top paste";
    case PcbRole::PasteBottom:
      return "Bottom paste";
    case PcbRole::SilkscreenTop:
      return "Top silkscreen";
    case PcbRole::SilkscreenBottom:
      return "Bottom silkscreen";
    case PcbRole::Profile:
      return "Board outline";
    case PcbRole::DrillPlated:
      return "Plated drills";
    case PcbRole::DrillNonPlated:
      return "Non-plated drills";
    case PcbRole::Unknown:
      break;
  }
  return "Unclassified layer";
}

}  // namespace polysmith::core
