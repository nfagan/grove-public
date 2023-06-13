#include "transform.hpp"
#include "ast.hpp"
#include "visitor.hpp"
#include "StringRegistry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace io::ast;
using namespace io;

namespace {

std::string tab_str(int depth) {
  std::string result;
  for (int i = 0; i < depth; i++) {
    result += "  ";
  }
  return result;
}

class ToStringVisitor : public Visitor {
public:
  ToStringVisitor(const StringRegistry& registry, std::string& result, AstToStringParams params) :
  result{result}, registry{registry}, params{params} {
    //
  }
  ~ToStringVisitor() override = default;

  void new_struct_node(const NewStructNode& node) override {
    result += "new ";
    result += registry.get(node.type);
    result.push_back(' ');
    result += std::to_string(node.ident.id);
    node.node->accept_const(*this);
  }

  void ref_node(const RefNode& node) override {
    result += "ref " + std::to_string(node.target.id);
  }

  void number_node(const NumberNode& node) override {
    if (node.type == io::ast::NumberNode::Type::Double) {
      result += std::to_string(node.double_value);

    } else if (node.type == io::ast::NumberNode::Type::Int64) {
      result += std::to_string(node.int_value);

    } else {
      assert(false);
    }
  }

  void string_node(const StringNode& node) override {
    result.push_back('\'');
    result += registry.get(node.str);
    result.push_back('\'');
  }

  void object_node(const ObjectNode& node) override {
    result.push_back('{');

    if (params.pretty_format) {
      tab_depth++;
    }

    for (auto& field : node.fields) {
      if (params.pretty_format) {
        result.push_back('\n');
        result += tab_str(tab_depth);
      }

      result += registry.get(field.first);
      result.push_back(':');
      field.second->accept_const(*this);
    }

    if (params.pretty_format) {
      result.push_back('\n');
      tab_depth--;
    }

    result.push_back('}');
  }

  void array_node(const ArrayNode& node) override {
    result.push_back('[');
    for (auto& el : node.elements) {
      el->accept_const(*this);
      if (el != node.elements.back()) {
        result.push_back(',');
      }
    }
    result.push_back(']');
  }

private:
  std::string& result;
  const StringRegistry& registry;
  AstToStringParams params;
  int tab_depth{};
};

} //  anon

std::string io::to_string(const ast::Ast& ast, const StringRegistry& registry, AstToStringParams params) {
  std::string result;
  ToStringVisitor visitor{registry, result, params};
  for (auto& node : ast.nodes) {
    node->accept_const(visitor);
  }
  return result;
}

GROVE_NAMESPACE_END
