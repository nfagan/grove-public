#pragma once

#include "token.hpp"
#include <string_view>

namespace grove::glsl {

TokenType maybe_keyword_token_type(const std::string_view& lex);

}