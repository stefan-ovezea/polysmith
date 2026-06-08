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
