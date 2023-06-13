#include "parse.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/serialize/utility.hpp"
#include <cassert>
#include <array>

GROVE_NAMESPACE_BEGIN

#define GROVE_TRY_ERR(expr) \
  if (auto err = (expr)) {  \
    return err;             \
  }

#define GROVE_TRY_ADD_ERR(expr)                           \
  if (auto err = (expr)) {                                \
    add_error(context->result, std::move(err.value()));   \
    return false;                                         \
  }

namespace {

using namespace ls;

enum class TokenPrecedence {
  None,
  Assign,
  IdentifierReference,
  Comp,
  Term,
  Factor
};

struct ParseContext {
  ParseResult* result;
  StringRegistry* str_registry;
  const char* source;
  const Token* tokens;
  int64_t num_tokens;
};

using ParsePrefix = Optional<ParseError>(const ParseContext*, int64_t*, uint32_t*);
using ParseInfix =
  Optional<ParseError>(const ParseContext*, int64_t*, TokenType, uint32_t, TokenPrecedence, uint32_t*);

struct ParseRule {
  ParsePrefix* prefix;
  ParseInfix* infix;
  TokenPrecedence prec;
};

uint32_t num_nodes(const ParseContext* context) {
  return uint32_t(context->result->nodes.size());
}
uint32_t num_parameters(const ParseContext* context) {
  return uint32_t(context->result->parameters.size());
}
uint32_t num_module_strings(const ParseContext* context) {
  return uint32_t(context->result->module_strings.size());
}
uint32_t num_stmt_blocks(const ParseContext* context) {
  return uint32_t(context->result->statement_blocks.size());
}
uint32_t num_subscripts(const ParseContext* context) {
  return uint32_t(context->result->subscripts.size());
}
uint32_t num_rules(const ParseContext* context) {
  return uint32_t(context->result->rules.size());
}
uint32_t num_axioms(const ParseContext* context) {
  return uint32_t(context->result->axioms.size());
}
uint32_t num_module_meta_type_labels(const ParseContext* ctx) {
  return uint32_t(ctx->result->module_meta_type_labels.size());
}

void add_error(ParseResult* res, ParseError&& err) {
  res->errors.push_back(std::move(err));
}
uint32_t add_node(const ParseContext* ctx, AstNode node) {
  auto ni = uint32_t(ctx->result->nodes.size());
  ctx->result->nodes.push_back(node);
  return ni;
}
void add_module_meta_label(const ParseContext* ctx, uint32_t li) {
  ctx->result->module_meta_type_labels.push_back(li);
}
void add_parameter(const ParseContext* ctx, uint32_t pi) {
  ctx->result->parameters.push_back(pi);
}
void add_subscript(const ParseContext* ctx, uint32_t si) {
  ctx->result->subscripts.push_back(si);
}
void add_rule(const ParseContext* ctx, uint32_t ri) {
  ctx->result->rules.push_back(ri);
}
void add_system(const ParseContext* ctx, uint32_t si) {
  ctx->result->systems.push_back(si);
}
void add_module(const ParseContext* ctx, uint32_t si) {
  ctx->result->modules.push_back(si);
}
void add_module_string(const ParseContext* ctx, uint32_t ri) {
  ctx->result->module_strings.push_back(ri);
}
void add_axiom(const ParseContext* ctx, uint32_t ai) {
  ctx->result->axioms.push_back(ai);
}

uint32_t add_stmt_block(const ParseContext* ctx, uint32_t stmt) {
  auto si = uint32_t(ctx->result->statement_blocks.size());
  ctx->result->statement_blocks.push_back(stmt);
  return si;
}

ParseContext make_parse_context(ParseResult* result,
                                StringRegistry* str_registry,
                                const char* source,
                                const Token* tokens,
                                int64_t num_tokens) {
  ParseContext ctx;
  ctx.result = result;
  ctx.str_registry = str_registry;
  ctx.source = source;
  ctx.tokens = tokens;
  ctx.num_tokens = num_tokens;
  return ctx;
}

ParseError make_error(std::string&& msg, int64_t tok) {
  ParseError err;
  err.message = std::move(msg);
  err.token = uint32_t(tok);
  return err;
}

std::string message_expected_token_types(const TokenType* expected, int size, TokenType actual) {
  std::string res{"Expected one of: "};
  for (int i = 0; i < size; i++) {
    res += to_string(expected[i]);
    if (i < size-1) {
      res += " | ";
    }
  }
  res += std::string("\nReceived: ") + to_string(actual);
  return res;
}

std::string message_expected_expression() {
  return "Expected expression.";
}
std::string message_unbalanced_brackets() {
  return "Unbalanced brackets.";
}
std::string message_non_contiguous_pred_decorators() {
  return "Pred decorators must be contiguous.";
}
std::string message_empty_rule() {
  return "Rule parameters cannot be empty.";
}

StringRef register_string(StringRegistry* registry, const Token* tok, const char* src) {
  std::string str{src + tok->begin, tok->end - tok->begin};
  return registry->emplace(str);
}

StringRef register_string(const ParseContext* context, const Token* tok) {
  return register_string(context->str_registry, tok, context->source);
}

const Token* peek(const Token* tokens, int64_t num_tokens, int64_t i) {
  assert(num_tokens > 0 && tokens[0].type == TokenType::Null);
  if (i >= num_tokens) {
    return tokens;
  } else {
    return tokens + i;
  }
}

const Token* peek(const ParseContext* context, int64_t i) {
  return peek(context->tokens, context->num_tokens, i);
}

inline int64_t advance(int64_t* i) {
  return (*i)++;
}

Optional<ParseError> consume(const Token* tokens, int64_t num_tokens, int64_t* i, TokenType type) {
  auto toki = advance(i);
  auto* tok = peek(tokens, num_tokens, toki);
  if (tok->type != type) {
    toki = toki >= num_tokens ? 0 : toki;
    return Optional<ParseError>(
      make_error(message_expected_token_types(&type, 1, tok->type), toki));
  } else {
    return NullOpt{};
  }
}

Optional<ParseError> consume(const ParseContext* ctx, int64_t* i, TokenType type) {
  return consume(ctx->tokens, ctx->num_tokens, i, type);
}

int64_t advance_up_to(const Token* tokens, int64_t i, int64_t size,
                      const TokenType* types, int num_types) {
  while (i < size) {
    auto& tt = tokens[i].type;
    for (int t = 0; t < num_types; t++) {
      if (tt == types[t]) {
        return i;
      }
    }
    i++;
  }
  return i;
}

void advance_up_to(const ParseContext* context, int64_t* i, const TokenType* types, int num_types) {
  *i = advance_up_to(context->tokens, *i, context->num_tokens, types, num_types);
}

AstNode make_module_node(StringRef ident, uint32_t param_beg, uint32_t param_sz,
                         uint32_t meta_beg, uint32_t meta_sz, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::Module;
  res.token = uint32_t(tok);
  res.module = {};
  res.module.identifier = ident;
  res.module.param_begin = param_beg;
  res.module.param_size = param_sz;
  res.module.meta_type_label_begin = meta_beg;
  res.module.meta_type_label_size = meta_sz;
  return res;
}

AstNode make_system_node(StringRef ident, uint32_t param_beg, uint32_t param_sz,
                         uint32_t rule_beg, uint32_t rule_sz,
                         uint32_t axiom_beg, uint32_t axiom_sz, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::System;
  res.token = uint32_t(tok);
  res.system = {};
  res.system.identifier = ident;
  res.system.param_begin = param_beg;
  res.system.param_size = param_sz;
  res.system.rule_begin = rule_beg;
  res.system.rule_size = rule_sz;
  res.system.axiom_begin = axiom_beg;
  res.system.axiom_size = axiom_sz;
  return res;
}

AstNode make_axiom_node(uint32_t str_beg, uint32_t str_sz, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::Axiom;
  res.token = uint32_t(tok);
  res.axiom = {};
  res.axiom.str_begin = str_beg;
  res.axiom.str_size = str_sz;
  return res;
}

AstNode make_rule_node(uint32_t param_beg, uint32_t param_sz,
                       uint32_t block_beg, uint32_t block_sz, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::Rule;
  res.token = uint32_t(tok);
  res.rule = {};
  res.rule.param_begin = param_beg;
  res.rule.param_size = param_sz;
  res.rule.block_begin = block_beg;
  res.rule.block_size = block_sz;
  return res;
}

AstNode make_type_identifier_node(StringRef ident, int64_t token) {
  AstNode res;
  res.type = AstNodeType::TypeIdentifier;
  res.token = uint32_t(token);
  res.type_identifier = {};
  res.type_identifier.identifier = ident;
  return res;
}

AstNode make_type_function_node(uint32_t param_beg, uint32_t num_params,
                                uint32_t ret_beg, int64_t token) {
  AstNode res;
  res.type = AstNodeType::TypeFunction;
  res.token = uint32_t(token);
  res.type_function = {};
  res.type_function.param_begin = param_beg;
  res.type_function.param_size = num_params;
  res.type_function.ret_begin = ret_beg;
  return res;
}

AstNode make_parameter_node(StringRef ident, uint32_t type, bool marked_pred, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::Parameter;
  res.token = uint32_t(tok);
  res.parameter = {};
  res.parameter.identifier = ident;
  res.parameter.type = type;
  res.parameter.marked_pred = marked_pred;
  return res;
}

AstNode make_binary_expr_node(TokenType op, uint32_t left, uint32_t right, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ExprBinary;
  res.token = uint32_t(tok);
  res.expr_binary = {};
  res.expr_binary.op = op;
  res.expr_binary.left = left;
  res.expr_binary.right = right;
  return res;
}

AstNode make_identifier_reference_expr_node(StringRef ident,
                                            SubscriptMethod method,
                                            uint32_t arg_beg, uint32_t num_args, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ExprIdentifierReference;
  res.token = uint32_t(tok);
  res.expr_identifier_reference = {};
  res.expr_identifier_reference.identifier = ident;
  res.expr_identifier_reference.subscript_method = method;
  res.expr_identifier_reference.arg_begin = arg_beg;
  res.expr_identifier_reference.arg_size = num_args;
  return res;
}

AstNode make_number_literal_expr_node(float value, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ExprNumberLiteral;
  res.token = uint32_t(tok);
  res.expr_number_literal = {};
  res.expr_number_literal.value = value;
  return res;
}

AstNode make_grouping_expr_node(uint32_t ni, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ExprGrouping;
  res.token = uint32_t(tok);
  res.expr_grouping = {};
  res.expr_grouping.expr = ni;
  return res;
}

AstNode make_assign_stmt_node(TokenType method, uint32_t lhs, uint32_t rhs, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::StmtAssign;
  res.token = uint32_t(tok);
  res.stmt_assign = {};
  res.stmt_assign.lhs = lhs;
  res.stmt_assign.rhs = rhs;
  res.stmt_assign.method = method;
  return res;
}

AstNode make_expr_stmt_node(uint32_t expr, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::StmtExpr;
  res.token = uint32_t(tok);
  res.stmt_expr = {};
  res.stmt_expr.expr = expr;
  return res;
}

AstNode make_if_stmt_node(uint32_t cond, uint32_t block_beg, uint32_t block_sz,
                          uint32_t else_block_beg, uint32_t else_block_sz, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::StmtIf;
  res.token = uint32_t(tok);
  res.stmt_if = {};
  res.stmt_if.cond = cond;
  res.stmt_if.block_begin = block_beg;
  res.stmt_if.block_size = block_sz;
  res.stmt_if.else_block_begin = else_block_beg;
  res.stmt_if.else_block_size = else_block_sz;
  return res;
}

AstNode make_return_stmt_node(bool match, uint32_t succ_str_begin, uint32_t succ_str_size,
                              uint32_t ret_str_begin, uint32_t ret_str_size, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::StmtReturn;
  res.token = uint32_t(tok);
  res.stmt_return = {};
  res.stmt_return.match = match;
  res.stmt_return.result_str_begin = ret_str_begin;
  res.stmt_return.result_str_size = ret_str_size;
  res.stmt_return.succ_str_begin = succ_str_begin;
  res.stmt_return.succ_str_size = succ_str_size;
  return res;
}

AstNode make_branch_in_node(int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ModuleBranch;
  res.token = uint32_t(tok);
  res.module_branch = {};
  res.module_branch.out = false;
  return res;
}

AstNode make_branch_out_node(int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ModuleBranch;
  res.token = uint32_t(tok);
  res.module_branch = {};
  res.module_branch.out = true;
  return res;
}

AstNode make_module_type_label_node(StringRef ident, int64_t tok) {
  AstNode res;
  res.type = AstNodeType::ModuleMetaTypeLabel;
  res.token = uint32_t(tok);
  res.module_meta_type_label = {};
  res.module_meta_type_label.identifier = ident;
  return res;
}

Optional<ParseError> argument_types(const ParseContext* context,
                                    int64_t* i, uint32_t* ni_beg, uint32_t* ni_sz) {
  std::vector<int64_t> args;
  while (*i < context->num_tokens) {
    auto toki = *i;
    auto tok = peek(context, toki);
    if (tok->type == TokenType::Rparen) {
      break;
    }
    GROVE_TRY_ERR(consume(context, i, TokenType::Identifier))
    args.push_back(*i - 1);
    auto nexti = *i;
    auto next = peek(context, nexti);
    if (next->type == TokenType::Comma) {
      advance(i);
    }
  }
  GROVE_TRY_ERR(consume(context, i, TokenType::Rparen));
  *ni_beg = num_nodes(context);
  for (int64_t argi : args) {
    auto* arg_tok = peek(context, argi);
    auto arg_node = make_type_identifier_node(register_string(context, arg_tok), argi);
    add_node(context, arg_node);
  }
  *ni_sz = num_nodes(context) - *ni_beg;
  return NullOpt{};
}

Optional<ParseError> type(const ParseContext* context, int64_t* i, uint32_t* ni) {
  auto toki = *i;
  auto* next = peek(context, *i);
  if (next->type == TokenType::Lparen) {
    //  function type
    advance(i);
    uint32_t arg_beg;
    uint32_t arg_sz;
    GROVE_TRY_ERR(argument_types(context, i, &arg_beg, &arg_sz))
    GROVE_TRY_ERR(consume(context, i, TokenType::Arrow))
    GROVE_TRY_ERR(consume(context, i, TokenType::Identifier))
    auto reti = *i - 1;
    auto ret_tok = peek(context, reti);
    auto ret_node = make_type_identifier_node(register_string(context, ret_tok), reti);
    auto ret_beg = num_nodes(context);
    add_node(context, ret_node);
    *ni = add_node(context, make_type_function_node(arg_beg, arg_sz, ret_beg, toki));
  } else {
    //  identifier type
    GROVE_TRY_ERR(consume(context, i, TokenType::Identifier))
    auto ii = *i - 1;
    auto ident_tok = peek(context, ii);
    auto ident_ref = register_string(context, ident_tok);
    *ni = add_node(context, make_type_identifier_node(ident_ref, ii));
  }
  return NullOpt{};
}

Optional<ParseError> function_parameter(const ParseContext* context, int64_t* i, uint32_t* pi) {
  auto toki = *i;
  auto* tok = peek(context, *i);
  bool is_pred = false;
  if (tok->type == TokenType::KwPred) {
    advance(i);
    is_pred = true;
  }
  GROVE_TRY_ERR(consume(context, i, TokenType::Identifier))
  auto param_name = *i - 1;
  auto ident = register_string(context, peek(context, param_name));
  GROVE_TRY_ERR(consume(context, i, TokenType::Colon))
  uint32_t ni;
  GROVE_TRY_ERR(type(context, i, &ni))
  *pi = add_node(context, make_parameter_node(ident, ni, is_pred, toki));
  return NullOpt{};
}

Optional<ParseError> function_parameters(const ParseContext* context, int64_t* i,
                                         uint32_t* pbeg, uint32_t* pend) {
  *pbeg = num_parameters(context);
  while (*i < context->num_tokens) {
    auto* tok = peek(context, *i);
    if (tok->type == TokenType::Rparen) {
      break;
    }
    uint32_t pi;
    GROVE_TRY_ERR(function_parameter(context, i, &pi))
    add_parameter(context, pi);
    auto nexti = *i;
    auto* next = peek(context, *i);
    if (next->type == TokenType::Comma) {
      advance(i);
    } else if (next->type != TokenType::Rparen) {
      advance(i);
      const TokenType expected[2] = {TokenType::Rparen, TokenType::Comma};
      return Optional<ParseError>(
        make_error(message_expected_token_types(expected, 2, next->type), nexti));
    }
  }
  *pend = num_parameters(context);
  return consume(context, i, TokenType::Rparen);
}

Optional<ParseError> expr_prec(const ParseContext* context, int64_t* i,
                               TokenPrecedence prec, uint32_t* ni);
Optional<ParseError> expr(const ParseContext* context, int64_t* i, uint32_t* ni);

Optional<ParseError> identifier_reference_expr(const ParseContext* context,
                                               int64_t* i, uint32_t* ni) {
  GROVE_TRY_ERR(consume(context, i, TokenType::Identifier))
  auto identi = *i - 1;
  auto* ident_tok = peek(context, identi);
  auto next = peek(context, *i);
  auto method = SubscriptMethod::None;
  std::vector<uint32_t> subs;
  if (next->type == TokenType::Period) {
    //  a.b
    advance(i);
    uint32_t sub;
    GROVE_TRY_ERR(identifier_reference_expr(context, i, &sub))
    subs.push_back(sub);
    method = SubscriptMethod::Period;
  } else if (next->type == TokenType::Lparen) {
    //  a()
    advance(i);
    while (*i < context->num_tokens) {
      if (peek(context, *i)->type == TokenType::Rparen) {
        advance(i);
        break;
      }

      uint32_t sub;
      GROVE_TRY_ERR(expr(context, i, &sub))
      subs.push_back(sub);
      const auto nexti = *i;
      auto next_next = peek(context, nexti);
      advance(i);
      if (next_next->type == TokenType::Rparen) {
        break;
      } else if (next_next->type != TokenType::Comma) {
        const TokenType expected[2] = {TokenType::Rparen, TokenType::Comma};
        return Optional<ParseError>(make_error(
          message_expected_token_types(expected, 2, next_next->type), nexti));
      }
    }
    method = SubscriptMethod::Parens;
  }
  const uint32_t arg_beg = num_subscripts(context);
  for (uint32_t sub : subs) {
    add_subscript(context, sub);
  }
  uint32_t num_args = num_subscripts(context) - arg_beg;
  auto ident_ref = register_string(context, ident_tok);
  auto node = make_identifier_reference_expr_node(ident_ref, method, arg_beg, num_args, identi);
  *ni = add_node(context, node);
  return NullOpt{};
}

Optional<ParseError> binary_expr(const ParseContext* context,
                                 int64_t* i, TokenType op, uint32_t left,
                                 TokenPrecedence prec, uint32_t* ni) {
  auto toki = *i;
  uint32_t right;
  GROVE_TRY_ERR(expr_prec(context, i, TokenPrecedence(int(prec)+1), &right))
  auto node = make_binary_expr_node(op, left, right, toki);
  *ni = add_node(context, node);
  return NullOpt{};
}

Optional<ParseError> number_expr(const ParseContext* context, int64_t* i, uint32_t* ni) {
  GROVE_TRY_ERR(consume(context, i, TokenType::Number))
  auto previ = *i - 1;
  auto* prev = peek(context, previ);
  auto num = io::parse_double(make_lexeme(*prev, context->source));
  assert(num);
  auto node = make_number_literal_expr_node(float(num.value()), previ);
  *ni = add_node(context, node);
  return NullOpt{};
}

Optional<ParseError> grouping_expr(const ParseContext* context, int64_t* i, uint32_t* ni) {
  auto toki = *i;
  advance(i);
  uint32_t ei;
  GROVE_TRY_ERR(expr(context, i, &ei))
  GROVE_TRY_ERR(consume(context, i, TokenType::Rparen))
  auto node = make_grouping_expr_node(ei, toki);
  *ni = add_node(context, node);
  return NullOpt{};
}

ParseRule* get_rule(TokenType type) {
  static std::array<ParseRule, int(TokenType::NUM_TOKEN_TYPES)> rules{};
  static bool initialized{};
  if (!initialized) {
    rules[int(TokenType::Identifier)] = {identifier_reference_expr, nullptr, TokenPrecedence::None};
    rules[int(TokenType::Gt)] = {nullptr, binary_expr, TokenPrecedence::Comp};
    rules[int(TokenType::Lt)] = {nullptr, binary_expr, TokenPrecedence::Comp};
    rules[int(TokenType::Ge)] = {nullptr, binary_expr, TokenPrecedence::Comp};
    rules[int(TokenType::Le)] = {nullptr, binary_expr, TokenPrecedence::Comp};
    rules[int(TokenType::EqualEqual)] = {nullptr, binary_expr, TokenPrecedence::Comp};
    rules[int(TokenType::Plus)] = {nullptr, binary_expr, TokenPrecedence::Term};
    rules[int(TokenType::Minus)] = {nullptr, binary_expr, TokenPrecedence::Term};
    rules[int(TokenType::Asterisk)] = {nullptr, binary_expr, TokenPrecedence::Factor};
    rules[int(TokenType::Fslash)] = {nullptr, binary_expr, TokenPrecedence::Factor};
    rules[int(TokenType::Number)] = {number_expr, nullptr, TokenPrecedence::None};
    rules[int(TokenType::Lparen)] = {grouping_expr, nullptr, TokenPrecedence::None};
    initialized = true;
  }
  return &rules[int(type)];
}

bool can_start_new_expr(TokenType type) {
  return type == TokenType::Identifier || type == TokenType::Comma;
}

Optional<ParseError> expr_prec(const ParseContext* context, int64_t* i,
                               TokenPrecedence prec, uint32_t* ni) {
  auto* tok = peek(context, *i);
  auto* rule = get_rule(tok->type);
  if (!rule->prefix) {
    return Optional<ParseError>(make_error(message_expected_expression(), *i));
  }

  uint32_t ei;
  auto err = rule->prefix(context, i, &ei);
  while (!err && *i < context->num_tokens) {
    auto* next = peek(context, *i);
    auto next_rule = get_rule(next->type);
    if (!next_rule->prefix && !next_rule->infix) {
      //  term
      break;
    } else if (!next_rule->infix) {
      if (can_start_new_expr(next->type)) {
        break;
      } else {
        auto toki = *i;
        advance(i);
        return Optional<ParseError>(make_error(message_expected_expression(), toki));
      }
    } else {
      if (prec <= next_rule->prec) {
        advance(i);
        err = next_rule->infix(context, i, next->type, ei, next_rule->prec, &ei);
      } else {
        break;
      }
    }
  }
  *ni = ei;
  return err;
}

Optional<ParseError> expr(const ParseContext* context, int64_t* i, uint32_t* ni) {
  return expr_prec(context, i, TokenPrecedence::Assign, ni);
}

Optional<ParseError> expr_stmt(const ParseContext* context, int64_t* i, uint32_t* ni) {
  uint32_t lhs;
  auto lhs_toki = *i;
  GROVE_TRY_ERR(expr(context, i, &lhs))
  auto next = peek(context, *i);
  if (next->type == TokenType::Equal || next->type == TokenType::Define) {
    advance(i);
    uint32_t rhs;
    GROVE_TRY_ERR(expr(context, i, &rhs))
    auto node = make_assign_stmt_node(next->type, lhs, rhs, lhs_toki);
    *ni = add_node(context, node);
  } else {
    auto node = make_expr_stmt_node(lhs, lhs_toki);
    *ni = add_node(context, node);
  }
  return NullOpt{};
}

bool stmt_block(const ParseContext* context, int64_t* i, uint32_t* sbeg, uint32_t* send);

bool if_stmt(const ParseContext* context, int64_t* i, uint32_t* ni) {
  auto toki = *i;
  advance(i);
  uint32_t cond;
  GROVE_TRY_ERR(expr(context, i, &cond))
  uint32_t block_beg;
  uint32_t block_end;
  if (!stmt_block(context, i, &block_beg, &block_end)) {
    return false;
  }

  auto next = peek(context, *i);
  uint32_t else_block_beg{};
  uint32_t else_block_end{};
  if (next->type == TokenType::KwElse) {
    advance(i);
    if (!stmt_block(context, i, &else_block_beg, &else_block_end)) {
      return false;
    }
  }

  GROVE_TRY_ERR(consume(context, i, TokenType::KwEnd))
  auto block_sz = block_end - block_beg;
  auto else_block_sz = else_block_end - else_block_beg;
  auto node = make_if_stmt_node(
    cond, block_beg, block_sz, else_block_beg, else_block_sz, toki);
  *ni = add_node(context, node);
  return true;
}

Optional<ParseError> module_str(const ParseContext* context, int64_t* i, TokenType term,
                                uint32_t* str_beg, uint32_t* str_end) {
  *str_beg = num_module_strings(context);
  int branch_depth{};
  while (*i < context->num_tokens) {
    auto tok = peek(context, *i);
    if (tok->type == term) {
      break;
    }

    //  branch in
    if (tok->type == TokenType::Lbracket) {
      branch_depth++;
      add_module_string(context, add_node(context, make_branch_in_node(*i)));
      advance(i);
    }

    uint32_t ni;
    GROVE_TRY_ERR(expr(context, i, &ni))
    add_module_string(context, ni);

    //  branch out
    auto nexti = *i;
    auto next = peek(context, nexti);
    while (next->type == TokenType::Rbracket) {
      if (branch_depth == 0) {
        return Optional<ParseError>(make_error(message_unbalanced_brackets(), nexti));
      }
      branch_depth--;
      add_module_string(context, add_node(context, make_branch_out_node(nexti)));
      advance(i);
      nexti = *i;
      next = peek(context, nexti);
    }

    if (next->type == TokenType::Comma) {
      advance(i);
    } else if (next->type != term) {
      advance(i);
      const TokenType expected[2] = {term, TokenType::Comma};
      return Optional<ParseError>(
        make_error(message_expected_token_types(expected, 2, next->type), nexti));
    }
  }

  if (branch_depth != 0) {
    return Optional<ParseError>(make_error(message_unbalanced_brackets(), *i));
  }

  GROVE_TRY_ERR(consume(context, i, term))
  *str_end = num_module_strings(context);
  return NullOpt{};
}

bool return_stmt(const ParseContext* context, int64_t* i, uint32_t* ni) {
  auto toki = *i;
  advance(i);

  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lbrace))

  bool allow_return_str{};
  if (peek(context, *i)->type == TokenType::KwMatch) {
    GROVE_TRY_ADD_ERR(consume(context, i, TokenType::KwMatch))
    GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Comma))
    GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lbrace))
    allow_return_str = true;
  }

  uint32_t succ_str_beg;
  uint32_t succ_str_end;
  GROVE_TRY_ADD_ERR(module_str(context, i, TokenType::Rbrace, &succ_str_beg, &succ_str_end))

  uint32_t ret_str_beg{};
  uint32_t ret_str_end{};
  if (allow_return_str) {
    if (peek(context, *i)->type != TokenType::Rbrace) {
      //  Allow empty return string.
      GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Comma))
      GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lbrace))
      GROVE_TRY_ADD_ERR(module_str(context, i, TokenType::Rbrace, &ret_str_beg, &ret_str_end))
    }
    GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Rbrace))
  }

  const bool match = true;  //  @TODO
  auto succ_str_sz = succ_str_end - succ_str_beg;
  auto ret_str_sz = ret_str_end - ret_str_beg;
  auto node = make_return_stmt_node(
    match, succ_str_beg, succ_str_sz, ret_str_beg, ret_str_sz, toki);
  *ni = add_node(context, node);
  return true;
}

bool stmt_block(const ParseContext* context, int64_t* i, uint32_t* sbeg, uint32_t* send) {
  bool had_error = false;
  std::vector<uint32_t> nis;
  bool proceed = true;
  while (*i < context->num_tokens && proceed) {
    auto* tok = peek(context, *i);
    bool new_error = false;
    switch (tok->type) {
      case TokenType::KwIf: {
        uint32_t ni;
        if (if_stmt(context, i, &ni)) {
          nis.push_back(ni);
        } else {
          new_error = true;
        }
        break;
      }
      case TokenType::KwReturn: {
        uint32_t ni;
        if (return_stmt(context, i, &ni)) {
          nis.push_back(ni);
        } else {
          new_error = true;
        }
        break;
      }
      case TokenType::KwElse:
      case TokenType::KwEnd:
        proceed = false;
        break;
      default: {
        uint32_t ni;
        if (auto err = expr_stmt(context, i, &ni)) {
          new_error = true;
          add_error(context->result, std::move(err.value()));
        } else {
          nis.push_back(ni);
        }
      }
    }
    if (new_error) {
      had_error = true;
      const TokenType tts[2] = {TokenType::KwIf, TokenType::KwEnd};
      advance_up_to(context, i, tts, 2);
    }
  }
  if (!had_error) {
    *sbeg = num_stmt_blocks(context);
    for (uint32_t si : nis) {
      add_stmt_block(context, si);
    }
    *send = num_stmt_blocks(context);
  }
  return !had_error;
}

bool axiom(const ParseContext* context, int64_t* i, uint32_t* ai) {
  auto toki = *i;
  advance(i);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lbrace))

  uint32_t str_beg;
  uint32_t str_end;
  GROVE_TRY_ADD_ERR(module_str(context, i, TokenType::Rbrace, &str_beg, &str_end))

  auto axiom_node = make_axiom_node(str_beg, str_end - str_beg, toki);
  *ai = add_node(context, axiom_node);
  return true;
}

Optional<ParseError> validate_rule_parameters(const ParseContext* ctx, int64_t rule_tok,
                                              uint32_t param_beg, uint32_t param_end,
                                              bool* any_pred) {
  //  Disallow non-contiguous predecessors in rule parameters, e.g. do not allow:
  //    rule (pred a: A, b: B, pred c: C)
  //  do allow:
  //    rule (a: A, pred b: B, pred c: C, d: D)
  *any_pred = false;

  if (param_beg == param_end) {
    return Optional<ParseError>(make_error(message_empty_rule(), rule_tok));
  }

  uint32_t last_pred{};
  for (uint32_t p = param_beg; p < param_end; p++) {
    auto& param = ctx->result->nodes[ctx->result->parameters[p]];
    assert(param.type == AstNodeType::Parameter);
    if (param.parameter.marked_pred) {
      if (!*any_pred) {
        *any_pred = true;
      } else if (p - last_pred != 1) {
        auto err = make_error(message_non_contiguous_pred_decorators(), param.token);
        return Optional<ParseError>(std::move(err));
      }
      last_pred = p;
    }
  }

  return NullOpt{};
}

void mark_all_as_pred(const ParseContext* ctx, uint32_t param_beg, uint32_t param_end) {
  for (uint32_t p = param_beg; p < param_end; p++) {
    auto& param = ctx->result->nodes[ctx->result->parameters[p]];
    assert(param.type == AstNodeType::Parameter);
    param.parameter.marked_pred = true;
  }
}

bool rule(const ParseContext* context, int64_t* i, uint32_t* ri) {
  auto toki = *i;
  advance(i);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lparen))

  uint32_t param_beg;
  uint32_t param_end;
  GROVE_TRY_ADD_ERR(function_parameters(context, i, &param_beg, &param_end))

  bool any_pred{};
  GROVE_TRY_ADD_ERR(validate_rule_parameters(context, toki, param_beg, param_end, &any_pred))

  if (!any_pred) {
    mark_all_as_pred(context, param_beg, param_end);
  }

  uint32_t block_beg;
  uint32_t block_end;
  if (!stmt_block(context, i, &block_beg, &block_end)) {
    return false;
  }
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::KwEnd))

  auto rule_node = make_rule_node(
    param_beg, param_end - param_beg, block_beg, block_end - block_beg, toki);
  *ri = add_node(context, rule_node);
  return true;
}

bool system_body(const ParseContext* context, int64_t* i,
                 uint32_t* rule_beg, uint32_t* rule_end,
                 uint32_t* axiom_beg, uint32_t* axiom_end) {
  bool had_error = false;
  std::vector<uint32_t> ris;
  std::vector<uint32_t> ais;
  bool proceed = true;
  while (*i < context->num_tokens && proceed) {
    auto* tok = peek(context, *i);
    if (tok->type == TokenType::KwRule) {
      uint32_t ri;
      if (!rule(context, i, &ri)) {
        had_error = true;
      } else {
        ris.push_back(ri);
      }
    } else if (tok->type == TokenType::KwAxiom) {
      uint32_t ai;
      if (!axiom(context, i, &ai)) {
        had_error = true;
      } else {
        ais.push_back(ai);
      }
    } else if (tok->type == TokenType::KwEnd) {
      proceed = false;
      break;
    } else {
      auto rt = TokenType::KwRule;
      auto err = make_error(message_expected_token_types(&rt, 1, tok->type), *i);
      add_error(context->result, std::move(err));
      advance_up_to(context, i, &rt, 1);
      had_error = true;
    }
  }
  if (!had_error) {
    *rule_beg = num_rules(context);
    for (uint32_t ri : ris) {
      add_rule(context, ri);
    }
    *rule_end = num_rules(context);
    *axiom_beg = num_axioms(context);
    for (uint32_t ai : ais) {
      add_axiom(context, ai);
    }
    *axiom_end = num_axioms(context);
  }
  return !had_error;
}

bool module(const ParseContext* context, int64_t* i, uint32_t* mi) {
  auto toki = *i;
  advance(i);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Identifier))
  auto name = peek(context, *i - 1);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lparen))

  uint32_t param_beg;
  uint32_t param_end;
  GROVE_TRY_ADD_ERR(function_parameters(context, i, &param_beg, &param_end))

  uint32_t meta_label_beg = num_module_meta_type_labels(context);
  if (peek(context, *i)->type == TokenType::KwIs) {
    advance(i);
    GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Identifier))
    auto meta_label = register_string(context, peek(context, *i - 1));
    auto meta_type_node = make_module_type_label_node(meta_label, *i - 1);
    add_module_meta_label(context, add_node(context, meta_type_node));
  }
  uint32_t meta_label_end = num_module_meta_type_labels(context);

  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::KwEnd))

  auto ident = register_string(context, name);
  auto node = make_module_node(
    ident, param_beg, param_end - param_beg,
    meta_label_beg, meta_label_end - meta_label_beg, toki);
  *mi = add_node(context, node);
  return true;
}

bool system(const ParseContext* context, int64_t* i, uint32_t* si) {
  auto toki = *i;
  advance(i);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Identifier))
  auto name_tok = peek(context, *i - 1);
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::Lparen))

  uint32_t param_beg;
  uint32_t param_end;
  GROVE_TRY_ADD_ERR(function_parameters(context, i, &param_beg, &param_end))

  uint32_t rule_beg;
  uint32_t rule_end;
  uint32_t axiom_beg;
  uint32_t axiom_end;
  if (!system_body(context, i, &rule_beg, &rule_end, &axiom_beg, &axiom_end)) {
    return false;
  }
  GROVE_TRY_ADD_ERR(consume(context, i, TokenType::KwEnd))

  auto ident = register_string(context, name_tok);
  auto node = make_system_node(
    ident,
    param_beg,
    param_end - param_beg,
    rule_beg,
    rule_end - rule_beg,
    axiom_beg,
    axiom_end - axiom_beg,
    toki);
  *si = add_node(context, node);
  return true;
}

} //  anon

ParseResult ls::parse(const Token* tokens, int64_t size, const ParseParams* params) {
  ParseResult result;
  auto context = make_parse_context(
    &result, params->str_registry, params->source, tokens, size);
  int64_t i{1}; //  skip null token;
  while (i < size) {
    auto& tok = tokens[i];
    switch (tok.type) {
      case TokenType::KwSystem: {
        uint32_t si;
        if (system(&context, &i, &si)) {
          add_system(&context, si);
        }
        break;
      }
      case TokenType::KwModule: {
        uint32_t mi;
        if (module(&context, &i, &mi)) {
          add_module(&context, mi);
        }
        break;
      }
      default: {
        constexpr int nt = 2;
        const TokenType types[nt] = {TokenType::KwSystem, TokenType::KwModule};
        add_error(&result, make_error(message_expected_token_types(types, nt, tok.type), i));
        advance_up_to(&context, &i, types, nt);
      }
    }
  }
  return result;
}

GROVE_NAMESPACE_END
