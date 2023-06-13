#include "ast.hpp"
#include "visitor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * CompilerDirective
 */

glsl::CompilerDirective::CompilerDirective(const Token& source_token,
                                           Type type,
                                           const char* begin,
                                           const char* end) :
  source_token(source_token),
  type(type),
  begin(begin),
  end(end) {
  //
}

void glsl::CompilerDirective::accept_const(AstVisitor& visitor) const {
  visitor.compiler_directive(*this);
}

void glsl::CompilerDirective::accept(AstVisitor& visitor) {
  visitor.compiler_directive(*this);
}

GROVE_NAMESPACE_END
