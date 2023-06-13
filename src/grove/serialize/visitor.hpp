#pragma once

#define GROVE_IO_AST_VISITOR_METHOD(name, type) \
  virtual void name(type&) {} \
  virtual void name(const type&) {}

namespace grove::io::ast {

struct RefNode;
struct ObjectNode;
struct ArrayNode;
struct NewStructNode;
struct NumberNode;
struct StringNode;

class Visitor {
public:
  virtual ~Visitor() = default;

  GROVE_IO_AST_VISITOR_METHOD(ref_node, RefNode)
  GROVE_IO_AST_VISITOR_METHOD(object_node, ObjectNode)
  GROVE_IO_AST_VISITOR_METHOD(array_node, ArrayNode)
  GROVE_IO_AST_VISITOR_METHOD(new_struct_node, NewStructNode)
  GROVE_IO_AST_VISITOR_METHOD(number_node, NumberNode)
  GROVE_IO_AST_VISITOR_METHOD(string_node, StringNode)
};

}

#undef GROVE_IO_AST_VISITOR_METHOD