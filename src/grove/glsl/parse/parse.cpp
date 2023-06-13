#include "parse.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct TokenIterator {
  TokenIterator(const std::vector<glsl::Token>& tokens) :
    tokens(&tokens),
    index(0) {
    //
  }

  bool has_next() const {
    return index < tokens->size();
  }

  void advance() {
    index++;
  }

  bool consume(glsl::TokenType type) {
    const bool matches = peek().type == type;
    if (matches) {
      advance();
    }
    return matches;
  }

  void consume_through_newline() {
    while (has_next() && peek().type != glsl::TokenType::NewLine) {
      advance();
    }

    if (peek().type == glsl::TokenType::NewLine) {
      advance();
    }
  }

  glsl::Token peek() const {
    return index >= tokens->size() ? glsl::Token::null_token : (*tokens)[index];
  }

  const std::vector<glsl::Token>* tokens;
  std::size_t index;
};

/*
 * Parse routines
 */

glsl::BoxedAstNode compiler_pragma(const glsl::Token& pound_tok,
                                   TokenIterator& it,
                                   glsl::ParseResult& result) {
  it.advance();

  if (!it.consume(glsl::TokenType::KeywordInclude)) {
    result.maybe_error = glsl::ParseError::UnexpectedTokenType;
    return nullptr;
  }

  auto lex = it.peek();
  if (lex.type != glsl::TokenType::StringLiteral) {
    result.maybe_error = glsl::ParseError::UnexpectedTokenType;
    return nullptr;
  }

  it.advance();

  auto type = glsl::CompilerDirective::Type::Include;
  auto begin = pound_tok.lexeme.data();
  auto end = lex.lexeme.data() + lex.lexeme.size() + 1;  //  +1 for "

  auto node =
    std::make_unique<glsl::CompilerDirective>(lex, type, begin, end);

  return node;
}

glsl::BoxedAstNode compiler_directive(TokenIterator& it, glsl::ParseResult& result) {
  auto pound_tok = it.peek();
  it.advance(); // consume #

  switch (it.peek().type) {
    case glsl::TokenType::KeywordPragma:
      return compiler_pragma(pound_tok, it, result);
    default:
      it.consume_through_newline();
      return nullptr;
  }
}

} //  anon

glsl::ParseResult::ParseResult() :
  maybe_error(ParseError::None) {
  //
}

bool glsl::ParseResult::success() const {
  return maybe_error == ParseError::None;
}

glsl::ParseResult glsl::parse(const std::vector<glsl::Token>& tokens) {
  TokenIterator it(tokens);
  glsl::ParseResult result;

  while (it.has_next() && result.maybe_error == ParseError::None) {
    BoxedAstNode maybe_node;

    const auto tok = it.peek();
    switch (tok.type) {
      case TokenType::Pound:
        maybe_node = compiler_directive(it, result);
      default:
        it.advance();
    }

    if (maybe_node) {
      result.nodes.push_back(std::move(maybe_node));
    }
  }

  return result;
}

GROVE_NAMESPACE_END
