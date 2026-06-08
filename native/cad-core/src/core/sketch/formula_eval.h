#pragma once

#include <functional>
#include <string>

namespace polysmith::core {

// Evaluates a simple arithmetic expression. Supports:
//   - numbers: 0-9, decimal points
//   - operators: + - * /
//   - parentheses: ( )
//   - unary minus: -expr
//   - parameter references: any [a-zA-Z_][a-zA-Z0-9_]* name
//
// The `resolver` callback is called for parameter names to look up their
// resolved values. Throws std::runtime_error on syntax errors, unknown
// names, or division by zero.
//
// Cycle detection: the caller should manage a set of names currently
// being resolved and pass a resolver that checks it.
double evaluate_formula(
    const std::string& expression,
    const std::function<double(const std::string&)>& resolver);

// Re-evaluate all parameters until fixpoint. Returns true if any
// parameter changed value or error state.
bool reify_parameters(
    std::vector<struct ParameterEntry>& params,
    int max_passes = 50);

}  // namespace polysmith::core
