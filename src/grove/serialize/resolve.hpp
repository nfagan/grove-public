#pragma once

#include "ast.hpp"
#include "error.hpp"
#include "declare.hpp"

namespace grove::io {

struct ResolveResult {
  std::vector<io::ParseError> errors;
  bool success{};
};

ResolveResult resolve_references(const ast::Ast& ast, const Declarations& decls);

}