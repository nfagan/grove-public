#pragma once

#include "ast.hpp"
#include "error.hpp"
#include "grove/common/Either.hpp"

namespace grove::io {

class StringRegistry;
using MaybeAst = Either<io::ast::Ast, std::vector<io::ParseError>>;

MaybeAst make_ast(const std::string& source, io::StringRegistry& registry);

}