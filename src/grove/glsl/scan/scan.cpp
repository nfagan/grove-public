#include "scan.hpp"
#include "keyword.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct SourceIterator {
  SourceIterator(const char* source, int size) :
  source(source), size(size), index(0) {
    //
  }

  bool has_next() const {
    return index < size;
  }

  char peek() {
    return index >= size ? '\0' : source[index];
  }

  void advance() {
    index++;
  }

  void advance_to_new_line() {
    while (has_next() && peek() != '\n') {
      advance();
    }
  }

  const char* current() const {
    return source + index;
  }

  const char* source;
  int size;
  int index;
};

glsl::Token make_token(glsl::TokenType type, const char* begin, int len) {
  std::string_view lex{begin, std::size_t(len)};
  return {type, lex};
}

glsl::Token make_token(glsl::TokenType type, const SourceIterator& it, int len) {
  return make_token(type, it.source + it.index, len);
}

bool is_alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

void process_identifier(SourceIterator& it, glsl::ScanResult& result) {
  int len = 0;
  const char* begin = it.current();

  while (it.has_next() && is_alpha(it.peek())) {
    it.advance();
    len++;
  }

  auto tok = make_token(glsl::TokenType::Identifier, begin, len);
  const auto maybe_kw = glsl::maybe_keyword_token_type(tok.lexeme);

  if (maybe_kw != glsl::TokenType::Null) {
    tok.type = maybe_kw;
  }

  result.tokens.push_back(tok);
}

void process_string_literal(SourceIterator& it, glsl::ScanResult& result) {
  it.advance(); //  consume '"'
  int len = 0;
  const char* begin = it.current();

  while (it.has_next() && it.peek() != '"') {
    it.advance();
    len++;
  }

  if (it.peek() != '"') {
    result.maybe_error = glsl::ScanError::UnterminatedStringLiteral;
    return;
  } else {
    it.advance();
  }

  result.tokens.push_back(make_token(glsl::TokenType::StringLiteral, begin, len));
}

void maybe_process_comment(SourceIterator& it, glsl::ScanResult&) {
  it.advance();
  auto next = it.peek();
  if (next == '/') {
    it.advance_to_new_line();
  }
}

} //  anon

/*
 * ScanResult
 */

glsl::ScanResult::ScanResult() :
maybe_error(ScanError::None) {
  //
}

bool glsl::ScanResult::success() const {
  return maybe_error == ScanError::None;
}

/*
 * Scan
 */

glsl::ScanResult glsl::scan(const char* source, int size) {
  SourceIterator it(source, size);
  glsl::ScanResult result;

  while (it.has_next() && result.maybe_error == ScanError::None) {
    auto c = it.peek();

    switch (c) {
      case '#':
        result.tokens.push_back(make_token(TokenType::Pound, it, 1));
        it.advance();
        break;
      case '/':
        maybe_process_comment(it, result);
        break;
      case '\n':
        result.tokens.push_back(make_token(TokenType::NewLine, it, 1));
        it.advance();
        break;
      case '"':
        process_string_literal(it, result);
        break;
      default:
        if (is_alpha(c)) {
          process_identifier(it, result);
        } else {
          it.advance();
        }
    }
  }

  return result;
}

GROVE_NAMESPACE_END
