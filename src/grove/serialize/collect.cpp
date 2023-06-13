#include "collect.hpp"
#include "visitor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace io;
using namespace io::ast;

class CollectVisitor : public Visitor {
public:
  CollectVisitor(const CollectPredicate& pred,
                 std::vector<Node*>& to_collect) :
                 pred{pred}, collected{to_collect} {
    //
  }
  ~CollectVisitor() override = default;

  void maybe_collect(Node& node) {
    if (pred(node)) {
      collected.push_back(&node);
    }
  }

  void new_struct_node(NewStructNode& node) override {
    maybe_collect(node);
    node.node->accept(*this);
  }

  void ref_node(RefNode& node) override {
    maybe_collect(node);
  }

  void number_node(NumberNode& node) override {
    maybe_collect(node);
  }

  void string_node(StringNode& node) override {
    maybe_collect(node);
  }

  void object_node(ObjectNode& node) override {
    maybe_collect(node);
    for (auto& field : node.fields) {
      field.second->accept(*this);
    }
  }

  void array_node(ArrayNode& node) override {
    maybe_collect(node);
    for (auto& element : node.elements) {
      element->accept(*this);
    }
  }

private:
  const CollectPredicate& pred;
  std::vector<Node*>& collected;
};


} //  anon

std::vector<ast::Node*> io::collect_if(const ast::Ast& ast, const CollectPredicate& pred) {
  std::vector<ast::Node*> result;
  CollectVisitor vis{pred, result};
  for (auto& node : ast.nodes) {
    node->accept(vis);
  }
  return result;
}

GROVE_NAMESPACE_END
