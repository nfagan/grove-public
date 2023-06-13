#include "scan.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include <array>

GROVE_NAMESPACE_BEGIN

using namespace ls;

namespace {

TokenType ident_or_kw_type(const char* c, int64_t size) {
  static const std::array<const char*, 11> kws{{
    "system",
    "module",
    "rule",
    "end",
    "pred",
    "if",
    "else",
    "return",
    "match",
    "axiom",
    "is",
  }};
  static const std::array<uint8_t, 11> kw_lens{{
    6,
    6,
    4,
    3,
    4,
    2,
    4,
    6,
    5,
    5,
    2
  }};
  static const std::array<TokenType, 11> types{{
    TokenType::KwSystem,
    TokenType::KwModule,
    TokenType::KwRule,
    TokenType::KwEnd,
    TokenType::KwPred,
    TokenType::KwIf,
    TokenType::KwElse,
    TokenType::KwReturn,
    TokenType::KwMatch,
    TokenType::KwAxiom,
    TokenType::KwIs
  }};
  for (int i = 0; i < int(kws.size()); i++) {
    auto* kw = kws[i];
    int64_t ind{};
    while (size == kw_lens[i] && ind < size) {
      if (c[ind] != kw[ind]) {
        break;
      } else {
        ind++;
      }
    }
    if (ind == size) {
      return types[i];
    }
  }
  return TokenType::Identifier;
}

std::string message_unrecognized_character(char c) {
  std::string res{"Unrecognized character: "};
  res += c;
  return res;
}

ScanError make_error(std::string&& msg) {
  ScanError res;
  res.message = std::move(msg);
  return res;
}

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}
bool is_alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool is_alpha_numeric(char c) {
  return is_digit(c) || is_alpha(c);
}
bool is_period(char c) {
  return c == '.';
}
bool is_whitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}
Token make_token(TokenType type, int64_t beg, int64_t end) {
  Token tok;
  tok.type = type;
  tok.begin = uint32_t(beg);
  tok.end = uint32_t(end);
  return tok;
}
void add_token(ScanResult* res, Token tok) {
  res->tokens.push_back(tok);
}

bool punct(ScanResult* res, const char* src, int64_t* i, int64_t size) {
  struct MatchNext {
    char next;
    TokenType type;
  };

  static const std::array<char, 17> first{{
    '*',
    '-',
    '+',
    '/',
    '\\',
    '(',
    ')',
    '[',
    ']',
    '{',
    '}',
    ':',
    '<',
    '>',
    ',',
    '.',
    '='
  }};

  static const std::array<TokenType, 17> first_types{{
    TokenType::Asterisk,
    TokenType::Minus,
    TokenType::Plus,
    TokenType::Fslash,
    TokenType::Bslash,
    TokenType::Lparen,
    TokenType::Rparen,
    TokenType::Lbracket,
    TokenType::Rbracket,
    TokenType::Lbrace,
    TokenType::Rbrace,
    TokenType::Colon,
    TokenType::Lt,
    TokenType::Gt,
    TokenType::Comma,
    TokenType::Period,
    TokenType::Equal
  }};

  static const std::array<MatchNext, 17> next{{
    {},
    {'>', TokenType::Arrow},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {'=', TokenType::Define}, //  12
    {'=', TokenType::Le},
    {'=', TokenType::Ge},
    {},
    {},
    {'=', TokenType::EqualEqual},
  }};

  const char c = src[*i];
  int64_t beg = *i;
  for (int fi = 0; fi < 17; fi++) {
    char f = first[fi];
    if (f == c) {
      TokenType type = first_types[fi];
      int64_t tok_size = 1;
      if (next[fi].next && *i + 1 < size && src[*i + 1] == next[fi].next) {
        type = next[fi].type;
        tok_size = 2;
      }
      *i += tok_size;
      add_token(res, make_token(type, beg, *i));
      return true;
    }
  }
  return false;
}

int64_t ident_or_kw(ScanResult* res, const char* src, int64_t i, int64_t size) {
  int64_t beg = i;
  while (i < size) {
    auto c = src[i];
    if (!is_alpha_numeric(c) && c != '_') {
      break;
    } else {
      i++;
    }
  }
  auto type = ident_or_kw_type(src + beg, i - beg);
  add_token(res, make_token(type, beg, i));
  return i;
}

int64_t digit(ScanResult* res, const char* src, int64_t i, int64_t size) {
  bool period = false;
  int64_t beg = i;
  while (i < size) {
    auto c = src[i];
    if (!is_digit(c)) {
      if (!period && is_period(c)) {
        period = true;
        i++;
      } else {
        break;
      }
    } else {
      i++;
    }
  }
  add_token(res, make_token(TokenType::Number, beg, i));
  return i;
}

int64_t eat_through_new_line(const char* src, int64_t i, int64_t size) {
  while (i < size) {
    if (src[i] == '\n') {
      i++;
      break;
    }
    i++;
  }
  return i;
}

void cases(ScanResult* res, const char* src, int64_t* i, int64_t size) {
  const char c = src[*i];
  if (is_digit(c)) {
    *i = digit(res, src, *i, size);
  } else if (is_alpha(c)) {
    *i = ident_or_kw(res, src, *i, size);
  } else if (c == '#') {
    *i = eat_through_new_line(src, *i, size);
  } else {
    bool punct_match = punct(res, src, i, size);
    if (!punct_match) {
      if (!is_whitespace(c)) {
        res->errors.push_back(make_error(message_unrecognized_character(c)));
      }
      (*i)++;
    }
  }
}

} //  anon

ScanResult ls::scan(const char* src, int64_t size) {
  ScanResult result;
  result.tokens.push_back(make_token(TokenType::Null, 0, 0));
  int64_t i{};
  while (i < size) {
    cases(&result, src, &i, size);
  }
  return result;
}

GROVE_NAMESPACE_END
