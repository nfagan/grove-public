#include "parse.hpp"
#include "ast.hpp"
#include "StringRegistry.hpp"
#include "utility.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Either.hpp"
#include <memory>
#include <string>
#include <cstdlib>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace io::ast;
using namespace io;

ParseError make_error_unexpected_type(const Token& tok) {
  auto msg = std::string{"Unexpected `"} + to_string(tok.type) + "`.";
  return ParseError{tok, std::move(msg)};
}

ParseError make_error_expected_type(const Token& tok, TokenType expected, TokenType received) {
  auto msg = std::string{"Expected `"} +
             to_string(expected) + "`, received: `" + to_string(received) + "`.";
  return ParseError{tok, std::move(msg)};
}

ParseError make_error_failed_to_parse_reference_identifier(const Token& tok) {
  return ParseError{tok, "Failed to parse reference identifier."};
}

ParseError make_error_failed_to_parse_double(const Token& tok) {
  return ParseError{tok, "Failed to parse double."};
}

ParseError make_error_duplicate_field_name(const Token& tok) {
  return ParseError{tok, "Duplicate field name."};
}

template <typename T>
using MaybeNode = Either<std::unique_ptr<T>, ParseError>;

template <typename T>
MaybeNode<T> ok_node(std::unique_ptr<T> v) {
  return either::make_left<MaybeNode<T>>(std::move(v));
}

template <typename T>
MaybeNode<T> err_node(ParseError&& err) {
  return either::make_right<MaybeNode<T>>(std::move(err));
}

template <typename T, typename U>
MaybeNode<U> transform_maybe_node(MaybeNode<T>&& orig) {
  if (orig) {
    return ok_node<U>(std::move(orig.get_left()));
  } else {
    return err_node<U>(std::move(orig.get_right()));
  }
}

template <typename T>
void push_result_or_error(ParseResult& res, MaybeNode<T>&& maybe_node) {
  if (maybe_node) {
    res.ast.nodes.push_back(std::move(maybe_node.get_left()));
  } else {
    res.errors.push_back(std::move(maybe_node.get_right()));
  }
}

Optional<ReferenceIdentifier> parse_reference_identifier(const Token& token) {
  if (auto res = parse_int64(token.lexeme)) {
    if (res.value() >= 0) {
      ReferenceIdentifier ident{uint64_t(res.value())};
      return Optional<ReferenceIdentifier>(ident);
    } else {
      return NullOpt{};
    }
  } else {
    return NullOpt{};
  }
}

struct TokenIterator {
  TokenIterator(const std::vector<Token>* tokens) : tokens{tokens} {
    //
  }

  const Token& peek() const {
    return peek_nth(0);
  }

  const Token& peek_nth(unsigned i) const {
    return index + i < tokens->size() ? (*tokens)[index + i] : Token::null();
  }

  bool has_next() const {
    return index < tokens->size();
  }

  void advance() {
    index++;
  }

  void advance_to(TokenType type) {
    while (has_next() && peek().type != type) {
      advance();
    }
  }

  bool consume(TokenType type) {
    if (peek().type == type) {
      advance();
      return true;
    } else {
      return false;
    }
  }

  std::size_t index{};
  const std::vector<Token>* tokens;
};

struct ParseInstance {
  ParseInstance(ParseInfo* info) : info{info} {
    //
  }

  ParseInfo* info;
};

Either<Token, ParseError> expect(const TokenIterator& it, TokenType expected) {
  auto src = it.peek();
  if (src.type == expected) {
    return either::left<Token, ParseError>(src);
  } else {
    return either::right<Token, ParseError>(
      make_error_expected_type(src, expected, src.type));
  }
}

Either<Token, ParseError> expect_consume(TokenIterator& it, TokenType expected) {
  auto src = it.peek();
  if (it.consume(expected)) {
    return either::left<Token, ParseError>(src);
  } else {
    return either::right<Token, ParseError>(
      make_error_expected_type(src, expected, src.type));
  }
}

MaybeNode<Node> value_node(TokenIterator& it, ParseInstance& instance);

MaybeNode<ObjectNode> object_node(TokenIterator& it, ParseInstance& instance) {
  auto source_tok = it.peek();
  it.advance();

  ObjectNode::Fields fields;

  while (it.has_next() && it.peek().type != io::TokenType::RightBrace) {
    auto field_tok = it.peek();
    auto ident_res = expect_consume(it, io::TokenType::Identifier);
    if (!ident_res) {
      return err_node<ObjectNode>(std::move(ident_res.get_right()));
    }
    auto colon_res = expect_consume(it, io::TokenType::Colon);
    if (!colon_res) {
      return err_node<ObjectNode>(std::move(colon_res.get_right()));
    }

    auto ident = instance.info->string_registry.emplace_view(ident_res.get_left().lexeme);
    if (fields.count(ident) > 0) {
      return err_node<ObjectNode>(make_error_duplicate_field_name(field_tok));
    }

    auto field_res = value_node(it, instance);
    if (!field_res) {
      return err_node<ObjectNode>(std::move(field_res.get_right()));
    }

    fields[ident] = std::move(field_res.get_left());
  }

  auto brace_res = expect_consume(it, io::TokenType::RightBrace);
  if (!brace_res) {
    return err_node<ObjectNode>(std::move(brace_res.get_right()));
  }

  return ok_node<ObjectNode>(
    std::make_unique<ObjectNode>(source_tok, std::move(fields)));
}

MaybeNode<ArrayNode> array_node(TokenIterator& it, ParseInstance& instance) {
  auto tok = it.peek();
  it.advance();

  std::vector<std::unique_ptr<Node>> elements;
  while (it.has_next() && it.peek().type != TokenType::RightBracket) {
    auto val_res = value_node(it, instance);
    if (!val_res) {
      return err_node<ArrayNode>(std::move(val_res.get_right()));
    } else {
      elements.push_back(std::move(val_res.get_left()));
    }
    if (it.peek().type != io::TokenType::RightBracket) {
      auto comma_res = expect_consume(it, TokenType::Comma);
      if (!comma_res) {
        return err_node<ArrayNode>(std::move(comma_res.get_right()));
      }
    }
  }

  auto bracket_res = expect_consume(it, TokenType::RightBracket);
  if (!bracket_res) {
    return err_node<ArrayNode>(std::move(bracket_res.get_right()));
  }

  return ok_node<ArrayNode>(
    std::make_unique<ArrayNode>(tok, std::move(elements)));
}

MaybeNode<StringNode> string_node(TokenIterator& it, ParseInstance& instance) {
  auto tok = it.peek();
  it.advance();
  auto str = instance.info->string_registry.emplace_view(tok.lexeme);
  return ok_node<StringNode>(std::make_unique<StringNode>(tok, str));
}

MaybeNode<NumberNode> number_node(TokenIterator& it, ParseInstance&) {
  auto tok = it.peek();
  it.advance();
  if (auto int64_res = parse_int64(tok.lexeme)) {
    auto node = std::make_unique<NumberNode>(tok, int64_res.value());
    return ok_node<NumberNode>(std::move(node));

  } else if (auto res = parse_double(tok.lexeme)) {
    auto node = std::make_unique<NumberNode>(tok, res.value());
    return ok_node<NumberNode>(std::move(node));

  } else {
    return err_node<NumberNode>(make_error_failed_to_parse_double(tok));
  }
}

MaybeNode<RefNode> ref_node(TokenIterator& it, ParseInstance&) {
  auto tok = it.peek();
  it.advance();

  auto num_res = expect_consume(it, TokenType::Number);
  if (!num_res) {
    return err_node<RefNode>(std::move(num_res.get_right()));
  }

  auto ref_res = parse_reference_identifier(num_res.get_left());
  if (!ref_res) {
    return err_node<RefNode>(make_error_failed_to_parse_reference_identifier(num_res.get_left()));
  } else {
    return ok_node<RefNode>(
      std::make_unique<RefNode>(tok, ref_res.value()));
  }
}

MaybeNode<Node> value_node(TokenIterator& it, ParseInstance& instance) {
  auto& curr = it.peek();
  switch (curr.type) {
    case TokenType::LeftBrace:
      return transform_maybe_node<ObjectNode, Node>(object_node(it, instance));

    case TokenType::LeftBracket:
      return transform_maybe_node<ArrayNode, Node>(array_node(it, instance));

    case TokenType::KeywordRef:
      return transform_maybe_node<RefNode, Node>(ref_node(it, instance));

    case TokenType::Number:
      return transform_maybe_node<NumberNode, Node>(number_node(it, instance));

    case TokenType::String:
      return transform_maybe_node<StringNode, Node>(string_node(it, instance));

    default:
      return err_node<Node>(make_error_unexpected_type(curr));
  }
}

MaybeNode<NewStructNode> new_struct_node(TokenIterator& it, ParseInstance& instance) {
  auto source_token = it.peek();
  it.advance();
  auto ident_res = expect_consume(it, TokenType::Identifier);
  if (!ident_res) {
    return err_node<NewStructNode>(std::move(ident_res.get_right()));
  }
  auto num_res = expect_consume(it, TokenType::Number);
  if (!num_res) {
    return err_node<NewStructNode>(std::move(num_res.get_right()));
  }

  auto registered_ident =
    instance.info->string_registry.emplace_view(ident_res.get_left().lexeme);

  auto num_tok = num_res.get_left();
  auto maybe_ref_ident = parse_reference_identifier(num_tok);
  if (!maybe_ref_ident) {
    return err_node<NewStructNode>(make_error_failed_to_parse_reference_identifier(num_tok));
  }

  auto brace_res = expect(it, TokenType::LeftBrace);
  if (!brace_res) {
    return err_node<NewStructNode>(std::move(brace_res.get_right()));
  }

  auto obj_res = object_node(it, instance);
  if (!obj_res) {
    return err_node<NewStructNode>(std::move(obj_res.get_right()));
  }

  auto ident = maybe_ref_ident.value();
  auto obj_node = std::move(obj_res.get_left());

  return ok_node<NewStructNode>(
    std::make_unique<NewStructNode>(
      source_token, registered_ident, ident, std::move(obj_node)));
}

} //  anon

io::ParseResult io::parse(const std::vector<Token>& tokens, ParseInfo& parse_info) {
  io::ParseResult result;
  TokenIterator it{&tokens};
  ParseInstance instance{&parse_info};

  while (it.has_next()) {
    const auto& tok = it.peek();
    if (tok.type == TokenType::KeywordNew) {
      auto res = new_struct_node(it, instance);
      if (!res) {
        it.advance_to(TokenType::KeywordNew);
      }
      push_result_or_error(result, std::move(res));

    } else {
      result.errors.push_back(make_error_unexpected_type(tok));
      it.advance();
    }
  }

  result.success = result.errors.empty();
  return result;
}

GROVE_NAMESPACE_END
