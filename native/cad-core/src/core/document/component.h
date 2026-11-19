#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

// One node of an imported file's product structure.
//
// A STEP assembly names each PLACEMENT of a part, not just the part: the same
// footprint appears once as "U1:1" and again as "U2:1", and the file's names
// for those occurrences are the only thing that tells two identical bodies
// apart in the tree. Storing them keeps the Browser showing what the file
// actually contains — a hierarchy of components — instead of a thousand-row
// list of bodies whose names all read the same.
//
// A component is a grouping, not a body: it owns no geometry of its own. Its
// members are the body features this occurrence is made of (`body_ids`, a body
// id being its feature id). Nesting is by `parent_id`, so the tree is a flat
// list the UI can assemble in one pass.
//
// This is deliberately the *import provenance* shape of an assembly — names and
// membership, no transforms or mates. A real assembly model (component
// instances the user positions and constrains with the vendored OndselSolver)
// is a different thing and must not be constrained by it.
struct ComponentNode {
  std::string id;         // "component-N"
  std::string name;       // occurrence name from the file, e.g. "U1:1"
  std::string parent_id;  // empty = top level (a direct child of the import)
  std::vector<std::string> body_ids;
};

}  // namespace polysmith::core
