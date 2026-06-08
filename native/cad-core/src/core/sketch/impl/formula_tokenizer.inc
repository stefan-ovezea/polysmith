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
