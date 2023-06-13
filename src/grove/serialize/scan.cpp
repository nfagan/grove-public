#include "scan.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Either.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using MaybeToken = Either<io::Token, io::ParseError>;

MaybeToken ok_maybe_token(const io::Token& token) {
  return either::make_left<MaybeToken>(token);
}

MaybeToken err_maybe_token(io::ParseError&& err) {
  return either::make_right<MaybeToken>(std::move(err));
}

io::Lexeme make_lexeme(const char* beg, const char* end) {
  return std::string_view{beg, std::size_t(end - beg)};
}

io::Token make_token(io::TokenType type, io::Lexeme lexeme) {
  return {type, lexeme};
}

void append_result(io::ScanResult& result, MaybeToken&& tok_res) {
  if (tok_res) {
    result.tokens.push_back(tok_res.get_left());
  } else {
    result.errors.push_back(tok_res.get_right());
  }
}

Optional<io::TokenType> find_keyword(io::Lexeme lex) {
  using Res = Optional<io::TokenType>;
  if (lex == "ref") {
    return Res(io::TokenType::KeywordRef);
  } else if (lex == "new") {
    return Res(io::TokenType::KeywordNew);
  } else {
    return NullOpt{};
  }
}

inline bool is_alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

inline bool is_alpha_numeric(char c) {
  return is_alpha(c) || is_digit(c);
}

struct TextIterator {
  TextIterator(const char* text, std::size_t size) : text{text}, size{size} {
    //
  }

  bool has_next() const {
    return index < size;
  }

  char peek() const {
    return index >= size ? '\0' : text[index];
  }

  char peek_nth(unsigned i) const {
    return index + i >= size ? '\0' : text[index + i];
  }

  void advance() {
    index++;
  }

  void advance(int num) {
    index += num;
  }

  const char* curr() const {
    return text + index;
  }

  const char* text;
  std::size_t index{};
  std::size_t size;
};

io::Token number(TextIterator& it) {
  auto beg = it.curr();
  while (it.has_next() && (is_digit(it.peek()) || it.peek() == '.' || it.peek() == '-')) {
    it.advance();
  }
  auto end = it.curr();
  return make_token(io::TokenType::Number, make_lexeme(beg, end));
}

MaybeToken ident_or_keyword(TextIterator& it) {
  auto beg = it.curr();
  while (it.has_next() && (is_alpha_numeric(it.peek()) || it.peek() == '_')) {
    it.advance();
  }
  auto end = it.curr();
  auto lex = make_lexeme(beg, end);
  if (auto kw = find_keyword(lex)) {
    return ok_maybe_token(make_token(kw.value(), lex));
  } else {
    return ok_maybe_token(make_token(io::TokenType::Identifier, lex));
  }
}

MaybeToken string_literal(TextIterator& it) {
  const auto apos = '\'';
  auto beg = it.curr() + 1;
  it.advance();
  while (it.has_next() && it.peek() != apos) {
    it.advance();
  }
  if (it.peek() != apos) {
    return err_maybe_token(io::ParseError{"Unterminated string literal."});
  } else {
    auto end = it.curr();
    it.advance();
    return ok_maybe_token(make_token(io::TokenType::String, make_lexeme(beg, end)));
  }
}

Optional<io::Token> punct(TextIterator& it) {
  const auto c = it.peek();
  auto beg = it.curr();

  const auto [tok_type, num_advance] = [c]() -> std::pair<io::TokenType, int> {
    switch (c) {
      case ':':
        return {io::TokenType::Colon, 1};
      case ',':
        return {io::TokenType::Comma, 1};
      case '\'':
        return {io::TokenType::Apostrophe, 1};
      case '.':
        return {io::TokenType::Period, 1};
      case '[':
        return {io::TokenType::LeftBracket, 1};
      case ']':
        return {io::TokenType::RightBracket, 1};
      case '{':
        return {io::TokenType::LeftBrace, 1};
      case '}':
        return {io::TokenType::RightBrace, 1};
      default:
        return {io::TokenType::Null, 0};
    }
  }();

  if (tok_type == io::TokenType::Null) {
    return NullOpt{};

  } else {
    it.advance(num_advance);
    auto end = it.curr();
    return Optional<io::Token>(make_token(tok_type, make_lexeme(beg, end)));
  }
}

} //  anon

io::ScanResult io::scan(const char* text, std::size_t size) {
  io::ScanResult result;
  TextIterator it{text, size};

  while (it.has_next()) {
    auto c = it.peek();
    if (is_digit(c) || c == '-') {
      result.tokens.push_back(number(it));

    } else if (is_alpha(c)) {
      append_result(result, ident_or_keyword(it));

    } else if (c == '\'') {
      append_result(result, string_literal(it));

    } else if (auto punct_res = punct(it)) {
      result.tokens.push_back(punct_res.value());

    } else {
      it.advance();
    }
  }

  result.success = result.errors.empty();
  return result;
}

GROVE_NAMESPACE_END
