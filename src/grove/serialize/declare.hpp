#pragma once

#include "ast.hpp"
#include "error.hpp"
#include <unordered_map>

namespace grove::io {

using Declarations =
  std::unordered_map<ReferenceIdentifier, ast::Node*, ReferenceIdentifier::Hash>;

struct DeclareResult {
  Declarations declarations;
  std::vector<ParseError> errors;
  bool success{};
};

DeclareResult declare_aggregates(const ast::Ast& ast);

}