#pragma once

#include "ast.hpp"
#include <functional>

namespace grove::io {

using CollectPredicate = std::function<bool(const ast::Node&)>;

std::vector<ast::Node*> collect_if(const ast::Ast& ast, const CollectPredicate& pred);

}