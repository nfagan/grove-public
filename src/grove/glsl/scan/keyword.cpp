#include "keyword.hpp"
#include "grove/common/common.hpp"
#include <cstring>
#include <array>

GROVE_NAMESPACE_BEGIN

namespace {

bool matches(const char* a, const std::string_view& b) {
  const auto len_a = int(std::strlen(a));
  const auto len_b = int(b.size());

  if (len_a != len_b) {
    return false;
  }

  for (int i = 0; i < len_a; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

} //  anon

glsl::TokenType glsl::maybe_keyword_token_type(const std::string_view& lex) {
  static std::array<const char*, 2> keywords{{"pragma", "include"}};
  static std::array<TokenType, 2> types{{TokenType::KeywordPragma, TokenType::KeywordInclude}};

  for (int i = 0; i < int(keywords.size()); i++) {
    if (matches(keywords[i], lex)) {
      return types[i];
    }
  }

  return TokenType::Null;
}

GROVE_NAMESPACE_END
