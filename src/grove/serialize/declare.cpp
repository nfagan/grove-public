#include "declare.hpp"
#include "visitor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace io::ast;

io::ParseError make_error_duplicate_registered_identifier(const io::Token& source_token) {
  return io::ParseError{source_token, "Duplicate registered identifier."};
}

class DeclareVisitor : public io::ast::Visitor {
public:
  explicit DeclareVisitor(io::DeclareResult* result) : result{result} {
    //
  }
  ~DeclareVisitor() override = default;

  void new_struct_node(NewStructNode& node) override {
    if (result->declarations.count(node.ident) > 0) {
      result->errors.push_back(
        make_error_duplicate_registered_identifier(node.source_token));
    } else {
      result->declarations[node.ident] = &node;
    }
  }

private:
  io::DeclareResult* result;
};

} //  anon

io::DeclareResult io::declare_aggregates(const ast::Ast& ast) {
  io::DeclareResult result;
  DeclareVisitor visitor{&result};

  for (auto& node : ast.nodes) {
    node->accept(visitor);
  }

  result.success = result.errors.empty();
  return result;
}

GROVE_NAMESPACE_END
