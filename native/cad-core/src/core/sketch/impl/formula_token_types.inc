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
