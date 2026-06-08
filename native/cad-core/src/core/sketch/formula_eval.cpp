#include "core/sketch/formula_eval.h"

#include "core/document/parameter.h"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace polysmith::core {
namespace {

#include "core/sketch/impl/formula_token_types.inc"
#include "core/sketch/impl/formula_tokenizer.inc"
#include "core/sketch/impl/formula_parser.inc"

}  // namespace

#include "core/sketch/impl/formula_eval_api.inc"

}  // namespace polysmith::core
