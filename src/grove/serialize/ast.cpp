#include "ast.hpp"
#include "visitor.hpp"
#include "grove/common/common.hpp"

#define VISITOR_ACCEPT(type, method)           \
  void ast::type::accept(Visitor& visitor) {   \
    return visitor.method(*this);              \
  }

#define VISITOR_ACCEPT_CONST(type, method)                  \
  void ast::type::accept_const(Visitor& visitor) const {    \
    return visitor.method(*this);                           \
  }

GROVE_NAMESPACE_BEGIN

using namespace io;

//  NewStructNode
VISITOR_ACCEPT(NewStructNode, new_struct_node)
VISITOR_ACCEPT_CONST(NewStructNode, new_struct_node)

//  RefNode
VISITOR_ACCEPT(RefNode, ref_node)
VISITOR_ACCEPT_CONST(RefNode, ref_node)

//  ObjectNode
VISITOR_ACCEPT(ObjectNode, object_node)
VISITOR_ACCEPT_CONST(ObjectNode, object_node)

ast::Node* ast::ObjectNode::field(RegisteredString str) {
  auto it = fields.find(str);
  return it == fields.end() ? nullptr : it->second.get();
}

const ast::Node* ast::ObjectNode::field(RegisteredString str) const {
  auto it = fields.find(str);
  return it == fields.end() ? nullptr : it->second.get();
}

//  ArrayNode
VISITOR_ACCEPT(ArrayNode, array_node)
VISITOR_ACCEPT_CONST(ArrayNode, array_node)

//  NumberNode
VISITOR_ACCEPT(NumberNode, number_node)
VISITOR_ACCEPT_CONST(NumberNode, number_node)

//  StringNode
VISITOR_ACCEPT(StringNode, string_node)
VISITOR_ACCEPT_CONST(StringNode, string_node)

GROVE_NAMESPACE_END

#undef VISITOR_ACCEPT_CONST
#undef VISITOR_ACCEPT
