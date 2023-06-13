#include "error.hpp"
#include "text.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

std::string io::ParseError::with_context(std::string_view source_text, int ctx_amount) const {
  if (source_token) {
    auto* text = source_text.data();
    auto& tok = source_token.value();
    const bool is_null = tok.type == TokenType::Null;

    const auto start = is_null ? 0 : tok.lexeme.data() - text;
    const auto stop = is_null ? 0 : tok.lexeme.data() + tok.lexeme.size() - text;

    return mark_text_with_message_and_context(source_text, start, stop, ctx_amount, message);

  } else {
    return message;
  }
}

GROVE_NAMESPACE_END
