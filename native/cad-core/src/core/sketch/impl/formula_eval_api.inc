double evaluate_formula(
    const std::string& expression,
    const std::function<double(const std::string&)>& resolver) {
  if (expression.empty()) {
    throw std::runtime_error("Empty expression");
  }
  Parser parser(expression, resolver);
  return parser.parse();
}

bool reify_parameters(std::vector<ParameterEntry>& params, int max_passes) {
  bool any_change = false;

  for (int pass = 0; pass < max_passes; ++pass) {
    bool changed = false;
    std::unordered_set<std::string> resolving;

    for (auto& p : params) {
      auto resolver = [&](const std::string& name) -> double {
        if (resolving.count(name)) {
          throw std::runtime_error("Cycle detected: '" + name + "'");
        }
        resolving.insert(name);

        double value = 0;
        bool found = false;
        for (const auto& other : params) {
          if (other.name == name) {
            if (other.has_error) {
              resolving.erase(name);
              throw std::runtime_error(
                  "Parameter '" + name +
                  "' has an unresolved expression");
            }
            value = other.resolved_value;
            found = true;
            break;
          }
        }

        resolving.erase(name);

        if (!found) {
          throw std::runtime_error("Unknown parameter: '" + name + "'");
        }
        return value;
      };

      try {
        double new_value = evaluate_formula(p.expression, resolver);
        if (p.has_error || std::fabs(new_value - p.resolved_value) > 1e-9) {
          p.resolved_value = new_value;
          p.has_error = false;
          p.error_message.clear();
          changed = true;
        }
      } catch (const std::exception& e) {
        if (!p.has_error || p.error_message != e.what()) {
          p.has_error = true;
          p.error_message = e.what();
          changed = true;
        }
      }
    }

    if (!changed) {
      break;
    }
    any_change = true;
  }

  return any_change;
}
