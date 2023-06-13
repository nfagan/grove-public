#pragma once

#include <string>

namespace grove::io {

class StringRegistry;

namespace ast {
  struct Ast;
}

struct AstToStringParams {
  bool pretty_format{false};
};

std::string to_string(const ast::Ast& ast, const StringRegistry& registry, AstToStringParams = {});

}