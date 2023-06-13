#include "compile.hpp"
#include "resolve.hpp"
#include "parse.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

struct CompileContext {
  const CompileParams* params;
  std::vector<uint8_t> instructions;
  bool is_lhs;
};

CompileContext make_compile_context(const CompileParams* params) {
  CompileContext ctx{};
  ctx.params = params;
  return ctx;
}

void add_instruction(CompileContext* ctx, uint8_t inst) {
  ctx->instructions.push_back(inst);
}

template <typename T>
void add_instructions(CompileContext* ctx, T data) {
  size_t beg = ctx->instructions.size();
  for (size_t i = 0; i < sizeof(T); i++) {
    add_instruction(ctx, 0);
  }
  memcpy(ctx->instructions.data() + beg, &data, sizeof(T));
#ifdef GROVE_DEBUG
  T res;
  memcpy(&res, ctx->instructions.data() + beg, sizeof(T));
  assert(res == data);
#endif
}

void add_instruction16(CompileContext* ctx, uint16_t insts) {
  add_instructions(ctx, insts);
}
void add_instruction32(CompileContext* ctx, uint32_t insts) {
  add_instructions(ctx, insts);
}
void add_instruction64(CompileContext* ctx, uint64_t insts) {
  add_instructions(ctx, insts);
}
void add_instructionf(CompileContext* ctx, float v) {
  add_instructions(ctx, v);
}

ForeignFunction* get_foreign_function_ptr(const CompileContext* ctx, StringRef ident, uint32_t si) {
  PendingForeignFunction pend{};
  pend.scope = si;
  pend.identifier = ident;
  auto ptr_it = ctx->params->foreign_functions->find(pend);
  assert(ptr_it != ctx->params->foreign_functions->end() && "No function pointer provided.");
  return ptr_it->second;
}

uint32_t num_instructions(CompileContext* ctx) {
  return uint32_t(ctx->instructions.size());
}
const Scope* get_scope(const CompileContext* ctx, uint32_t si) {
  return ctx->params->scopes.begin() + si;
}
uint32_t get_scope_by_node(const CompileContext* ctx, uint32_t ni) {
  return ctx->params->scopes_by_node->at(ni);
}
uint32_t get_type_by_node(const CompileContext* ctx, uint32_t ni) {
  return ctx->params->types_by_node->at(ni);
}
uint32_t get_block_stmt(const CompileContext* ctx, uint32_t stmti) {
  return ctx->params->statement_blocks[stmti];
}
uint32_t get_module_str(const CompileContext* ctx, uint32_t ri) {
  return ctx->params->module_strings[ri];
}
const AstNode* get_subscript_arg_node(const CompileContext* ctx, uint32_t si) {
  return ctx->params->nodes.begin() + ctx->params->subscripts[si];
}
uint32_t get_subscript_arg(const CompileContext* ctx, uint32_t si) {
  return ctx->params->subscripts[si];
}
const TypeNode* get_type_node(const CompileContext* ctx, uint32_t ti) {
  return ctx->params->type_nodes.begin() + ti;
}
uint32_t get_type_node_ref(const CompileContext* ctx, uint32_t ti) {
  return ctx->params->type_node_refs[ti];
}
const StorageLocation* get_storage(const CompileContext* ctx, uint32_t si) {
  return ctx->params->storage.begin() + si;
}
const ModuleField* get_field(const CompileContext* ctx, uint32_t fi) {
  return ctx->params->module_fields.begin() + fi;
}
uint32_t type_size(const CompileContext* ctx, uint32_t ti) {
  return type_size(ctx->params->type_nodes.begin(), ctx->params->storage.begin(), ti);
}
bool is_branch_module_type(const CompileContext* ctx, uint32_t ti) {
  return ti == ctx->params->branch_in_t || ti == ctx->params->branch_out_t;
}

void compile_stmt(CompileContext* context, uint32_t stmti, uint32_t si);
void compile_expr(CompileContext* context, uint32_t ei, uint32_t si);

void compile_binary_expr(CompileContext* context, const AstNode& node, uint32_t si) {
  compile_expr(context, node.expr_binary.left, si);
  compile_expr(context, node.expr_binary.right, si);

  uint32_t til = get_type_by_node(context, node.expr_binary.left);
  uint32_t tir = get_type_by_node(context, node.expr_binary.right);
  if (til == context->params->v3_t) {
    assert(tir == context->params->v3_t);
    (void) tir;
    add_instruction(context, Instructions::vop);
    add_instruction(context, 3);
  }

  switch (node.expr_binary.op) {
    case TokenType::Asterisk:
      add_instruction(context, Instructions::mulf);
      break;
    case TokenType::Fslash:
      add_instruction(context, Instructions::divf);
      break;
    case TokenType::Plus:
      add_instruction(context, Instructions::addf);
      break;
    case TokenType::Minus:
      add_instruction(context, Instructions::subf);
      break;
    case TokenType::Gt:
      add_instruction(context, Instructions::gtf);
      break;
    case TokenType::Lt:
      add_instruction(context, Instructions::ltf);
      break;
    case TokenType::Ge:
      add_instruction(context, Instructions::gef);
      break;
    case TokenType::Le:
      add_instruction(context, Instructions::lef);
      break;
    case TokenType::EqualEqual:
      add_instruction(context, Instructions::testf);
      break;
    default:
      assert(false);
  }
}

[[maybe_unused]] bool is_module_type(const CompileContext* context, uint32_t si, StringRef ident) {
  auto ti = lookup_type(context->params->scopes.begin(), si, ident);
  return ti && get_type_node(context, ti.value())->type == TypeNodeType::Module;
}

void get_field_reference_offset_size(CompileContext* context,
                                     const AstNode::ExprIdentifierReference& node_data, uint32_t ti,
                                     uint16_t* f_off, uint16_t* f_size) {
  assert(node_data.arg_size == 1);
  auto* arg = get_subscript_arg_node(context, node_data.arg_begin);
  assert(arg->type == AstNodeType::ExprIdentifierReference);
  auto& arg_data = arg->expr_identifier_reference;

  const auto ty = get_type_node(context, ti);
  assert(ty->type == TypeNodeType::Module);
  const auto fi = lookup_field(
    context->params->module_fields.begin(),
    arg_data.identifier,
    ty->module.field_begin,
    ty->module.field_size);
  assert(fi);

  auto* f = get_field(context, fi.value());
  auto* f_store = get_storage(context, f->storage);

  uint16_t nested_off{};
  auto* curr_arg_data = &arg_data;
  while (curr_arg_data->subscript_method != SubscriptMethod::None) {
    //  a.b.c.
    assert(curr_arg_data->arg_size == 1);
    auto* sub_arg = get_subscript_arg_node(context, curr_arg_data->arg_begin);
    assert(sub_arg->type == AstNodeType::ExprIdentifierReference);
    const auto sub_ty = get_type_node(context, f->type);
    assert(sub_ty->type == TypeNodeType::Module);
    const auto sub_fi = lookup_field(
      context->params->module_fields.begin(),
      sub_arg->expr_identifier_reference.identifier,
      sub_ty->module.field_begin,
      sub_ty->module.field_size);

    nested_off += uint16_t(f_store->offset);
    f = get_field(context, sub_fi.value());
    f_store = get_storage(context, f->storage);
    curr_arg_data = &sub_arg->expr_identifier_reference;
  }

  *f_off = uint16_t(f_store->offset + nested_off);
  *f_size = uint16_t(f_store->size);
  assert(*f_size > 0);
}

void compile_identifier_reference_expr(CompileContext* context, const AstNode& node, uint32_t si) {
  auto& node_data = node.expr_identifier_reference;
  auto& ident = node_data.identifier;

  const Variable* var;
  uint32_t var_si;
  bool v_success = lookup_variable(context->params->scopes.begin(), si, ident, &var, &var_si);

  if (!v_success) {
    //  Identifier must be a module constructor.
    assert(is_module_type(context, si, ident));
    assert(node_data.subscript_method == SubscriptMethod::Parens);
    for (uint32_t i = 0; i < node_data.arg_size; i++) {
      compile_expr(context, get_subscript_arg(context, i + node_data.arg_begin), si);
    }
    return;
  }

  assert(v_success);

  const uint8_t inst = context->is_lhs ? Instructions::store : Instructions::load;
  const uint32_t store_off = get_scope(context, var_si)->stack_offset;

  if (node_data.subscript_method == SubscriptMethod::None) {
    auto* v_store = get_storage(context, var->storage);
    const auto v_off = uint16_t(v_store->offset + store_off);
    const auto v_size = uint16_t(v_store->size);
    add_instruction(context, inst);
    add_instruction16(context, v_off);
    add_instruction16(context, v_size);

  } else if (node_data.subscript_method == SubscriptMethod::Period) {
    uint16_t f_off{};
    uint16_t f_size{};
    get_field_reference_offset_size(context, node_data, var->type, &f_off, &f_size);

    const auto* v_store = get_storage(context, var->storage);
    f_off += uint16_t(v_store->offset + store_off);

    add_instruction(context, inst);
    add_instruction16(context, f_off);
    add_instruction16(context, f_size);

  } else if (node_data.subscript_method == SubscriptMethod::Parens) {
    for (uint32_t i = 0; i < node_data.arg_size; i++) {
      auto ai = get_subscript_arg(context, i + node_data.arg_begin);
      compile_expr(context, ai, si);
    }
    auto ty_node = get_type_node(context, var->type);
    assert(ty_node->type == TypeNodeType::Function);
    auto& ty_f = ty_node->function;
    uint32_t arg_sz{};
    for (uint32_t i = 0; i < ty_f.param_size; i++) {
      auto pti = get_type_node_ref(context, i + ty_f.param_begin);
      auto sz = type_size(context, pti);
      assert(sz > 0);
      arg_sz += sz;
    }
    auto rti = get_type_node_ref(context, ty_f.ret_begin);
    uint32_t ret_sz = type_size(context, rti);
    //  Require that ret_sz == 0 implies void_t return type
    assert((rti == context->params->void_t && ret_sz == 0) || ret_sz > 0);
    ForeignFunction* ptr = get_foreign_function_ptr(context, ident, var_si);
    add_instruction(context, Instructions::call);
    add_instruction64(context, (uint64_t) ptr);
    add_instruction16(context, uint16_t(arg_sz));
    add_instruction16(context, uint16_t(ret_sz));

  } else {
    assert(false);
  }
}

void compile_number_literal_expr(CompileContext* ctx, const AstNode& node, uint32_t) {
  add_instruction(ctx, Instructions::constantf);
  add_instructionf(ctx, node.expr_number_literal.value);
}

void compile_expr(CompileContext* context, uint32_t ei, uint32_t si) {
  auto& node = context->params->nodes[ei];
  switch (node.type) {
    case AstNodeType::ExprBinary:
      compile_binary_expr(context, node, si);
      break;
    case AstNodeType::ExprIdentifierReference:
      compile_identifier_reference_expr(context, node, si);
      break;
    case AstNodeType::ExprNumberLiteral:
      compile_number_literal_expr(context, node, si);
      break;
    case AstNodeType::ExprGrouping:
      compile_expr(context, node.expr_grouping.expr, si);
      break;
    default:
      assert(false);
      break;
  }
}

void compile_if_stmt(CompileContext* context, const AstNode& node, uint32_t si) {
  auto& if_data = node.stmt_if;
  compile_expr(context, if_data.cond, si);

  add_instruction(context, Instructions::jump_if);
  const auto if_off = uint16_t(num_instructions(context));
  add_instruction(context, 0);
  add_instruction(context, 0);

  for (uint32_t i = 0; i < if_data.block_size; i++) {
    auto stmti = get_block_stmt(context, i + if_data.block_begin);
    auto block_si = get_scope_by_node(context, stmti);
    compile_stmt(context, stmti, block_si);
  }

  add_instruction(context, Instructions::jump);
  auto jump_off = uint16_t(num_instructions(context));
  add_instruction(context, 0);
  add_instruction(context, 0);

  const auto else_off = uint16_t(num_instructions(context));
  memcpy(context->instructions.data() + if_off, &else_off, sizeof(uint16_t));

  for (uint32_t i = 0; i < if_data.else_block_size; i++) {
    auto stmti = get_block_stmt(context, i + if_data.else_block_begin);
    auto block_si = get_scope_by_node(context, stmti);
    compile_stmt(context, stmti, block_si);
  }

  auto jump_to = uint16_t(num_instructions(context));
  auto* jump_write = context->instructions.data() + jump_off;
  memcpy(jump_write, &jump_to, sizeof(uint16_t));
}

void compile_assign_stmt(CompileContext* context, const AstNode& node, uint32_t si) {
  compile_expr(context, node.stmt_assign.rhs, si);
  context->is_lhs = true;
  compile_expr(context, node.stmt_assign.lhs, si);
  context->is_lhs = false;
}

void compile_expr_stmt(CompileContext* context, const AstNode& node, uint32_t si) {
  compile_expr(context, node.stmt_expr.expr, si);
}

void compile_module_args(CompileContext* context, uint32_t mi, uint32_t si,
                         uint32_t* size, uint32_t* ti) {
  *ti = get_type_by_node(context, mi);
  auto ty = get_type_node(context, *ti);
  assert(ty->type == TypeNodeType::Module);

  if (!is_branch_module_type(context, *ti)) {
    compile_expr(context, mi, si);
  }

  auto* store = get_storage(context, ty->module.storage);
  *size = store->size;
}

void compile_return_module_strs(CompileContext* context, uint32_t si, bool match,
                                uint32_t succ_beg, uint32_t succ_sz,
                                uint32_t res_beg, uint32_t res_sz) {
  uint32_t succ_str_size_bytes{};
  std::vector<uint32_t> succ_str;
  for (uint32_t i = 0; i < succ_sz; i++) {
    auto mi = get_module_str(context, i + succ_beg);
    uint32_t sz;
    uint32_t ti;
    compile_module_args(context, mi, si, &sz, &ti);
    succ_str_size_bytes += sz;
    succ_str.push_back(ti);
  }

  uint32_t ret_str_size_bytes{};
  std::vector<uint32_t> ret_str;
  for (uint32_t i = 0; i < res_sz; i++) {
    auto mi = get_module_str(context, i + res_beg);
    uint32_t sz;
    uint32_t ti;
    compile_module_args(context, mi, si, &sz, &ti);
    ret_str_size_bytes += sz;
    ret_str.push_back(ti);
  }

  add_instruction(context, Instructions::ret);
  add_instruction(context, uint8_t(match));
  add_instruction32(context, succ_str_size_bytes);
  add_instruction32(context, succ_sz);
  add_instruction32(context, ret_str_size_bytes);
  add_instruction32(context, res_sz);

  for (uint32_t ti : succ_str) {
    add_instruction32(context, ti);
  }
  for (uint32_t ti : ret_str) {
    add_instruction32(context, ti);
  }
}

void compile_return_stmt(CompileContext* context, const AstNode& node, uint32_t si) {
  auto& ret_stmt = node.stmt_return;
  compile_return_module_strs(
    context,
    si,
    ret_stmt.match,
    ret_stmt.succ_str_begin,
    ret_stmt.succ_str_size,
    ret_stmt.result_str_begin,
    ret_stmt.result_str_size);
}

void compile_stmt(CompileContext* context, uint32_t stmti, uint32_t si) {
  auto& s = context->params->nodes[stmti];
  switch (s.type) {
    case AstNodeType::StmtIf:
      compile_if_stmt(context, s, si);
      break;
    case AstNodeType::StmtExpr:
      compile_expr_stmt(context, s, si);
      break;
    case AstNodeType::StmtAssign:
      compile_assign_stmt(context, s, si);
      break;
    case AstNodeType::StmtReturn:
      compile_return_stmt(context, s, si);
      break;
    default:
      assert(false);
  }
}

CompileResult to_result(CompileContext* ctx) {
  CompileResult result;
  result.instructions = std::move(ctx->instructions);
  return result;
}

} //  anon

CompileResult ls::compile_rule(const CompileParams* params, uint32_t ri) {
  auto ctx = make_compile_context(params);
  auto& rule_node = params->nodes[ri];
  assert(rule_node.type == AstNodeType::Rule);

  auto si = get_scope_by_node(&ctx, ri);
  for (uint32_t i = 0; i < rule_node.rule.block_size; i++) {
    auto stmti = get_block_stmt(&ctx, i + rule_node.rule.block_begin);
    compile_stmt(&ctx, stmti, si);
  }

  return to_result(&ctx);
}

CompileResult ls::compile_axiom(const CompileParams* params, uint32_t ai) {
  auto ctx = make_compile_context(params);
  auto& axiom_node = params->nodes[ai];
  assert(axiom_node.type == AstNodeType::Axiom);

  auto& axiom = axiom_node.axiom;
  auto si = get_scope_by_node(&ctx, ai);
  compile_return_module_strs(
    &ctx,
    si,
    true,
    axiom.str_begin,
    axiom.str_size,
    0,
    0);
  return to_result(&ctx);
}

CompileParams ls::to_compile_params(const ParseResult& parse_res,
                                    const ResolveResult& resolve_res,
                                    const CompileParams::ForeignFunctions* foreign_funcs) {
  CompileParams p;
  p.nodes = make_view(parse_res.nodes);
  p.type_nodes = make_view(resolve_res.type_nodes);
  p.type_node_refs = make_view(resolve_res.type_node_refs);
  p.storage = make_view(resolve_res.storage_locations);
  p.module_fields = make_view(resolve_res.module_fields);
  p.scopes = make_view(resolve_res.scopes);
  p.statement_blocks = make_view(parse_res.statement_blocks);
  p.subscripts = make_view(parse_res.subscripts);
  p.module_strings = make_view(parse_res.module_strings);
  p.scopes_by_node = &resolve_res.scopes_by_node;
  p.types_by_node = &resolve_res.types_by_node;
  p.foreign_functions = foreign_funcs;
  p.branch_in_t = resolve_res.branch_in_t;
  p.branch_out_t = resolve_res.branch_out_t;
  p.float_t = resolve_res.float_t;
  p.v3_t = resolve_res.v3_t;
  p.void_t = resolve_res.void_t;
  return p;
}

GROVE_NAMESPACE_END
