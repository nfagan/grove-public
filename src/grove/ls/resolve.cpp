#include "resolve.hpp"
#include "parse.hpp"
#include "StringRegistry.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

#define GROVE_TRY_ERR(expr) \
  if (auto err = (expr)) {  \
    return err;             \
  }

namespace {

using namespace ls;

std::string get_type_name(const ResolveContext* ctx, uint32_t ti) {
  auto* node = ctx->type_nodes.data() + ti;
  const auto* reg = ctx->params->registry;
  switch (node->type) {
    case TypeNodeType::Scalar:
      return reg->get(node->scalar.name);
    case TypeNodeType::Module:
      return reg->get(node->module.name);
    case TypeNodeType::Function:
      return "<function>";
    default: {
      assert(false);
      return "";
    }
  }
}

std::string message_duplicate_type_identifier(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Duplicate type identifier: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_unresolved_type_identifier(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Unresolved type identifier: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_unresolved_identifier(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Unresolved identifier: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_unresolved_parameter_type(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Unresolved type for: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_unresolved_meta_type_label(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Unresolved meta type label: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_missing_required_meta_type_field(const ResolveContext* ctx, StringRef label,
                                                     StringRef field) {
  auto label_name = ctx->params->registry->get(label);
  auto field_name = ctx->params->registry->get(field);
  std::string res = std::string{"Module is missing required field \""} + field_name + "\"";
  res += " of meta type \"" + label_name + "\".";
  return res;
}
std::string message_wrong_type_for_meta_type_field(const ResolveContext* ctx, StringRef label,
                                                   StringRef field, uint32_t expected_ti,
                                                   uint32_t received_ti) {
  auto label_name = ctx->params->registry->get(label);
  auto field_name = ctx->params->registry->get(field);
  std::string res = std::string{"Field \""} + field_name + "\" of meta type \"" + label_name + "\"";
  res += " must be of type " + get_type_name(ctx, expected_ti);
  res += "; was " + get_type_name(ctx, received_ti) + ".";
  return res;
}
std::string message_duplicate_identifier(const ResolveContext* ctx, StringRef ident) {
  return std::string{"Duplicate identifier: \""} + ctx->params->registry->get(ident) + "\".";
}
std::string message_variable_decl_must_be_simple_identifier() {
  return "Declared variable must be an identifier with no subscripts.";
}
std::string message_type_mismatch(const ResolveContext* ctx, uint32_t tia, uint32_t tib) {
  //  @TODO, type representation.
  auto name_a = get_type_name(ctx, tia);
  auto name_b = get_type_name(ctx, tib);
  return std::string{"Type mismatch: "} + name_a + " != " + name_b;
}
std::string message_dot_subscript_into_non_module_type() {
  return "Dot subscript into non-module type.";
}
std::string message_parens_subscript_into_non_function_type() {
  return "Parens subscript into non-function type.";
}
std::string message_wrong_number_of_arguments() {
  return "Incorrect number of arguments.";
}
std::string message_non_module_return_str() {
  return "Non-module return string.";
}
std::string message_empty_return_str() {
  return "Return string cannot be empty.";
}
std::string message_not_all_paths_return() {
  return "All control paths in a rule must have a return statement.";
}
std::string message_return_str_starts_with_branch() {
  return "Return string cannot begin with a branch.";
}
std::string message_condition_must_be_bool() {
  return "Condition must be bool.";
}
std::string message_cyclic_module_definition() {
  return "Module definitions cannot contain cycles.";
}
std::string message_non_float_arg_to_binary_expr() {
  return "Arguments to binary expression must be float.";
}

StringRef register_string(ResolveContext* ctx, const std::string& s) {
  return ctx->params->registry->emplace(s);
}

TypeID next_type_id(ResolveContext* ctx) {
  return ctx->params->type_ids->next_type_id();
}

ResolveError make_error(std::string&& msg, uint32_t token) {
  ResolveError res;
  res.message = std::move(msg);
  res.token = token;
  return res;
}

Optional<ResolveError> make_opt_error(std::string&& msg, uint32_t token) {
  return Optional<ResolveError>(make_error(std::move(msg), token));
}

void add_error(ResolveContext* ctx, ResolveError&& err) {
  ctx->result->errors.push_back(std::move(err));
}

Scope make_scope(uint32_t parent) {
  Scope scope{};
  scope.parent = parent;
  return scope;
}

uint32_t add_scope(ResolveContext* ctx, Scope&& scope) {
  auto si = uint32_t(ctx->scopes.size());
  ctx->scopes.push_back(std::move(scope));
  return si;
}

uint32_t add_type(ResolveContext* ctx, TypeNode node) {
  auto ti = uint32_t(ctx->type_nodes.size());
  ctx->type_nodes.push_back(node);
  return ti;
}

void add_pending_foreign_function(ResolveContext* ctx, const PendingForeignFunction& func) {
  ctx->pending_foreign_functions.insert(func);
}

void add_scope_by_node(ResolveContext* ctx, uint32_t ni, uint32_t si) {
  assert(ctx->scopes_by_node.count(ni) == 0);
  ctx->scopes_by_node[ni] = si;
}

uint32_t get_scope_by_node(ResolveContext* ctx, uint32_t ni) {
  return ctx->scopes_by_node.at(ni);
}

void add_type_by_node(ResolveContext* ctx, uint32_t ni, uint32_t ti) {
  assert(ctx->types_by_node.count(ni) == 0);
  ctx->types_by_node[ni] = ti;
}

uint32_t reserve_storage(ResolveContext* ctx) {
  auto si = uint32_t(ctx->storage_locations.size());
  ctx->storage_locations.emplace_back();
  return si;
}

uint32_t reserve_fields(ResolveContext* ctx, uint32_t num_fields) {
  auto fi = uint32_t(ctx->module_fields.size());
  ctx->module_fields.resize(ctx->module_fields.size() + num_fields);
  return fi;
}

uint32_t num_type_nodes(ResolveContext* ctx) {
  return uint32_t(ctx->type_nodes.size());
}

StorageLocation* get_storage(ResolveContext* ctx, uint32_t si) {
  return ctx->storage_locations.data() + si;
}
ModuleField* get_module_field(ResolveContext* ctx, uint32_t fi) {
  return ctx->module_fields.data() + fi;
}
TypeNode* get_type_node(ResolveContext* ctx, uint32_t ti) {
  return ctx->type_nodes.data() + ti;
}
const TypeNode* get_type_node(const ResolveContext* ctx, uint32_t ti) {
  return ctx->type_nodes.data() + ti;
}
const AstNode* get_parameter_node(const ResolveContext* ctx, uint32_t pi) {
  auto* node = ctx->params->nodes.begin() + ctx->params->parameters[pi];
  assert(node->type == AstNodeType::Parameter);
  return node;
}
const AstNode* get_ast_node(const ResolveContext* ctx, uint32_t ni) {
  return ctx->params->nodes.begin() + ni;
}
const AstNode* get_module_meta_type_label_node(const ResolveContext* ctx, uint32_t li) {
  auto* node = ctx->params->nodes.begin() + ctx->params->module_meta_type_labels[li];
  assert(node->type == AstNodeType::ModuleMetaTypeLabel);
  return node;
}
uint32_t get_subscript_arg(const ResolveContext* ctx, uint32_t si) {
  return ctx->params->subscripts[si];
}
const AstNode* get_subscript_arg_node(ResolveContext* ctx, uint32_t si) {
  return ctx->params->nodes.begin() + ctx->params->subscripts[si];
}
uint32_t get_type_node_ref(const ResolveContext* ctx, uint32_t pi) {
  return ctx->type_node_refs[pi];
}
uint32_t get_block_stmt(const ResolveContext* ctx, uint32_t stmti) {
  return ctx->params->statement_blocks[stmti];
}
uint32_t get_module_str(const ResolveContext* ctx, uint32_t ri) {
  return ctx->params->module_strings[ri];
}
bool is_return_stmt(const ResolveContext* ctx, uint32_t ni) {
  return get_ast_node(ctx, ni)->type == AstNodeType::StmtReturn;
}

PendingForeignFunction make_pending_foreign_function(StringRef ident, uint32_t si, uint32_t ti) {
  PendingForeignFunction res{};
  res.identifier = ident;
  res.scope = si;
  res.type_index = ti;
  return res;
}

Variable make_variable(uint32_t ti, uint32_t si) {
  Variable var;
  var.type = ti;
  var.storage = si;
  return var;
}

TypeNode make_scalar_type(TypeID id, StringRef name, uint32_t storage) {
  TypeNode res;
  res.type = TypeNodeType::Scalar;
  res.scalar = {};
  res.scalar.id = id;
  res.scalar.name = name;
  res.scalar.storage = storage;
  return res;
}

TypeNode make_function_type(TypeID id, uint32_t param_beg, uint32_t param_sz, uint32_t ret_beg) {
  TypeNode res;
  res.type = TypeNodeType::Function;
  res.function = {};
  res.function.id = id;
  res.function.param_begin = param_beg;
  res.function.param_size = param_sz;
  res.function.ret_begin = ret_beg;
  return res;
}

TypeNode make_module_type(TypeID id, StringRef name, uint32_t storage,
                          uint32_t field_begin, uint32_t field_size,
                          uint32_t meta_type_begin = 0, uint32_t meta_type_size = 0) {
  TypeNode res;
  res.type = TypeNodeType::Module;
  res.module = {};
  res.module.id = id;
  res.module.name = name;
  res.module.storage = storage;
  res.module.field_begin = field_begin;
  res.module.field_size = field_size;
  res.module.meta_type_begin = meta_type_begin;
  res.module.meta_type_size = meta_type_size;
  return res;
}

TypeNode make_module_meta_type(StringRef name) {
  TypeNode res;
  res.type = TypeNodeType::ModuleMetaType;
  res.module_meta_type = {};
  res.module_meta_type.name = name;
  return res;
}

TypeNode placeholder_type_node() {
  return TypeNode{};
}

uint32_t type_size(const ResolveContext* ctx, uint32_t ti) {
  return type_size(ctx->type_nodes.data(), ctx->storage_locations.data(), ti);
}

uint32_t compute_type_size(ResolveContext* ctx, uint32_t ti) {
  //  @NOTE: This procedure assumes all types have been created and assigned to module fields.
  auto& t = *get_type_node(ctx, ti);
  switch (t.type) {
    case TypeNodeType::Scalar: {
      uint32_t size = get_storage(ctx, t.scalar.storage)->size;
      assert(size > 0);
      return size;
    }
    case TypeNodeType::Function: {
      return function_ptr_size();
    }
    case TypeNodeType::Module: {
      uint32_t s{};
      for (uint32_t i = 0; i < t.module.field_size; i++) {
        auto* field = get_module_field(ctx, i + t.module.field_begin);
        s += compute_type_size(ctx, field->type);
      }
      return s;
    }
    default: {
      assert(false);
      return 0;
    }
  }
}

Optional<uint32_t> register_type(ResolveContext* ctx, uint32_t si,
                                 StringRef type_name, TypeNode type) {
  auto& scope = ctx->scopes[si];
  if (scope.types.count(type_name)) {
    return NullOpt{};
  } else {
    auto ti = add_type(ctx, type);
    scope.types[type_name] = ti;
    return Optional<uint32_t>(ti);
  }
}

[[maybe_unused]] bool is_empty_module_type(ResolveContext* ctx, uint32_t ti) {
  auto* ty = get_type_node(ctx, ti);
  return ty->type == TypeNodeType::Module && ty->module.field_size == 0;
}

bool register_storage_location(ResolveContext* ctx, uint32_t si, StringRef var_name, uint32_t ti) {
  auto& scope = ctx->scopes[si];
  if (scope.variables.count(var_name)) {
    return false;
  } else {
    auto ts = type_size(ctx, ti);
    assert(is_empty_module_type(ctx, ti) || ts > 0);
    auto storei = reserve_storage(ctx);
    auto var = make_variable(ti, storei);
    scope.variables[var_name] = var;
    const uint32_t off = scope.stack_size;
    auto* store = get_storage(ctx, storei);
    store->size = ts;
    store->offset = off;
    scope.stack_size += ts;
    return true;
  }
}

uint32_t reserve_module_fields(ResolveContext* ctx, uint32_t field_size) {
  auto fi = reserve_fields(ctx, field_size);
  for (uint32_t i = 0; i < field_size; i++) {
    get_module_field(ctx, fi + i)->storage = reserve_storage(ctx);
  }
  return fi;
}

Optional<uint32_t> add_named_scalar_type(ResolveContext* ctx, uint32_t scope,
                                         const char* name, uint32_t size) {
  auto store = reserve_storage(ctx);
  auto name_ref = register_string(ctx, name);
  auto type_id = next_type_id(ctx);
  auto t = make_scalar_type(type_id, name_ref, store);
  get_storage(ctx, store)->size = size;
  return register_type(ctx, scope, name_ref, t);
}

Optional<uint32_t> add_v3_type(ResolveContext* ctx, uint32_t scope, uint32_t float_t,
                               uint32_t float_size) {
  auto ident = register_string(ctx, "v3");
  uint32_t ti;
  if (auto ind = register_type(ctx, scope, ident, placeholder_type_node())) {
    ti = ind.value();
  } else {
    return NullOpt{};
  }

  const uint32_t num_fields = 3;
  uint32_t field_beg = reserve_module_fields(ctx, num_fields);
  uint32_t storei = reserve_storage(ctx);
  *get_type_node(ctx, ti) = make_module_type(next_type_id(ctx), ident, storei, field_beg, num_fields);

  const char* field_names[3] = {"x", "y", "z"};
  uint32_t off{};
  for (uint32_t i = 0; i < num_fields; i++) {
    auto* f = get_module_field(ctx, i + field_beg);
    f->name = register_string(ctx, field_names[i]);
    f->type = float_t;
    auto* f_store = get_storage(ctx, f->storage);
    f_store->size = float_size;
    f_store->offset = off;
    off += float_size;
  }

  get_storage(ctx, storei)->size = off;
  return Optional<uint32_t>(ti);
}

Optional<uint32_t> add_named_empty_module_type(ResolveContext* ctx, uint32_t scope,
                                               const char* name) {
  auto ident = register_string(ctx, name);
  uint32_t ti;
  if (auto ind = register_type(ctx, scope, ident, placeholder_type_node())) {
    ti = ind.value();
  } else {
    return NullOpt{};
  }

  uint32_t storei = reserve_storage(ctx);
  *get_type_node(ctx, ti) = make_module_type(next_type_id(ctx), ident, storei, 0, 0);
  get_storage(ctx, storei)->size = 0;
  return Optional<uint32_t>(ti);
}

Optional<uint32_t> lookup_type(ResolveContext* ctx, uint32_t si, StringRef name) {
  return lookup_type(ctx->scopes.data(), si, name);
}

Optional<uint32_t> lookup_field(ResolveContext* ctx, StringRef name,
                                uint32_t f_beg, uint32_t f_size) {
  return lookup_field(ctx->module_fields.data(), name, f_beg, f_size);
}

bool lookup_variable(ResolveContext* ctx, uint32_t si, StringRef name,
                     Variable** var, uint32_t* in_scope) {
  return lookup_variable(ctx->scopes.data(), si, name, var, in_scope);
}

uint32_t get_module_type(ResolveContext* ctx, uint32_t mi, uint32_t si) {
  //  Only valid after declaring modules.
  auto& mod = ctx->params->nodes[mi];
  assert(mod.type == AstNodeType::Module);
  auto mod_ti = lookup_type(ctx, si, mod.module.identifier);
  assert(mod_ti);
  return mod_ti.value();
}

Optional<uint32_t> require_type(ResolveContext* ctx, uint32_t si, uint32_t ni);

Optional<uint32_t> require_function_type(ResolveContext* ctx, uint32_t si, const AstNode& node) {
  std::vector<uint32_t> param_ts;
  auto& func = node.type_function;
  for (uint32_t i = 0; i < func.param_size; i++) {
    if (auto pti = require_type(ctx, si, i + func.param_begin)) {
      param_ts.push_back(pti.value());
    } else {
      return NullOpt{};
    }
  }
  if (auto rti = require_type(ctx, si, func.ret_begin)) {
    param_ts.push_back(rti.value());
  } else {
    return NullOpt{};
  }

  auto param_beg = uint32_t(ctx->type_node_refs.size());
  auto param_sz = func.param_size;
  assert(param_sz + 1 == param_ts.size());  //  +1 for return type
  for (uint32_t p : param_ts) {
    ctx->type_node_refs.push_back(p);
  }

  auto ty = make_function_type(next_type_id(ctx), param_beg, param_sz, param_beg + param_sz);
  return Optional<uint32_t>(add_type(ctx, ty));
}

Optional<uint32_t> require_type(ResolveContext* ctx, uint32_t si, uint32_t ni) {
  auto& node = ctx->params->nodes[ni];
  switch (node.type) {
    case AstNodeType::TypeIdentifier:
      return lookup_type(ctx, si, node.type_identifier.identifier);
    case AstNodeType::TypeFunction:
      return require_function_type(ctx, si, node);
    default:
      assert(false);
      return NullOpt{};
  }
}

bool add_base_types(ResolveContext* ctx, uint32_t scope) {
  if (auto ti = add_named_empty_module_type(ctx, scope, "<null>")) {
    //  @HACK: By first adding a type with a name we can't reference, we ensure any valid type index
    //  will be > 0.
    assert(ti.value() == 0);
    (void) ti;
  } else {
    assert(false);
    return false;
  }
  if (auto ti = add_named_empty_module_type(ctx, scope, "void")) {
    ctx->void_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_named_empty_module_type(ctx, scope, "[*")) {
    ctx->branch_in_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_named_empty_module_type(ctx, scope, "*]")) {
    ctx->branch_out_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_named_scalar_type(ctx, scope, "float", sizeof(float))) {
    ctx->float_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_named_scalar_type(ctx, scope, "int", sizeof(int32_t))) {
    ctx->int_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_named_scalar_type(ctx, scope, "bool", sizeof(int32_t))) { //  @note
    ctx->bool_t = ti.value();
  } else {
    return false;
  }
  if (auto ti = add_v3_type(ctx, scope, ctx->float_t, sizeof(float))) {
    ctx->v3_t = ti.value();
  } else {
    return false;
  }
  return true;
}

bool add_base_constants(ResolveContext* ctx, uint32_t scope) {
  if (!register_storage_location(ctx, scope, register_string(ctx, "true"), ctx->bool_t)) {
    return false;
  }
  if (!register_storage_location(ctx, scope, register_string(ctx, "false"), ctx->bool_t)) {
    return false;
  }
  return true;
}

bool type_equality(const ResolveContext* ctx, uint32_t tia, uint32_t tib) {
  auto* ty1 = get_type_node(ctx, tia);
  auto* ty2 = get_type_node(ctx, tib);
  if (ty1->type != ty2->type) {
    return false;
  }
  switch (ty1->type) {
    case TypeNodeType::Scalar:
      return ty1->scalar.id == ty2->scalar.id;
    case TypeNodeType::Module:
      return ty1->module.id == ty2->module.id;
    case TypeNodeType::Function: {
      auto& ty1f = ty1->function;
      auto& ty2f = ty2->function;
      if (ty1f.param_size != ty2f.param_size) {
        return false;
      }
      for (uint32_t i = 0; i < ty1f.param_size; i++) {
        uint32_t ref1 = get_type_node_ref(ctx, ty1f.param_begin + i);
        uint32_t ref2 = get_type_node_ref(ctx, ty2f.param_begin + i);
        if (!type_equality(ctx, ref1, ref2)) {
          return false;
        }
      }
      uint32_t ret_ref1 = get_type_node_ref(ctx, ty1f.ret_begin);
      uint32_t ret_ref2 = get_type_node_ref(ctx, ty2f.ret_begin);
      if (!type_equality(ctx, ret_ref1, ret_ref2)) {
        return false;
      }
      return true;
    }
    default:
      assert(false);
      return false;
  }
}

Optional<ResolveError> resolve_stmt(ResolveContext* ctx, uint32_t stmti, uint32_t si);
Optional<ResolveError> resolve_expr(ResolveContext* ctx, uint32_t ei,
                                    uint32_t parent_scope, uint32_t* ti);

Optional<ResolveError> resolve_binary_expr(ResolveContext* ctx, const AstNode& node,
                                           uint32_t si, uint32_t* ti) {
  uint32_t ti_lhs;
  GROVE_TRY_ERR(resolve_expr(ctx, node.expr_binary.left, si, &ti_lhs))
  uint32_t ti_rhs;
  GROVE_TRY_ERR(resolve_expr(ctx, node.expr_binary.right, si, &ti_rhs))
  if (!type_equality(ctx, ti_lhs, ti_rhs)) {
    return make_opt_error(message_type_mismatch(ctx, ti_lhs, ti_rhs), node.token);
  }

  add_type_by_node(ctx, node.expr_binary.left, ti_lhs);
  add_type_by_node(ctx, node.expr_binary.right, ti_rhs);

  switch (node.expr_binary.op) {
    case TokenType::Asterisk:
    case TokenType::Fslash:
    case TokenType::Plus:
    case TokenType::Minus: {
      if (ti_lhs != ctx->float_t && ti_lhs != ctx->v3_t) {
        return make_opt_error(message_non_float_arg_to_binary_expr(), node.token);
      }
      *ti = ti_lhs;
      break;
    }
    case TokenType::Gt:
    case TokenType::Lt:
    case TokenType::Ge:
    case TokenType::Le:
    case TokenType::EqualEqual:
      *ti = ctx->bool_t;
      break;
    default:
      assert(false);
      *ti = ~0u;
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_module_str(ResolveContext* ctx,
                                          const AstNode& ret, uint32_t si, uint32_t* ti);

Optional<ResolveError> resolve_field_reference(ResolveContext* ctx, const AstNode& parent_node,
                                               uint32_t module_ti,
                                               const AstNode::ExprIdentifierReference& ref,
                                               uint32_t* ti) {
  assert(ref.arg_size == 1);
  auto* ty_node = get_type_node(ctx, module_ti);
  if (ty_node->type != TypeNodeType::Module) {
    return make_opt_error(message_dot_subscript_into_non_module_type(), parent_node.token);
  }

  auto& ty_mod = ty_node->module;
  auto* arg = get_subscript_arg_node(ctx, ref.arg_begin);
  assert(arg->type == AstNodeType::ExprIdentifierReference);

  auto& arg_ref = arg->expr_identifier_reference;
  auto& arg_ident = arg_ref.identifier;
  auto fi = lookup_field(ctx, arg_ident, ty_mod.field_begin, ty_mod.field_size);
  if (!fi) {
    return make_opt_error(message_unresolved_identifier(ctx, arg_ident), arg->token);
  }

  auto* field = get_module_field(ctx, fi.value());
  if (arg_ref.subscript_method != SubscriptMethod::None) {
    assert(arg_ref.arg_size == 1);
    return resolve_field_reference(ctx, *arg, field->type, arg_ref, ti);
  }

  *ti = field->type;
  return NullOpt{};
}

Optional<ResolveError> resolve_identifier_reference_expr(ResolveContext* ctx, const AstNode& node,
                                                         uint32_t si, uint32_t* ti) {
  Variable* var;
  uint32_t var_si;
  auto& ident_info = node.expr_identifier_reference;
  if (!lookup_variable(ctx, si, ident_info.identifier, &var, &var_si)) {
    //  Check if a module reference.
    if (auto mod_ti = lookup_type(ctx, si, ident_info.identifier)) {
      (void) mod_ti;
      return resolve_module_str(ctx, node, si, ti);
    }
    return make_opt_error(message_unresolved_identifier(ctx, ident_info.identifier), node.token);
  }
  if (ident_info.subscript_method == SubscriptMethod::None) {
    assert(ident_info.arg_size == 0);
    *ti = var->type;

  } else if (ident_info.subscript_method == SubscriptMethod::Period) {
    return resolve_field_reference(ctx, node, var->type, ident_info, ti);

  } else {
    assert(ident_info.subscript_method == SubscriptMethod::Parens);
    auto* ty_node = get_type_node(ctx, var->type);
    if (ty_node->type != TypeNodeType::Function) {
      return make_opt_error(message_parens_subscript_into_non_function_type(), node.token);
    }

    auto& ty_f = ty_node->function;
    if (ty_f.param_size != ident_info.arg_size) {
      return make_opt_error(message_wrong_number_of_arguments(), node.token);
    }

    for (uint32_t i = 0; i < ident_info.arg_size; i++) {
      uint32_t arg_ti;
      uint32_t argi = get_subscript_arg(ctx, ident_info.arg_begin + i);
      auto* arg = get_subscript_arg_node(ctx, ident_info.arg_begin + i);
      GROVE_TRY_ERR(resolve_expr(ctx, argi, si, &arg_ti))
      auto expect_ti = get_type_node_ref(ctx, ty_f.param_begin + i);
      if (!type_equality(ctx, arg_ti, expect_ti)) {
        return make_opt_error(message_type_mismatch(ctx, arg_ti, expect_ti), arg->token);
      }
    }

    //  @NOTE: use variable scope.
    auto func = make_pending_foreign_function(ident_info.identifier, var_si, var->type);
    add_pending_foreign_function(ctx, func);
    *ti = get_type_node_ref(ctx, ty_f.ret_begin);
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_number_literal_expr(ResolveContext* ctx, const AstNode&,
                                                   uint32_t, uint32_t* ti) {
  *ti = ctx->float_t;
  return NullOpt{};
}

Optional<ResolveError> resolve_expr(ResolveContext* ctx, uint32_t ei, uint32_t si, uint32_t* ti) {
  auto& n = ctx->params->nodes[ei];
  switch (n.type) {
    case AstNodeType::ExprBinary:
      return resolve_binary_expr(ctx, n, si, ti);
    case AstNodeType::ExprIdentifierReference:
      return resolve_identifier_reference_expr(ctx, n, si, ti);
    case AstNodeType::ExprNumberLiteral:
      return resolve_number_literal_expr(ctx, n, si, ti);
    case AstNodeType::ExprGrouping:
      return resolve_expr(ctx, n.expr_grouping.expr, si, ti);
    default:
      assert(false);
      return NullOpt{};
  }
}

Optional<ResolveError> resolve_expr_stmt(ResolveContext* ctx, const AstNode& node, uint32_t si) {
  auto& stmt = node.stmt_expr;
  uint32_t ti;
  GROVE_TRY_ERR(resolve_expr(ctx, stmt.expr, si, &ti))
  (void) ti;
  return NullOpt{};
}

Optional<ResolveError> resolve_assign_stmt(ResolveContext* ctx, const AstNode& node, uint32_t si) {
  auto& stmt = node.stmt_assign;
  uint32_t ti_rhs;
  GROVE_TRY_ERR(resolve_expr(ctx, stmt.rhs, si, &ti_rhs))
  if (stmt.method == TokenType::Define) {
    auto& lhs_node = ctx->params->nodes[stmt.lhs];
    assert(lhs_node.type == AstNodeType::ExprIdentifierReference);
    if (lhs_node.expr_identifier_reference.subscript_method != SubscriptMethod::None) {
      return make_opt_error(message_variable_decl_must_be_simple_identifier(), lhs_node.token);
    }

    auto name = lhs_node.expr_identifier_reference.identifier;
    if (!register_storage_location(ctx, si, name, ti_rhs)) {
      return make_opt_error(message_duplicate_identifier(ctx, name), lhs_node.token);
    }
  } else {
    assert(stmt.method == TokenType::Equal);
    uint32_t ti_lhs;
    GROVE_TRY_ERR(resolve_expr(ctx, stmt.lhs, si, &ti_lhs))
    if (!type_equality(ctx, ti_lhs, ti_rhs)) {
      return make_opt_error(message_type_mismatch(ctx, ti_lhs, ti_rhs), node.token);
    }
  }
  return NullOpt{};
}

Optional<ResolveError> resolve_if_stmt(ResolveContext* ctx, const AstNode& node, uint32_t si) {
  uint32_t condti;
  auto& if_data = node.stmt_if;
  GROVE_TRY_ERR(resolve_expr(ctx, if_data.cond, si, &condti))
  if (condti != ctx->bool_t) {
    return make_opt_error(message_condition_must_be_bool(), node.token);
  }

  auto block_si = add_scope(ctx, make_scope(si));
  bool if_block_returns{};
  for (uint32_t i = 0; i < if_data.block_size; i++) {
    auto stmti = get_block_stmt(ctx, i + if_data.block_begin);
    add_scope_by_node(ctx, stmti, block_si);
    GROVE_TRY_ERR(resolve_stmt(ctx, stmti, block_si))
    if_block_returns |= is_return_stmt(ctx, stmti);
  }
  if_block_returns |= ctx->scopes[block_si].all_sub_paths_return;

  bool else_block_returns{};
  if (if_data.else_block_size > 0) {
    auto else_si = add_scope(ctx, make_scope(si));
    for (uint32_t i = 0; i < if_data.else_block_size; i++) {
      auto stmti = get_block_stmt(ctx, i + if_data.else_block_begin);
      add_scope_by_node(ctx, stmti, else_si);
      GROVE_TRY_ERR(resolve_stmt(ctx, stmti, else_si))
      else_block_returns |= is_return_stmt(ctx, stmti);
    }
    else_block_returns |= ctx->scopes[else_si].all_sub_paths_return;
  }

  if (if_block_returns && else_block_returns) {
    ctx->scopes[si].all_sub_paths_return = true;
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_module_str(ResolveContext* ctx,
                                          const AstNode& ret, uint32_t si, uint32_t* ti) {
  if (ret.type != AstNodeType::ExprIdentifierReference ||
      ret.expr_identifier_reference.subscript_method != SubscriptMethod::Parens) {
    return make_opt_error(message_non_module_return_str(), ret.token);
  }

  auto& ret_data = ret.expr_identifier_reference;
  auto mod_ident = ret_data.identifier;
  auto mod_ti = lookup_type(ctx, si, mod_ident);
  if (!mod_ti) {
    return make_opt_error(message_unresolved_type_identifier(ctx, mod_ident), ret.token);
  }
  auto* mod_ty = get_type_node(ctx, mod_ti.value());
  if (mod_ty->type != TypeNodeType::Module) {
    return make_opt_error(message_non_module_return_str(), ret.token);
  }
  if (mod_ty->module.field_size != ret_data.arg_size) {
    return make_opt_error(message_wrong_number_of_arguments(), ret.token);
  }
  for (uint32_t j = 0; j < mod_ty->module.field_size; j++) {
    uint32_t arg_ti;
    auto argi = get_subscript_arg(ctx, j + ret_data.arg_begin);
    GROVE_TRY_ERR(resolve_expr(ctx, argi, si, &arg_ti))
    auto* field = get_module_field(ctx, mod_ty->module.field_begin + j);
    if (!type_equality(ctx, field->type, arg_ti)) {
      return make_opt_error(message_type_mismatch(ctx, field->type, arg_ti), ret.token);
    }
  }
  *ti = mod_ti.value();
  return NullOpt{};
}

Optional<ResolveError> resolve_return_stmt(ResolveContext* ctx, const AstNode& node, uint32_t si) {
  auto& ret_stmt = node.stmt_return;

  if (ret_stmt.succ_str_size == 0) {
    return make_opt_error(message_empty_return_str(), node.token);
  }

  for (uint32_t i = 0; i < ret_stmt.succ_str_size; i++) {
    auto ri = get_module_str(ctx, i + ret_stmt.succ_str_begin);
    auto& mod_node = ctx->params->nodes[ri];

    uint32_t mod_ti;
    if (mod_node.type == AstNodeType::ModuleBranch) {
      if (i == 0) {
        //  Disallow return {match, {[x]}}
        //  Require return {match, {I, [x]}}
        return make_opt_error(message_return_str_starts_with_branch(), mod_node.token);
      }
      mod_ti = mod_node.module_branch.out ? ctx->branch_out_t : ctx->branch_in_t;
    } else {
      GROVE_TRY_ERR(resolve_expr(ctx, ri, si, &mod_ti))
      if (get_type_node(ctx, mod_ti)->type != TypeNodeType::Module) {
        return make_opt_error(message_non_module_return_str(), ctx->params->nodes[ri].token);
      }
    }
    add_type_by_node(ctx, ri, mod_ti);
  }

  for (uint32_t i = 0; i < ret_stmt.result_str_size; i++) {
    auto ri = get_module_str(ctx, i + ret_stmt.result_str_begin);
    auto& mod_node = ctx->params->nodes[ri];

    uint32_t mod_ti;
    if (mod_node.type == AstNodeType::ModuleBranch) {
      if (i == 0) {
        //  Disallow return {match, {[x]}}
        //  Require return {match, {I, [x]}}
        return make_opt_error(message_return_str_starts_with_branch(), mod_node.token);
      }
      mod_ti = mod_node.module_branch.out ? ctx->branch_out_t : ctx->branch_in_t;
    } else {
      GROVE_TRY_ERR(resolve_expr(ctx, ri, si, &mod_ti))
      if (get_type_node(ctx, mod_ti)->type != TypeNodeType::Module) {
        return make_opt_error(message_non_module_return_str(), ctx->params->nodes[ri].token);
      }
    }
    add_type_by_node(ctx, ri, mod_ti);
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_stmt(ResolveContext* ctx, uint32_t stmti, uint32_t parent_scope) {
  auto& s = ctx->params->nodes[stmti];
  switch (s.type) {
    case AstNodeType::StmtExpr:
      return resolve_expr_stmt(ctx, s, parent_scope);
    case AstNodeType::StmtAssign:
      return resolve_assign_stmt(ctx, s, parent_scope);
    case AstNodeType::StmtIf:
      return resolve_if_stmt(ctx, s, parent_scope);
    case AstNodeType::StmtReturn:
      return resolve_return_stmt(ctx, s, parent_scope);
    default:
      assert(false);
      return NullOpt{};
  }
}

Optional<ResolveError> push_module_meta_types(ResolveContext* ctx, uint32_t mi,
                                              uint32_t* meta_begin, uint32_t* meta_end) {
  auto& mod = ctx->params->nodes[mi];
  assert(mod.type == AstNodeType::Module);

  *meta_begin = num_type_nodes(ctx);
  for (uint32_t i = 0; i < mod.module.meta_type_label_size; i++) {
    auto* label_node = get_module_meta_type_label_node(ctx, i + mod.module.meta_type_label_begin);
    StringRef label_name = label_node->module_meta_type_label.identifier;

    const ModuleDescriptor* meta_type_desc{};
    for (auto& desc : ctx->params->module_meta_types) {
      if (desc.name == label_name) {
        meta_type_desc = &desc;
        break;
      }
    }

    if (!meta_type_desc) {
      auto msg = message_unresolved_meta_type_label(ctx, label_name);
      return make_opt_error(std::move(msg), label_node->token);
    } else {
      add_type(ctx, make_module_meta_type(label_name));
    }
  }

  *meta_end = num_type_nodes(ctx);
  return NullOpt{};
}

Optional<ResolveError> declare_module(ResolveContext* ctx, uint32_t mi, uint32_t si) {
  auto& mod = ctx->params->nodes[mi];
  assert(mod.type == AstNodeType::Module);
  auto& module = mod.module;

  std::unordered_set<StringRef, StringRef::Hash> fields;
  for (uint32_t pi = module.param_begin; pi < module.param_begin + module.param_size; pi++) {
    auto& node = *get_parameter_node(ctx, pi);
    auto& param = node.parameter;
    if (fields.count(param.identifier)) {
      return make_opt_error(message_duplicate_identifier(ctx, param.identifier), node.token);
    } else {
      fields.insert(param.identifier);
    }
  }

  uint32_t ti;
  if (auto ind = register_type(ctx, si, module.identifier, placeholder_type_node())) {
    ti = ind.value();
  } else {
    return make_opt_error(message_duplicate_type_identifier(ctx, module.identifier), mod.token);
  }

  uint32_t meta_type_begin{};
  uint32_t meta_type_end{};
  if (auto err = push_module_meta_types(ctx, mi, &meta_type_begin, &meta_type_end)) {
    return err;
  }

  uint32_t field_size = module.param_size;
  uint32_t field_beg = reserve_module_fields(ctx, field_size);
  uint32_t storei = reserve_storage(ctx);

  auto ty = make_module_type(
    next_type_id(ctx), module.identifier, storei, field_beg, field_size,
    meta_type_begin, meta_type_end - meta_type_begin);
  *get_type_node(ctx, ti) = ty;
  return NullOpt{};
}

Optional<ResolveError> set_proposed_module_field_types(ResolveContext* ctx, uint32_t mi, uint32_t si) {
  //  Assign types to module fields. Types might be cyclic after this procedure, e.g. we temporarily
  //  allow module A(v: B) and module B(v: A). But this will be caught by checking for
  //  cycles later after all proposed field types have been assigned.
  auto& mod = ctx->params->nodes[mi];
  assert(mod.type == AstNodeType::Module);
  auto& module = mod.module;
  auto ti = lookup_type(ctx, si, module.identifier);
  if (!ti) {
    return make_opt_error(message_unresolved_type_identifier(ctx, module.identifier), mod.token);
  }

  auto& mod_ty_node = *get_type_node(ctx, ti.value());
  assert(mod_ty_node.type == TypeNodeType::Module);
  auto& mod_ty = mod_ty_node.module;
  uint32_t f_beg = mod_ty.field_begin;
  assert(mod_ty.field_size == module.param_size);

  for (uint32_t i = 0; i < module.param_size; i++) {
    auto& p = *get_parameter_node(ctx, module.param_begin + i);
    auto& param = p.parameter;
    auto pti = require_type(ctx, si, param.type);
    if (!pti) {
      return make_opt_error(message_unresolved_parameter_type(ctx, param.identifier), p.token);
    }
    auto fi = f_beg + i;
    auto* field = get_module_field(ctx, fi);
    auto* field_store = get_storage(ctx, field->storage);
    assert(field_store->offset == 0 && field_store->size == 0);
    (void) field_store;
    field->type = pti.value();
    field->name = param.identifier;
  }

  auto* mod_store = get_storage(ctx, mod_ty.storage);
  assert(mod_store->size == 0 && mod_store->offset == 0);
  (void) mod_store;
  return NullOpt{};
}

Optional<ResolveError> check_cyclic_module_definition(ResolveContext* ctx, uint32_t src_ti,
                                                      uint32_t src_token, uint32_t check_ti) {
  auto& mod_ty_node = *get_type_node(ctx, src_ti);
  assert(mod_ty_node.type == TypeNodeType::Module);
  auto& mod_ty = mod_ty_node.module;
  uint32_t f_beg = mod_ty.field_begin;

  for (uint32_t i = 0; i < mod_ty.field_size; i++) {
    const auto* field = get_module_field(ctx, i + f_beg);
    const auto* type_node = get_type_node(ctx, field->type);
    if (type_node->type == TypeNodeType::Module) {
      if (check_ti == field->type) {
        return make_opt_error(message_cyclic_module_definition(), src_token);
      }

      auto maybe_err = check_cyclic_module_definition(ctx, field->type, src_token, check_ti);
      if (maybe_err) {
        return maybe_err;
      }
    }
  }

  return NullOpt{};
}

Optional<ResolveError> check_meta_type_compatibility(ResolveContext* ctx, uint32_t mi, uint32_t ti) {
  auto& mod = ctx->params->nodes[mi];
  assert(mod.type == AstNodeType::Module);

  auto* mod_ty = get_type_node(ctx, ti);
  assert(mod_ty->type == TypeNodeType::Module);
  auto* mod_field_it = ctx->module_fields.data() + mod_ty->module.field_begin;
  auto* mod_field_end = mod_field_it + mod_ty->module.field_size;

  for (uint32_t i = 0; i < mod.module.meta_type_label_size; i++) {
    auto* label_node = get_module_meta_type_label_node(ctx, i + mod.module.meta_type_label_begin);
    StringRef label_name = label_node->module_meta_type_label.identifier;

    const ModuleDescriptor* meta_type_desc{};
    for (auto& desc : ctx->params->module_meta_types) {
      if (desc.name == label_name) {
        meta_type_desc = &desc;
        break;
      }
    }

    //  Unresolved meta type should be handled at time of module declaration.
    assert(meta_type_desc);

    for (uint32_t f = 0; f < meta_type_desc->field_descriptors.size; f++) {
      auto off = f + meta_type_desc->field_descriptors.begin;
      auto* field_desc = ctx->params->module_meta_type_fields.data() + off;

      const ModuleField* match_field{};
      for (auto* it = mod_field_it; it != mod_field_end; ++it) {
        if (it->name == field_desc->name) {
          match_field = it;
          break;
        }
      }

      if (!match_field) {
        auto msg = message_missing_required_meta_type_field(
          ctx, meta_type_desc->name, field_desc->name);
        return make_opt_error(std::move(msg), mod.token);

      } else if (match_field->type != field_desc->type) {
        const uint32_t field_ind = uint32_t(match_field - mod_field_it) + mod.module.param_begin;
        const uint32_t tok = get_parameter_node(ctx, field_ind)->token;

        auto msg = message_wrong_type_for_meta_type_field(
          ctx, meta_type_desc->name, field_desc->name, field_desc->type, match_field->type);
        return make_opt_error(std::move(msg), tok);
      }
    }
  }

  return NullOpt{};
}

void resolve_module(ResolveContext* ctx, uint32_t ti) {
  //  Module definitions must be checked for cyclic references before this procedure.
  auto& mod_ty_node = *get_type_node(ctx, ti);
  assert(mod_ty_node.type == TypeNodeType::Module);
  auto& mod_ty = mod_ty_node.module;

  uint32_t off{};
  for (uint32_t i = 0; i < mod_ty.field_size; i++) {
    auto* field = get_module_field(ctx, i + mod_ty.field_begin);
    auto* field_store = get_storage(ctx, field->storage);

    auto sz = compute_type_size(ctx, field->type);
    assert(sz > 0);

    assert(field_store->offset == 0 && field_store->size == 0);
    field_store->offset = off;
    field_store->size = sz;
    off += sz;
  }

  auto* mod_store = get_storage(ctx, mod_ty.storage);
  assert(mod_store->size == 0 && mod_store->offset == 0);
  mod_store->size = off;
}

Optional<ResolveError> resolve_axiom(ResolveContext* ctx, uint32_t ai, uint32_t parent_scope) {
  auto& axiom_node = ctx->params->nodes[ai];
  assert(axiom_node.type == AstNodeType::Axiom);
  auto& axiom = axiom_node.axiom;
  add_scope_by_node(ctx, ai, parent_scope);

  for (uint32_t i = 0; i < axiom.str_size; i++) {
    auto ri = get_module_str(ctx, i + axiom.str_begin);
    uint32_t mod_ti;
    GROVE_TRY_ERR(resolve_module_str(ctx, ctx->params->nodes[ri], parent_scope, &mod_ti))
    add_type_by_node(ctx, ri, mod_ti);
  }
  return NullOpt{};
}

Optional<ResolveError> resolve_rule(ResolveContext* ctx, uint32_t ri, uint32_t parent_scope) {
  auto& rule_node = ctx->params->nodes[ri];
  assert(rule_node.type == AstNodeType::Rule);
  auto& rule = rule_node.rule;
  auto si = add_scope(ctx, make_scope(parent_scope));
  add_scope_by_node(ctx, ri, si);

  for (uint32_t i = 0; i < rule.param_size; i++) {
    auto& p = *get_parameter_node(ctx, i + rule.param_begin);
    auto pti = require_type(ctx, si, p.parameter.type);
    if (!pti) {
      return make_opt_error(
        message_unresolved_parameter_type(ctx, p.parameter.identifier), p.token);
    }
    if (!register_storage_location(ctx, si, p.parameter.identifier, pti.value())) {
      return make_opt_error(message_duplicate_identifier(ctx, p.parameter.identifier), p.token);
    }
  }

  bool has_ret{};
  for (uint32_t i = 0; i < rule.block_size; i++) {
    auto stmti = get_block_stmt(ctx, i + rule.block_begin);
    GROVE_TRY_ERR(resolve_stmt(ctx, stmti, si))
    has_ret |= is_return_stmt(ctx, stmti);
  }

  if (!has_ret && !ctx->scopes[si].all_sub_paths_return) {
    return make_opt_error(message_not_all_paths_return(), rule_node.token);
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_system(ResolveContext* ctx, uint32_t sysi, uint32_t parent_scope) {
  auto& sys_node = ctx->params->nodes[sysi];
  assert(sys_node.type == AstNodeType::System);
  auto& sys = sys_node.system;
  const uint32_t si = add_scope(ctx, make_scope(parent_scope));
  add_scope_by_node(ctx, sysi, si);

  for (uint32_t i = 0; i < sys.param_size; i++) {
    auto& p = *get_parameter_node(ctx, i + sys.param_begin);
    auto pti = require_type(ctx, si, p.parameter.type);
    if (!pti) {
      return make_opt_error(
        message_unresolved_parameter_type(ctx, p.parameter.identifier), p.token);
    }
    if (!register_storage_location(ctx, si, p.parameter.identifier, pti.value())) {
      return make_opt_error(message_duplicate_identifier(ctx, p.parameter.identifier), p.token);
    }
  }

  for (uint32_t i = 0; i < sys.rule_size; i++) {
    uint32_t ri = ctx->params->rules[i + sys.rule_begin];
    if (auto err = resolve_rule(ctx, ri, si)) {
      return err;
    }
  }

  for (uint32_t i = 0; i < sys.axiom_size; i++) {
    uint32_t ai = ctx->params->axioms[i + sys.axiom_begin];
    if (auto err = resolve_axiom(ctx, ai, si)) {
      return err;
    }
  }

  return NullOpt{};
}

Optional<ResolveError> resolve_modules(ResolveContext* ctx, uint32_t root_scope) {
  for (uint32_t mi : ctx->params->modules) {
    if (auto err = set_proposed_module_field_types(ctx, mi, root_scope)) {
      return err;
    }
  }

  for (uint32_t mi : ctx->params->modules) {
    uint32_t token = ctx->params->nodes[mi].token;
    uint32_t ti = get_module_type(ctx, mi, root_scope);
    if (auto err = check_cyclic_module_definition(ctx, ti, token, ti)) {
      return err;
    }
  }

  for (uint32_t mi : ctx->params->modules) {
    uint32_t ti = get_module_type(ctx, mi, root_scope);
    resolve_module(ctx, ti);
  }

  for (uint32_t mi : ctx->params->modules) {
    uint32_t ti = get_module_type(ctx, mi, root_scope);
    if (auto err = check_meta_type_compatibility(ctx, mi, ti)) {
      return err;
    }
  }

  return NullOpt{};
}

void set_scope_offset(ResolveContext* ctx, uint32_t si, uint32_t psi) {
  auto& scope = ctx->scopes[si];
  auto& par_scope = ctx->scopes[psi];
  assert(scope.parent == psi);
  scope.stack_offset = par_scope.stack_size + par_scope.stack_offset;
  auto scope_range = scope.stack_offset + scope.stack_size;
  ctx->scope_range = std::max(ctx->scope_range, scope_range);
}

void stmt_offsets(ResolveContext* ctx, uint32_t stmti, uint32_t parent_scope);

void if_stmt_offsets(ResolveContext* ctx, const AstNode& node, uint32_t parent_scope) {
  auto& if_data = node.stmt_if;
  for (uint32_t i = 0; i < if_data.block_size; i++) {
    auto stmti = get_block_stmt(ctx, i + if_data.block_begin);
    auto si = get_scope_by_node(ctx, stmti);
    set_scope_offset(ctx, si, parent_scope);
    stmt_offsets(ctx, stmti, si);
  }
  for (uint32_t i = 0; i < if_data.else_block_size; i++) {
    auto stmti = get_block_stmt(ctx, i + if_data.else_block_begin);
    auto si = get_scope_by_node(ctx, stmti);
    set_scope_offset(ctx, si, parent_scope);
    stmt_offsets(ctx, stmti, si);
  }
}

void stmt_offsets(ResolveContext* ctx, uint32_t stmti, uint32_t parent_scope) {
  auto& s = ctx->params->nodes[stmti];
  switch (s.type) {
    case AstNodeType::StmtIf:
      if_stmt_offsets(ctx, s, parent_scope);
      break;
    default:
      break;
  }
}

void rule_offsets(ResolveContext* ctx, uint32_t ri, uint32_t parent_scope) {
  auto si = get_scope_by_node(ctx, ri);
  set_scope_offset(ctx, si, parent_scope);
  auto& rule_node = ctx->params->nodes[ri];
  for (uint32_t i = 0; i < rule_node.rule.block_size; i++) {
    auto stmti = get_block_stmt(ctx, i + rule_node.rule.block_begin);
    stmt_offsets(ctx, stmti, si);
  }
}

void system_offsets(ResolveContext* ctx, uint32_t sysi, uint32_t parent_scope) {
  auto si = get_scope_by_node(ctx, sysi);
  set_scope_offset(ctx, si, parent_scope);
  auto& sys_node = ctx->params->nodes[sysi];
  for (uint32_t i = 0; i < sys_node.system.rule_size; i++) {
    rule_offsets(ctx, ctx->params->rules[sys_node.system.rule_begin + i], si);
  }
}

void transfer_result(ResolveContext* ctx, ResolveResult* res, uint32_t root_scope) {
  res->root_scope = root_scope;
  res->scopes = std::move(ctx->scopes);
  res->type_nodes = std::move(ctx->type_nodes);
  res->storage_locations = std::move(ctx->storage_locations);
  res->module_fields = std::move(ctx->module_fields);
  res->type_node_refs = std::move(ctx->type_node_refs);
  res->scopes_by_node = std::move(ctx->scopes_by_node);
  res->types_by_node = std::move(ctx->types_by_node);
  res->pending_foreign_functions = std::move(ctx->pending_foreign_functions);
  res->scope_range = ctx->scope_range;
  res->branch_in_t = ctx->branch_in_t;
  res->branch_out_t = ctx->branch_out_t;
  res->float_t = ctx->float_t;
  res->bool_t = ctx->bool_t;
  res->int_t = ctx->int_t;
  res->v3_t = ctx->v3_t;
  res->void_t = ctx->void_t;
}

} //  anon

bool ls::init_resolve_context(ResolveContext* ctx, const ResolveParams* params) {
  ctx->params = params;
  ctx->root_scope = add_scope(ctx, make_scope(null_scope_parent()));

  if (!add_base_types(ctx, ctx->root_scope)) {
    return false;
  }

  if (!add_base_constants(ctx, ctx->root_scope)) {
    return false;
  }

  return true;
}

ResolveResult ls::resolve(ResolveContext* ctx) {
  ResolveResult result;
  ctx->result = &result;

  for (uint32_t mi : ctx->params->modules) {
    if (auto err = declare_module(ctx, mi, ctx->root_scope)) {
      add_error(ctx, std::move(err.value()));
      return result;
    }
  }

  if (auto err = resolve_modules(ctx, ctx->root_scope)) {
    add_error(ctx, std::move(err.value()));
    return result;
  }

  for (uint32_t si : ctx->params->systems) {
    if (auto err = resolve_system(ctx, si, ctx->root_scope)) {
      add_error(ctx, std::move(err.value()));
      return result;
    }
  }

  //  set scope offsets
  for (uint32_t si : ctx->params->systems) {
    system_offsets(ctx, si, ctx->root_scope);
  }

  transfer_result(ctx, &result, ctx->root_scope);
  return result;
}

ResolveParams ls::to_resolve_params(const ParseResult& res,
                                    StringRegistry* registry,
                                    TypeIDStore* store) {
  ResolveParams p;
  p.nodes = make_view(res.nodes);
  p.parameters = make_view(res.parameters);
  p.subscripts = make_view(res.subscripts);
  p.statement_blocks = make_view(res.statement_blocks);
  p.module_strings = make_view(res.module_strings);
  p.rules = make_view(res.rules);
  p.systems = make_view(res.systems);
  p.modules = make_view(res.modules);
  p.axioms = make_view(res.axioms);
  p.module_meta_type_labels = make_view(res.module_meta_type_labels);
  p.registry = registry;
  p.type_ids = store;
  return p;
}

bool ls::lookup_variable(Scope* scopes, uint32_t si, StringRef name,
                         Variable** var, uint32_t* in_scope) {
  while (true) {
    auto& scope = scopes[si];
    if (auto it = scope.variables.find(name); it != scope.variables.end()) {
      *var = &it->second;
      *in_scope = si;
      return true;
    } else if (scope.has_parent()) {
      si = scope.parent;
    } else {
      return false;
    }
  }
}

bool ls::lookup_variable(const Scope* scopes, uint32_t si, StringRef name,
                         const Variable** var, uint32_t* in_scope) {
  while (true) {
    auto& scope = scopes[si];
    if (auto it = scope.variables.find(name); it != scope.variables.end()) {
      *var = &it->second;
      *in_scope = si;
      return true;
    } else if (scope.has_parent()) {
      si = scope.parent;
    } else {
      return false;
    }
  }
}

Optional<uint32_t> ls::lookup_type(const Scope* scopes, uint32_t si, StringRef name) {
  while (true) {
    auto& scope = scopes[si];
    if (auto it = scope.types.find(name); it != scope.types.end()) {
      return Optional<uint32_t>(it->second);
    } else if (scope.has_parent()) {
      si = scope.parent;
    } else {
      return NullOpt{};
    }
  }
}

Optional<uint32_t> ls::lookup_field(const ModuleField* fields, StringRef name,
                                    uint32_t f_beg, uint32_t f_size) {
  for (uint32_t fi = f_beg; fi < f_beg + f_size; fi++) {
    auto* f = fields + fi;
    if (f->name == name) {
      return Optional<uint32_t>(fi);
    }
  }
  return NullOpt{};
}

Optional<uint32_t> ls::get_module_field_index(const TypeNode::Module& mod_ty,
                                              const ModuleField* fields, StringRef name) {
  auto res = lookup_field(fields, name, mod_ty.field_begin, mod_ty.field_size);
  if (res) {
    res.value() -= mod_ty.field_begin;
  }
  return res;
}

uint32_t ls::type_size(const TypeNode* type_nodes, const StorageLocation* locations, uint32_t ti) {
  auto& t = type_nodes[ti];
  switch (t.type) {
    case TypeNodeType::Scalar: {
      auto& loc = locations[t.scalar.storage];
      assert(loc.size > 0);
      return loc.size;
    }
    case TypeNodeType::Function: {
      return function_ptr_size();
    }
    case TypeNodeType::Module: {
      auto& loc = locations[t.module.storage];
      assert(t.module.field_size == 0 || loc.size > 0);
      return loc.size;
    }
    default: {
      assert(false);
      return 0;
    }
  }
}

Optional<uint32_t> ls::module_type_size(const TypeNode* type_nodes,
                                        const StorageLocation* locations, uint32_t ti) {
  auto& t = type_nodes[ti];
  if (t.type != TypeNodeType::Module) {
    return NullOpt{};
  } else {
    return Optional<uint32_t>(type_size(type_nodes, locations, ti));
  }
}

Optional<uint32_t> ls::sum_module_type_sizes(const TypeNode* type_nodes,
                                             const StorageLocation* locations,
                                             const uint32_t* tis,
                                             uint32_t num_tis) {
  uint32_t res{};
  for (uint32_t i = 0; i < num_tis; i++) {
    if (auto sz = module_type_size(type_nodes, locations, tis[i])) {
      res += sz.value();
    } else {
      return NullOpt{};
    }
  }
  return Optional<uint32_t>(res);
}

bool ls::is_module_with_meta_type(const TypeNode* types, uint32_t ti, StringRef meta_label) {
  auto* ty = types + ti;
  if (ty->type == TypeNodeType::Module) {
    for (uint32_t i = 0; i < ty->module.meta_type_size; i++) {
      auto* mt = types + i + ty->module.meta_type_begin;
      assert(mt->type == TypeNodeType::ModuleMetaType);
      if (mt->module_meta_type.name == meta_label) {
        return true;
      }
    }
  }
  return false;
}

bool ls::is_function_type(const TypeNode* type_nodes, const uint32_t* type_node_refs, uint32_t ti,
                          const uint32_t* arg_tis, uint32_t num_args, uint32_t result_ti) {
  auto& ty = type_nodes[ti];
  if (ty.type != TypeNodeType::Function) {
    return false;
  }

  auto& f = ty.function;
  if (f.param_size != num_args) {
    return false;
  }

  for (uint32_t i = 0; i < num_args; i++) {
    if (type_node_refs[i + f.param_begin] != arg_tis[i]) {
      return false;
    }
  }

  if (type_node_refs[f.ret_begin] != result_ti) {
    return false;
  }

  return true;
}

bool ls::get_rule_parameter_info(const AstNode::Rule& rule, const AstNode* nodes,
                                 const uint32_t* params, const Scope* scopes, uint32_t si,
                                 RuleParameter* info) {
  for (uint32_t i = 0; i < rule.param_size; i++) {
    auto& node = nodes[params[rule.param_begin + i]];
    assert(node.type == AstNodeType::Parameter);
    auto& ty_ast_node = nodes[node.parameter.type];
    if (ty_ast_node.type != AstNodeType::TypeIdentifier) {
      assert(false);
      return false;
    }
    auto ti = lookup_type(scopes, si, ty_ast_node.type_identifier.identifier);
    if (!ti) {
      assert(false);
      return false;
    }
    RuleParameter param{};
    param.type = ti.value();
    param.marked_pred = node.parameter.marked_pred;
    info[i] = param;
  }
  return true;
}

namespace {

bool check_read_write_module_field(const TypeNode* types, const StorageLocation* locations,
                                   const ModuleField* fields, uint32_t ti, uint32_t fi,
                                   uint32_t expect_field_type, uint32_t expect_field_size,
                                   uint32_t* off) {
  auto& ty = types[ti];
  if (ty.type != TypeNodeType::Module) {
    return false;
  }
  auto& ty_mod = ty.module;
  if (fi >= ty_mod.field_size) {
    return false;
  }
  auto full_fi = fi + ty_mod.field_begin;
  auto* field = fields + full_fi;
  if (field->type != expect_field_type) {
    return false;
  }
  auto* store = locations + field->storage;
  if (store->size != expect_field_size) {
    return false;
  }
  *off = store->offset;
  return true;
}

} //  anon

bool ls::read_module_field(const void* data, const TypeNode* types, const StorageLocation* locations,
                           const ModuleField* fields, uint32_t ti, uint32_t fi,
                           uint32_t expect_field_type, uint32_t expect_field_size, void* dst) {
  uint32_t off;
  bool success = check_read_write_module_field(
    types, locations, fields, ti, fi, expect_field_type, expect_field_size, &off);
  if (!success) {
    return false;
  }
  auto read_from = static_cast<const unsigned char*>(data) + off;
  memcpy(dst, read_from, expect_field_size);
  return true;
}

bool ls::write_module_field(void* dst, const TypeNode* types, const StorageLocation* locations,
                            const ModuleField* fields, uint32_t ti, uint32_t fi,
                            uint32_t expect_field_type, uint32_t expect_field_size,
                            const void* src) {
  uint32_t off;
  bool success = check_read_write_module_field(
    types, locations, fields, ti, fi, expect_field_type, expect_field_size, &off);
  if (!success) {
    return false;
  }
  auto write_to = static_cast<unsigned char*>(dst) + off;
  memcpy(write_to, src, expect_field_size);
  return true;
}

bool ls::read_module_fieldf(const void* data, const TypeNode* types, uint32_t float_ti,
                            const StorageLocation* locations, const ModuleField* fields,
                            uint32_t ti, uint32_t fi, float* dst) {
  return read_module_field(data, types, locations, fields, ti, fi, float_ti, sizeof(float), dst);
}

GROVE_NAMESPACE_END
