#pragma once

#include "grove/common/identifier.hpp"
#include <string>
#include <string_view>
#include <cstdint>
#include <unordered_map>

namespace grove::ls {

struct StringRef {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, StringRef, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(StringRef, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(StringRef, id)
  uint64_t id;
};

constexpr uint32_t null_scope_parent() {
  return ~0u;
}

struct Span {
  uint32_t begin;
  uint32_t size;
};

struct StorageLocation {
  uint32_t offset;
  uint32_t size;
};

struct Variable {
  uint32_t type;
  uint32_t storage;
};

struct ModuleField {
  StringRef name;
  uint32_t type;
  uint32_t storage;
};

struct ModuleDescriptor {
  StringRef name;
  Span field_descriptors;
};

struct ModuleFieldDescriptor {
  StringRef name;
  uint32_t type;
};

struct RuleParameter {
  uint32_t type;
  bool marked_pred;
};

struct StringSplice {
  uint32_t rule;
  uint32_t str_begin;
  uint32_t param_begin;
  uint32_t size;
};

struct DerivingString {
  const uint32_t* str;
  uint32_t str_size;
  const uint8_t* str_data;
  uint32_t str_data_size;
};

//  argument size in bytes, return size in bytes, data.
//  read arguments from, then write results to data.
using ForeignFunction = void(uint32_t, uint32_t, uint8_t*);

struct PendingForeignFunction {
  struct Hash {
    std::size_t operator()(const PendingForeignFunction& f) const noexcept {
      return std::hash<uint64_t>{}(f.identifier.id);
    }
  };
  struct Equal {
    bool operator()(const PendingForeignFunction& a,
                    const PendingForeignFunction& b) const noexcept {
      //  @NOTE: Don't use type_index in equality check. We don't yet hash function types,
      //  so multiple equivalent function types can exist with different type node indices.
      return a.identifier == b.identifier && a.scope == b.scope;
    }
  };

  StringRef identifier;
  uint32_t scope;
  uint32_t type_index;
};

constexpr uint32_t function_ptr_size() {
  return 8;
}

constexpr uint32_t bool_t_size() {
  return sizeof(int32_t);
}

struct Scope {
  bool has_parent() const {
    return parent != null_scope_parent();
  }

  uint32_t parent;
  std::unordered_map<StringRef, uint32_t, StringRef::Hash> types;
  std::unordered_map<StringRef, Variable, StringRef::Hash> variables;
  uint32_t stack_offset;
  uint32_t stack_size;
  bool all_sub_paths_return;
};

enum class TokenType : uint32_t {
  Null,
  Number,
  Identifier,
  Lparen,
  Rparen,
  Lbracket,
  Rbracket,
  Lbrace,
  Rbrace,
  Colon,
  Arrow,
  Plus,
  Minus,
  Lt,
  Le,
  Gt,
  Ge,
  Asterisk,
  Fslash,
  Bslash,
  Comma,
  Period,
  Define,
  Equal,
  EqualEqual,
  KwModule,
  KwSystem,
  KwRule,
  KwEnd,
  KwPred,
  KwIf,
  KwElse,
  KwReturn,
  KwMatch,
  KwAxiom,
  KwIs,
  NUM_TOKEN_TYPES
};

struct Token {
  TokenType type;
  uint32_t begin;
  uint32_t end;
  uint32_t line;
};

static_assert(sizeof(Token) == 16);

void show(const Token& tok, const char* src);
const char* to_string(TokenType type);
std::string_view make_lexeme(const Token& tok, const char* src);

enum class SubscriptMethod : uint8_t {
  None,
  Period,
  Parens
};

enum class AstNodeType {
  System,
  Axiom,
  Module,
  ModuleBranch,
  ModuleMetaTypeLabel,
  Rule,
  Parameter,
  TypeIdentifier,
  TypeFunction,
  //
  ExprIdentifierReference,
  ExprNumberLiteral,
  ExprBinary,
  ExprGrouping,
  //
  StmtExpr,
  StmtAssign,
  StmtIf,
  StmtReturn
};

struct AstNode {
  struct Rule {
    uint32_t param_begin;
    uint32_t param_size;
    uint32_t block_begin;
    uint32_t block_size;
  };
  struct System {
    StringRef identifier;
    uint32_t param_begin;
    uint32_t param_size;
    uint32_t rule_begin;
    uint32_t rule_size;
    uint32_t axiom_begin;
    uint32_t axiom_size;
  };
  struct Axiom {
    uint32_t str_begin;
    uint32_t str_size;
  };
  struct Module {
    StringRef identifier;
    uint32_t param_begin;
    uint32_t param_size;
    uint32_t meta_type_label_begin;
    uint32_t meta_type_label_size;
  };
  struct ModuleBranch {
    bool out;  //  in: [ vs out: ]
  };
  struct ModuleMetaTypeLabel {
    StringRef identifier;
  };
  struct Parameter {
    StringRef identifier;
    uint32_t type;
    bool marked_pred;
  };
  struct TypeIdentifier {
    StringRef identifier;
  };
  struct TypeFunction {
    uint32_t param_begin;
    uint32_t param_size;
    uint32_t ret_begin;
  };
  struct ExprBinary {
    TokenType op;
    uint32_t left;
    uint32_t right;
  };
  struct ExprNumberLiteral {
    float value;
  };
  struct ExprGrouping {
    uint32_t expr;
  };
  struct ExprIdentifierReference {
    StringRef identifier;
    SubscriptMethod subscript_method;
    uint32_t arg_begin;
    uint32_t arg_size;
  };
  struct StmtAssign {
    TokenType method;
    uint32_t lhs;
    uint32_t rhs;
  };
  struct StmtExpr {
    uint32_t expr;
  };
  struct StmtIf {
    uint32_t cond;
    uint32_t block_begin;
    uint32_t block_size;
    uint32_t else_block_begin;
    uint32_t else_block_size;
  };
  struct StmtReturn {
    bool match;
    uint32_t succ_str_begin;
    uint32_t succ_str_size;
    uint32_t result_str_begin;
    uint32_t result_str_size;
  };

  AstNodeType type;
  uint32_t token;
  union {
    Rule rule;
    System system;
    Axiom axiom;
    Module module;
    ModuleBranch module_branch;
    ModuleMetaTypeLabel module_meta_type_label;
    Parameter parameter;
    TypeIdentifier type_identifier;
    TypeFunction type_function;
    ExprBinary expr_binary;
    ExprNumberLiteral expr_number_literal;
    ExprGrouping expr_grouping;
    ExprIdentifierReference expr_identifier_reference;
    StmtAssign stmt_assign;
    StmtExpr stmt_expr;
    StmtIf stmt_if;
    StmtReturn stmt_return;
  };
};

struct TypeID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TypeID, id)
  uint32_t id;
};

struct TypeIDStore {
  uint32_t next() {
    return next_id++;
  }
  TypeID next_type_id() {
    return TypeID{next()};
  }

  uint32_t next_id{1};
};

enum class TypeNodeType {
  Null,
  Scalar,
  Function,
  Module,
  ModuleMetaType
};

struct TypeNode {
  struct Scalar {
    TypeID id;
    StringRef name;
    uint32_t storage;
  };
  struct Module {
    TypeID id;
    StringRef name;
    uint32_t storage;
    uint32_t field_begin;
    uint32_t field_size;
    uint32_t meta_type_begin;
    uint32_t meta_type_size;
  };
  struct ModuleMetaType {
    StringRef name;
  };
  struct Function {
    TypeID id;
    uint32_t param_begin;
    uint32_t param_size;
    uint32_t ret_begin;
  };

  TypeNodeType type;
  union {
    Scalar scalar;
    Module module;
    ModuleMetaType module_meta_type;
    Function function;
  };
};

struct Instructions {
  static constexpr uint8_t load = uint8_t(1);
  static constexpr uint8_t store = uint8_t(2);
  static constexpr uint8_t constantf = uint8_t(3);
  static constexpr uint8_t mulf = uint8_t(4);
  static constexpr uint8_t divf = uint8_t(5);
  static constexpr uint8_t addf = uint8_t(6);
  static constexpr uint8_t subf = uint8_t(7);
  static constexpr uint8_t ltf = uint8_t(8);
  static constexpr uint8_t gtf = uint8_t(9);
  static constexpr uint8_t lef = uint8_t(10);
  static constexpr uint8_t gef = uint8_t(11);
  static constexpr uint8_t testf = uint8_t(12);
  static constexpr uint8_t jump_if = uint8_t(13);
  static constexpr uint8_t jump = uint8_t(14);
  static constexpr uint8_t call = uint8_t(15);
  static constexpr uint8_t ret = uint8_t(16);
  static constexpr uint8_t vop = uint8_t(17);
};

}