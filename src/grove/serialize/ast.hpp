#pragma once

#include "token.hpp"
#include "common.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace grove::io::ast {

class Visitor;

struct Node {
  virtual ~Node() = default;
  virtual void accept(Visitor& vis) = 0;
  virtual void accept_const(Visitor& vis) const = 0;

  template <typename T>
  bool is() const {
    return dynamic_cast<T*>(this) != nullptr;
  }

  template <typename T>
  T* as() {
    return dynamic_cast<T*>(this);
  }

  template <typename T>
  const T* as() const {
    return dynamic_cast<T*>(this);
  }
};

using BoxedNode = std::unique_ptr<Node>;

struct Ast {
  std::vector<BoxedNode> nodes;
};

struct RefNode : public Node {
  RefNode(const Token& source_token, ReferenceIdentifier target) :
  source_token{source_token}, target{target} {
    //
  }
  ~RefNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Token source_token;
  ReferenceIdentifier target;
  Node* target_node{};
};

struct ObjectNode : public Node {
  using Fields =
    std::unordered_map<RegisteredString,
                       BoxedNode,
                       RegisteredString::Hash>;

  ObjectNode(const Token& source_token,
             Fields&& nodes) :
             source_token{source_token},
             fields{std::move(nodes)} {
    //
  }
  ~ObjectNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Node* field(RegisteredString str);
  const Node* field(RegisteredString str) const;

  template <typename T>
  T* field_as(RegisteredString str) {
    if (auto f = field(str)) {
      return f->as<T>();
    } else {
      return nullptr;
    }
  }

  template <typename T>
  const T* field_as(RegisteredString str) const {
    if (auto f = field(str)) {
      return f->as<T>();
    } else {
      return nullptr;
    }
  }

  Token source_token;
  Fields fields;
};

struct NewStructNode : public Node {
  NewStructNode(const Token& source_token,
                RegisteredString type,
                ReferenceIdentifier ident,
                std::unique_ptr<ObjectNode> node) :
    source_token{source_token}, type{type}, ident{ident}, node{std::move(node)} {
    //
  }
  ~NewStructNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Token source_token;
  RegisteredString type;
  ReferenceIdentifier ident;
  std::unique_ptr<ObjectNode> node;
};

struct ArrayNode : public Node {
  ArrayNode(const Token& source_token,
            std::vector<BoxedNode>&& nodes) :
            source_token{source_token},
            elements{std::move(nodes)} {
    //
  }
  ~ArrayNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Token source_token;
  std::vector<BoxedNode> elements;
};

struct NumberNode : public Node {
  enum class Type {
    Double,
    Int64
  };

  NumberNode(const Token& source_token, double v) :
    source_token{source_token}, type{Type::Double}, double_value{v} {
    //
  }

  NumberNode(const Token& source_token, int64_t v) :
    source_token{source_token}, type{Type::Int64}, int_value{v} {
    //
  }

  ~NumberNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Token source_token;
  Type type;

  union {
    double double_value;
    int64_t int_value;
  };
};

struct StringNode : public Node {
  StringNode(const Token& source_token, RegisteredString str) :
    source_token{source_token}, str{str} {
    //
  }
  ~StringNode() override = default;

  void accept(Visitor& visitor) override;
  void accept_const(Visitor& visitor) const override;

  Token source_token;
  RegisteredString str;
};

}