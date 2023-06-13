#include "dump.hpp"
#include "parse.hpp"
#include "resolve.hpp"
#include "StringRegistry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

std::string dump_expr(uint32_t ni, DumpContext* ctx);
std::string dump_stmt(uint32_t si, DumpContext* ctx);
std::string dump_type(uint32_t ti, DumpContext* ctx);

const char* op_str(TokenType type) {
  switch (type) {
    case TokenType::Gt:
      return ">";
    case TokenType::Lt:
      return "<";
    case TokenType::Ge:
      return ">=";
    case TokenType::Le:
      return "<=";
    case TokenType::Plus:
      return "+";
    case TokenType::Minus:
      return "-";
    case TokenType::Asterisk:
      return "*";
    case TokenType::Fslash:
      return "/";
    case TokenType::Define:
      return ":=";
    case TokenType::Equal:
      return "=";
    case TokenType::EqualEqual:
      return "==";
    default:
      assert(false);
      return "";
  }
}

std::string expr_lparen(DumpContext* ctx) {
  if (ctx->parens_expr) {
    return "(";
  } else {
    return "";
  }
}

std::string expr_rparen(DumpContext* ctx) {
  if (ctx->parens_expr) {
    return ")";
  } else {
    return "";
  }
}

std::string tab_str(const DumpContext* ctx) {
  std::string res;
  for (int i = 0; i < ctx->tab; i++) {
    res += "  ";
  }
  return res;
}

void tab_in(DumpContext* ctx) {
  ctx->tab++;
}

void tab_out(DumpContext* ctx) {
  ctx->tab--;
  assert(ctx->tab >= 0);
}

std::string dump_function_type(const AstNode& node, DumpContext* ctx) {
  std::string str{"("};
  for (uint32_t i = 0; i < node.type_function.param_size; i++) {
    str += dump_type(node.type_function.param_begin + i, ctx);
    if (i < node.type_function.param_size-1) {
      str += ",";
    }
  }
  str += ") -> ";
  str += dump_type(node.type_function.ret_begin, ctx);
  return str;
}

std::string dump_type(uint32_t ti, DumpContext* ctx) {
  auto& ty = ctx->nodes[ti];
  switch (ty.type) {
    case AstNodeType::TypeIdentifier: {
      return ctx->registry->get(ty.type_identifier.identifier);
    }
    case AstNodeType::TypeFunction: {
      return dump_function_type(ty, ctx);
    }
    default: {
      assert(false);
      return "";
    }
  }
}

std::string dump_parameter(uint32_t pi, DumpContext* ctx) {
  auto& node = ctx->nodes[pi];
  assert(node.type == AstNodeType::Parameter);
  auto& param = node.parameter;
  std::string str;
  if (param.marked_pred) {
    str += "pred ";
  }
  str += ctx->registry->get(param.identifier);
  str += ": ";
  str += dump_type(param.type, ctx);
  return str;
}

std::string dump_parameters(uint32_t pbegin, uint32_t psize, DumpContext* ctx) {
  std::string str;
  for (uint32_t p = pbegin; p < pbegin + psize; p++) {
    auto pi = ctx->parameters[p];
    str += dump_parameter(pi, ctx);
    if (p < pbegin + psize - 1) {
      str += ",";
    }
  }
  return str;
}

std::string dump_binary_expr(const AstNode& node, DumpContext* ctx) {
  auto lhs = expr_lparen(ctx) + dump_expr(node.expr_binary.left, ctx) + expr_rparen(ctx);
  auto rhs = expr_lparen(ctx) + dump_expr(node.expr_binary.right, ctx) + expr_rparen(ctx);
  return lhs + op_str(node.expr_binary.op) + rhs;
}

std::string dump_identifier_reference_expr(const AstNode& node, DumpContext* ctx) {
  auto& nd = node.expr_identifier_reference;
  auto str = ctx->registry->get(nd.identifier);
  if (nd.subscript_method == SubscriptMethod::None) {
    assert(nd.arg_size == 0);
  } else if (nd.subscript_method == SubscriptMethod::Period) {
    assert(nd.arg_size == 1);
    auto subi = ctx->subscripts[nd.arg_begin];
    str += ".";
    str += dump_expr(subi, ctx);
  } else if (nd.subscript_method == SubscriptMethod::Parens) {
    str += "(";
    for (uint32_t a = nd.arg_begin; a < nd.arg_begin + nd.arg_size; a++) {
      auto argi = ctx->subscripts[a];
      str += dump_expr(argi, ctx);
      if (a < nd.arg_begin + nd.arg_size - 1) {
        str += ",";
      }
    }
    str += ")";
  } else {
    assert(false);
  }
  return str;
}

std::string dump_number_literal_expr(const AstNode& node, DumpContext*) {
  return std::to_string(node.expr_number_literal.value);
}

std::string dump_grouping_expr(const AstNode& node, DumpContext* ctx) {
  return "(" + dump_expr(node.expr_grouping.expr, ctx) + ")";
}

std::string dump_expr(uint32_t ni, DumpContext* ctx) {
  auto& node = ctx->nodes[ni];
  switch (node.type) {
    case AstNodeType::ExprBinary:
      return dump_binary_expr(node, ctx);
    case AstNodeType::ExprIdentifierReference:
      return dump_identifier_reference_expr(node, ctx);
    case AstNodeType::ExprNumberLiteral:
      return dump_number_literal_expr(node, ctx);
    case AstNodeType::ExprGrouping:
      return dump_grouping_expr(node, ctx);
    default:
      assert(false);
      return "";
  }
}

std::string dump_expr_stmt(const AstNode& node, DumpContext* ctx) {
  return tab_str(ctx) + dump_expr(node.stmt_expr.expr, ctx);
}

std::string dump_assign_stmt(const AstNode& node, DumpContext* ctx) {
  auto lhs = dump_expr(node.stmt_assign.lhs, ctx);
  auto rhs = dump_expr(node.stmt_assign.rhs, ctx);
  auto str = tab_str(ctx);
  str += lhs + op_str(node.stmt_assign.method) + rhs;
  return str;
}

std::string dump_stmt_block(uint32_t beg, uint32_t sz, DumpContext* ctx) {
  std::string str;
  for (uint32_t s = beg; s < beg + sz; s++) {
    auto si = ctx->statement_blocks[s];
    str += dump_stmt(si, ctx);
    str += "\n";
  }
  return str;
}

std::string dump_if_stmt(const AstNode& node, DumpContext* ctx) {
  auto str = tab_str(ctx);
  str += "if ";
  auto& if_data = node.stmt_if;
  str += dump_expr(if_data.cond, ctx);
  str += "\n";
  tab_in(ctx);
  str += dump_stmt_block(if_data.block_begin, if_data.block_size, ctx);
  tab_out(ctx);
  if (if_data.else_block_size > 0) {
    str += "else\n";
    tab_in(ctx);
    str += dump_stmt_block(if_data.else_block_begin, if_data.else_block_size, ctx);
    tab_out(ctx);
  }
  str += "end";
  return str;
}

std::string dump_return_stmt(const AstNode& node, DumpContext* ctx) {
  auto str = tab_str(ctx);
  str += "return {";
  auto& ret = node.stmt_return;
  str += ret.match ? "match" : "nomatch";
  str += ",{";
  for (uint32_t s = ret.succ_str_begin; s < ret.succ_str_begin + ret.succ_str_size; s++) {
    uint32_t ni = ctx->module_strings[s];
    auto& mod_node = ctx->nodes[ni];
    if (mod_node.type == ls::AstNodeType::ModuleBranch) {
      str += mod_node.module_branch.out ? "]" : "[";
    } else {
      str += dump_expr(ni, ctx);
    }
    if (s < ret.succ_str_begin + ret.succ_str_size - 1) {
      str += ",";
    }
  }
  str += "},{";
  for (uint32_t s = ret.result_str_begin; s < ret.result_str_begin + ret.result_str_size; s++) {
    uint32_t ni = ctx->module_strings[s];
    auto& mod_node = ctx->nodes[ni];
    if (mod_node.type == ls::AstNodeType::ModuleBranch) {
      str += mod_node.module_branch.out ? "]" : "[";
    } else {
      str += dump_expr(ni, ctx);
    }
    if (s < ret.result_str_begin + ret.result_str_size - 1) {
      str += ",";
    }
  }
  str += "}";
  return str;
}

std::string dump_stmt(uint32_t si, DumpContext* ctx) {
  auto& node = ctx->nodes[si];
  switch (node.type) {
    case AstNodeType::StmtExpr:
      return dump_expr_stmt(node, ctx);
    case AstNodeType::StmtAssign:
      return dump_assign_stmt(node, ctx);
    case AstNodeType::StmtIf:
      return dump_if_stmt(node, ctx);
    case AstNodeType::StmtReturn:
      return dump_return_stmt(node, ctx);
    default:
      assert(false);
      return "";
  }
}

} //  anon

std::string ls::dump_rule(uint32_t ri, DumpContext* ctx) {
  auto& node = ctx->nodes[ri];
  assert(node.type == AstNodeType::Rule);
  auto& rule = node.rule;
  std::string str{"rule ("};
  str += dump_parameters(rule.param_begin, rule.param_size, ctx);
  str += ")\n";
  str += dump_stmt_block(rule.block_begin, rule.block_size, ctx);
  str += "end";
  return str;
}

std::string ls::dump_system(uint32_t sysi, DumpContext* ctx) {
  auto& node = ctx->nodes[sysi];
  assert(node.type == AstNodeType::System);
  auto& sys = node.system;
  std::string str{"system "};
  str += ctx->registry->get(sys.identifier);
  str += "(";
  str += dump_parameters(sys.param_begin, sys.param_size, ctx);
  str += ")\n";
  for (uint32_t r = sys.rule_begin; r < sys.rule_begin + sys.rule_size; r++) {
    str += dump_rule(ctx->rules[r], ctx);
    str += "\n";
  }
  str += "end";
  return str;
}

std::string ls::dump_module_bytes(const uint8_t* data, uint32_t ti, DumpContext* ctx) {
  auto& ty = ctx->type_nodes[ti];
  assert(ty.type == TypeNodeType::Module);
  auto& ty_mod = ty.module;

  std::string str;
  str += ctx->registry->get(ty_mod.name);

  if (ctx->hide_module_contents) {
    return str;
  }

  str += "(\n";
  tab_in(ctx);

  for (uint32_t i = 0; i < ty_mod.field_size; i++) {
    str += tab_str(ctx);
    auto* f = ctx->module_fields.begin() + i + ty_mod.field_begin;
    str += ctx->registry->get(f->name);
    str += ": ";
    auto* fstore = ctx->storage.begin() + f->storage;
    const uint32_t off = fstore->offset;
    uint32_t fti = f->type;
    auto* fty = ctx->type_nodes.begin() + fti;

    switch (fty->type) {
      case TypeNodeType::Scalar: {
        str += ctx->registry->get(fty->scalar.name);
        str += " [";
        if (fstore->size == sizeof(float) && fti == ctx->float_t) {
          float v;
          memcpy(&v, data + off, sizeof(float));
          str += std::to_string(v);
        } else if (fstore->size == sizeof(int32_t) && fti == ctx->int_t) {
          int32_t v;
          memcpy(&v, data + off, sizeof(int32_t));
          str += std::to_string(v);
        } else if (fstore->size == bool_t_size() && fti == ctx->bool_t) {
          static_assert(bool_t_size() == sizeof(int32_t));
          int32_t v;
          memcpy(&v, data + off, sizeof(int32_t));
          assert(v == 0 || v == 1);
          str += v ? "true" : "false";
        } else {
          assert(false);
          str += "<unknown>";
        }
        str += "]";
        break;
      }
      case TypeNodeType::Function: {
        str += "<function>";
        break;
      }
      case TypeNodeType::Module: {
        str += dump_module_bytes(data + off, fti, ctx);
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    str += "\n";
  }

  tab_out(ctx);
  str += tab_str(ctx) + ")";
  return str;
}

DumpContext ls::to_dump_context(const ParseResult& parse_res,
                                const ResolveResult& resolve_res,
                                const StringRegistry* registry) {
  DumpContext ctx;
  ctx.nodes = make_view(parse_res.nodes);
  ctx.type_nodes = make_view(resolve_res.type_nodes);
  ctx.parameters = make_view(parse_res.parameters);
  ctx.subscripts = make_view(parse_res.subscripts);
  ctx.statement_blocks = make_view(parse_res.statement_blocks);
  ctx.module_strings = make_view(parse_res.module_strings);
  ctx.rules = make_view(parse_res.rules);
  ctx.systems = make_view(parse_res.systems);
  ctx.modules = make_view(parse_res.modules);
  ctx.storage = make_view(resolve_res.storage_locations);
  ctx.module_fields = make_view(resolve_res.module_fields);
  ctx.registry = registry;
  ctx.float_t = resolve_res.float_t;
  ctx.int_t = resolve_res.int_t;
  ctx.bool_t = resolve_res.bool_t;
  return ctx;
}

GROVE_NAMESPACE_END
