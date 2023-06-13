#include "resolve.hpp"
#include "visitor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace io::ast;

io::ParseError make_error_undefined_reference(const io::Token& source_token) {
  return io::ParseError{source_token, "Unresolved reference."};
}

class ReferenceResolver : public Visitor {
public:
  ReferenceResolver(io::ResolveResult* result,
                    const io::Declarations* decls) :
                    result{result}, decls{decls} {
    //
  }

  ~ReferenceResolver() override = default;

  void ref_node(RefNode& node) override {
    auto it = decls->find(node.target);
    if (it == decls->end()) {
      result->errors.push_back(make_error_undefined_reference(node.source_token));
    } else {
      node.target_node = it->second;
    }
  }

  void new_struct_node(NewStructNode& node) override {
    node.node->accept(*this);
  }

  void array_node(ArrayNode& node) override {
    for (auto& element : node.elements) {
      element->accept(*this);
    }
  }

  void object_node(ObjectNode& node) override {
    for (auto& field : node.fields) {
      field.second->accept(*this);
    }
  }

private:
  io::ResolveResult* result;
  const io::Declarations* decls;
};

} //  anon

io::ResolveResult io::resolve_references(const ast::Ast& ast, const Declarations& decls) {
  io::ResolveResult result;
  ReferenceResolver resolver{&result, &decls};

  for (auto& node : ast.nodes) {
    node->accept(resolver);
  }

  result.success = result.errors.empty();
  return result;
}

GROVE_NAMESPACE_END
