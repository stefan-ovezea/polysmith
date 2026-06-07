#include "core/formula_eval.h"
#include "core/parameter.h"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace polysmith::core {
namespace {

enum class TokenKind {
  Number,
  Name,
  Plus,
  Minus,
  Star,
  Slash,
  LParen,
  RParen,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  double number_value = 0;
  std::string name_value;
};

class Tokenizer {
 public:
  explicit Tokenizer(const std::string& input) : input_(input), pos_(0) {}

  Token next() {
    skip_whitespace();
    if (pos_ >= input_.size()) {
      return Token{TokenKind::End};
    }

    char c = input_[pos_];

    if (std::isdigit(c) || c == '.') {
      return read_number();
    }
    if (std::isalpha(c) || c == '_') {
      return read_name();
    }

    ++pos_;
    switch (c) {
      case '+':
        return Token{TokenKind::Plus};
      case '-':
        return Token{TokenKind::Minus};
      case '*':
        return Token{TokenKind::Star};
      case '/':
        return Token{TokenKind::Slash};
      case '(':
        return Token{TokenKind::LParen};
      case ')':
        return Token{TokenKind::RParen};
      default:
        throw std::runtime_error(
            std::string("Unexpected character: '") + c + "'");
    }
  }

  // Peek without consuming.
  Token peek() {
    auto saved = pos_;
    auto tok = next();
    pos_ = saved;
    return tok;
  }

 private:
  void skip_whitespace() {
    while (pos_ < input_.size() && std::isspace(input_[pos_])) {
      ++pos_;
    }
  }

  Token read_number() {
    std::string num;
    while (pos_ < input_.size() &&
           (std::isdigit(input_[pos_]) || input_[pos_] == '.')) {
      num += input_[pos_];
      ++pos_;
    }
    return Token{TokenKind::Number, std::stod(num)};
  }

  Token read_name() {
    std::string name;
    while (pos_ < input_.size() &&
           (std::isalnum(input_[pos_]) || input_[pos_] == '_')) {
      name += input_[pos_];
      ++pos_;
    }
    return Token{TokenKind::Name, 0, std::move(name)};
  }

  const std::string& input_;
  std::size_t pos_;
};

class Parser {
 public:
  Parser(const std::string& expression,
         const std::function<double(const std::string&)>& resolver)
      : tokenizer_(expression), resolver_(resolver) {
    current_ = tokenizer_.next();
  }

  double parse() {
    double result = expression();
    if (current_.kind != TokenKind::End) {
      throw std::runtime_error("Unexpected token after expression");
    }
    return result;
  }

 private:
  void advance() { current_ = tokenizer_.next(); }

  Token expect(TokenKind kind, const char* label) {
    if (current_.kind != kind) {
      throw std::runtime_error(std::string("Expected ") + label);
    }
    Token tok = current_;
    advance();
    return tok;
  }

  // expression = term (("+" | "-") term)*
  double expression() {
    double left = term();
    while (current_.kind == TokenKind::Plus ||
           current_.kind == TokenKind::Minus) {
      TokenKind op = current_.kind;
      advance();
      double right = term();
      if (op == TokenKind::Plus) {
        left = left + right;
      } else {
        left = left - right;
      }
    }
    return left;
  }

  // term = factor (("*" | "/") factor)*
  double term() {
    double left = factor();
    while (current_.kind == TokenKind::Star ||
           current_.kind == TokenKind::Slash) {
      TokenKind op = current_.kind;
      advance();
      double right = factor();
      if (op == TokenKind::Star) {
        left = left * right;
      } else {
        if (right == 0.0) {
          throw std::runtime_error("Division by zero");
        }
        left = left / right;
      }
    }
    return left;
  }

  // factor = NUMBER | PARAM_NAME | "-" factor | "(" expression ")"
  double factor() {
    if (current_.kind == TokenKind::Minus) {
      advance();
      return -factor();
    }
    if (current_.kind == TokenKind::Number) {
      double value = current_.number_value;
      advance();
      return value;
    }
    if (current_.kind == TokenKind::Name) {
      std::string name = current_.name_value;
      advance();
      return resolver_(name);
    }
    if (current_.kind == TokenKind::LParen) {
      advance();
      double value = expression();
      expect(TokenKind::RParen, "')'");
      return value;
    }
    throw std::runtime_error("Unexpected token in expression");
  }

  Tokenizer tokenizer_;
  const std::function<double(const std::string&)>& resolver_;
  Token current_;
};

}  // namespace

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

}  // namespace polysmith::core
