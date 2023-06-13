#pragma once

#include "../scan/token.hpp"
#include <memory>
#include <vector>

namespace grove {

namespace glsl {
  class AstVisitor;

  struct AstNode {
    virtual ~AstNode() = default;
    virtual void accept_const(AstVisitor& visitor) const = 0;
    virtual void accept(AstVisitor& visitor) = 0;
  };

  struct CompilerDirective : public AstNode {
    enum class Type {
      Include = 0
    };

    CompilerDirective(const Token& source_token, Type type, const char* begin, const char* end);
    ~CompilerDirective() override = default;

    void accept_const(AstVisitor& visitor) const override;
    void accept(AstVisitor& visitor) override;

    Token source_token;
    Type type;
    const char* begin;
    const char* end;
  };

  using BoxedAstNode = std::unique_ptr<AstNode>;
  using BoxedAstNodes = std::vector<BoxedAstNode>;
}

}