#include "LSystemComponent.hpp"
#include "../render/debug_draw.hpp"
#include "../terrain/terrain.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/fs.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/SlotLists.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/serialize/text.hpp"
#include "grove/ls/ls.hpp"
#include "grove/math/util.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/frame.hpp"

#include <imgui/imgui.h>
#include <memory>
#include <random>

GROVE_NAMESPACE_BEGIN

namespace ls {

struct Internode {
  static bool is_axis_root(const Internode* nodes, int self_index) {
    auto& self = nodes[self_index];
    return self.parent == -1 || nodes[self.parent].has_lateral_child(self_index);
  }
  bool is_root() const {
    return parent == -1;
  }
  bool has_medial_child() const {
    return medial_child != -1;
  }
  bool has_lateral_child(int i) const {
    auto lat_end = lateral_child_begin + lateral_child_size;
    return i >= lateral_child_begin && i < lat_end;
  }
  Vec3f tip_position() const {
    return p + d * length;
  }

  Vec3f p;
  Vec3f d;
  float diameter;
  float length;
  int parent;
  int medial_child;
  int lateral_child_begin;
  int lateral_child_size;
};

} //  ls

namespace {

using namespace ls;

struct RandStream {
  float nextf() {
    return float(dis(gen));
  }
  Vec3f nextf3() {
    return Vec3f{nextf(), nextf(), nextf()};
  }
  void seed(uint32_t s) {
    gen.seed(s);
  }

  std::random_device rd;
  std::mt19937 gen{rd()};
  std::uniform_real_distribution<> dis{0.0, 1.0};
};

struct ExecutionPipelineCompileResult {
  std::vector<RuleParameter> rule_params;
  std::vector<Span> rule_param_spans;
  std::vector<Span> rule_instruction_spans;
  std::vector<uint8_t> rule_instructions;
  std::vector<uint32_t> rule_si;
  uint32_t num_rules{};
};

struct AvailableForeignFunction {
  ForeignFunction* func;
  uint32_t arg_tis[8];
  uint32_t num_args;
  uint32_t ret_ti;
};

using AvailableForeignFuncs = std::unordered_map<StringRef, AvailableForeignFunction, StringRef::Hash>;

struct ExecutionContext {
  DeriveContext derive_context{};

  std::unique_ptr<uint8_t[]> stack;
  size_t stack_size{};

  std::unique_ptr<uint8_t[]> frame;
  size_t frame_size{};

  std::vector<uint32_t> axiom;
  std::vector<uint8_t> axiom_data;
};

struct LSExecutionPipeline {
  StringRegistry string_registry;
  TypeIDStore type_id_store;

  uint32_t system_node_index{};
  uint32_t system_scope_index{};

  ParseResult parse_result;
  ResolveResult resolve_result;
  CompileParams::ForeignFunctions foreign_functions;
  ExecutionPipelineCompileResult compile_result;

  std::vector<ModuleDescriptor> module_meta_types;
  std::vector<ModuleFieldDescriptor> module_meta_type_fields;

  StringRef ident_true{};
  StringRef ident_internode{};
  StringRef ident_p{};
  StringRef ident_d{};
  StringRef ident_l{};
};

} //  anon

struct ls::LSystemComponent {
  std::string src_file_path;
  Optional<LSExecutionPipeline> debug_execution_pipeline;
  Optional<ExecutionContext> debug_execution_context;
  uint32_t rand_seed{5489u};
  bool use_rand_seed{};
  int num_steps{};
  bool need_run_system{};
  bool need_regen_execution_context{};
  bool hide_module_contents_in_repr{true};
  bool draw_node_bounds{};
  DeriveResult latest_derive_result{};
  std::string latest_derive_result_repr{};
  std::vector<ls::Internode> debug_internodes;

  ProceduralTreeRootsRenderer::DrawableHandle debug_drawable{};

  float gen_execution_context_ms{};
  float derive_ms{};
  float build_tree_ms{};
  float gen_mesh_ms{};

  float length_scale{1.0f};
  float leaf_diameter{0.04f};
  float diameter_power{1.8f};
  Vec3f origin{8.0f};
  bool lock_to_terrain{true};
};

namespace {

//  @NOTE: Not thread safe
struct {
  RandStream rand_stream;
} globals;

void system_print(uint32_t arg_size, uint32_t ret_size, uint8_t* data) {
  assert(arg_size == sizeof(float) && ret_size == 0);
  (void) arg_size;
  (void) ret_size;
  float v;
  memcpy(&v, data, sizeof(float));
  printf("%0.5f\n", v);
}

void system_urand(uint32_t arg_size, uint32_t ret_size, uint8_t* data) {
  assert(arg_size == 0 && ret_size == sizeof(float));
  (void) arg_size;
  (void) ret_size;
  float res = globals.rand_stream.nextf();
  memcpy(data, &res, sizeof(float));
}

void system_urand3(uint32_t arg_size, uint32_t ret_size, uint8_t* data) {
  assert(arg_size == 0 && ret_size == 3 * sizeof(float));
  (void) arg_size;
  (void) ret_size;
  Vec3f res = globals.rand_stream.nextf3();
  memcpy(data, &res.x, 3 * sizeof(float));
}

void system_norm3(uint32_t arg_size, uint32_t ret_size, uint8_t* data) {
  assert(arg_size == 3 * sizeof(float) && ret_size == 3 * sizeof(float));
  Vec3f v;
  memcpy(&v.x, data, 3 * sizeof(float));
  v = normalize(v);
  memcpy(data, &v.x, 3 * sizeof(float));
  (void) arg_size;
  (void) ret_size;
}

std::string token_marked_message(std::string_view src, const Token& token,
                                 const std::string& message) {
  return io::mark_text_with_message_and_context(src, token.begin, token.end, 32, message);
}

void create_meta_types(StringRegistry* str_reg, uint32_t v3_t, uint32_t float_t,
                       std::vector<ModuleDescriptor>& mod_descs,
                       std::vector<ModuleFieldDescriptor>& mod_field_descs) {
  const auto field_beg = uint32_t(mod_field_descs.size());
  ModuleDescriptor& internode_desc = mod_descs.emplace_back();
  internode_desc.name = str_reg->emplace("internode");
  internode_desc.field_descriptors.begin = field_beg;

  ModuleFieldDescriptor pos_desc{};
  pos_desc.name = str_reg->emplace("p");
  pos_desc.type = v3_t;

  ModuleFieldDescriptor dir_desc{};
  dir_desc.name = str_reg->emplace("d");
  dir_desc.type = v3_t;

  ModuleFieldDescriptor len_desc{};
  len_desc.name = str_reg->emplace("l");
  len_desc.type = float_t;

  mod_field_descs.push_back(pos_desc);
  mod_field_descs.push_back(dir_desc);
  mod_field_descs.push_back(len_desc);
  internode_desc.field_descriptors.size = uint32_t(mod_field_descs.size()) - field_beg;
}

Optional<ScanResult> do_scan(const std::string& src) {
  auto scan_res = ls::scan(src.data(), int64_t(src.size()));
  if (!scan_res.errors.empty()) {
    for (auto& err : scan_res.errors) {
      printf("%s\n", err.message.c_str());
    }
    return NullOpt{};

  } else {
    return Optional<ScanResult>(std::move(scan_res));
  }
}

Optional<ParseResult> do_parse(const ScanResult& scan_res, const std::string& src,
                               StringRegistry* str_reg) {
  ParseParams parse_params;
  parse_params.source = src.data();
  parse_params.str_registry = str_reg;

  auto parse_res = parse(scan_res.tokens.data(), int64_t(scan_res.tokens.size()), &parse_params);
  if (!parse_res.errors.empty()) {
    auto src_view = std::string_view{src};
    for (auto& err : parse_res.errors) {
      auto msg = token_marked_message(src_view, scan_res.tokens[err.token], err.message);
      printf("%s\n", msg.c_str());
    }
    return NullOpt{};

  } else if (parse_res.systems.empty()) {
    printf("No systems\n");
    return NullOpt{};

  } else {
    return Optional<ParseResult>(std::move(parse_res));
  }
}

Optional<ResolveResult> do_resolve(const ScanResult& scan_res, const ParseResult& parse_res,
                                   const std::string& src, StringRegistry* str_reg,
                                   TypeIDStore* type_ids,
                                   std::vector<ModuleDescriptor>& module_meta_types,
                                   std::vector<ModuleFieldDescriptor>& module_meta_type_fields) {
  auto res_params = to_resolve_params(parse_res, str_reg, type_ids);

  ResolveContext res_ctx{};
  if (!init_resolve_context(&res_ctx, &res_params)) {
    printf("Failed to initialize resolve context.\n");
    return NullOpt{};
  }

  assert(res_ctx.v3_t && res_ctx.float_t);
  create_meta_types(str_reg, res_ctx.v3_t, res_ctx.float_t, module_meta_types, module_meta_type_fields);
  res_params.module_meta_types = make_view(module_meta_types);
  res_params.module_meta_type_fields = make_view(module_meta_type_fields);

  auto res_res = resolve(&res_ctx);
  if (!res_res.errors.empty()) {
    auto src_view = std::string_view{src};
    for (auto& err : res_res.errors) {
      auto msg = token_marked_message(src_view, scan_res.tokens[err.token], err.message);
      printf("%s\n", msg.c_str());
    }
    return NullOpt{};

  } else {
    return Optional<ResolveResult>(std::move(res_res));
  }
}

ExecutionPipelineCompileResult do_compile(const ParseResult& parse_res,
                                          const ResolveResult& resolve_res,
                                          const CompileParams::ForeignFunctions& foreign_funcs,
                                          const uint32_t sysi) {
  ExecutionPipelineCompileResult result;

  auto comp_params = to_compile_params(parse_res, resolve_res, &foreign_funcs);
  auto& sys = parse_res.nodes[sysi].system;

  std::vector<RuleParameter> rule_params;
  std::vector<Span> rule_param_spans;

  std::vector<Span> rule_instruction_spans;
  std::vector<uint8_t> rule_instructions;
  std::vector<uint32_t> rule_si;
  const uint32_t num_rules = sys.rule_size;
  for (uint32_t i = 0; i < sys.rule_size; i++) {
    auto ri = parse_res.rules[sys.rule_begin + i];
    auto rsi = resolve_res.scopes_by_node.at(ri);
    rule_si.push_back(rsi);
    auto& rule = parse_res.nodes[ri].rule;
    auto comp_res = compile_rule(&comp_params, ri);

    Span inst_span{};
    inst_span.begin = uint32_t(rule_instructions.size());
    inst_span.size = uint32_t(comp_res.instructions.size());
    rule_instruction_spans.push_back(inst_span);
    rule_instructions.insert(
      rule_instructions.end(), comp_res.instructions.begin(), comp_res.instructions.end());

    Span param_span{};
    param_span.begin = uint32_t(rule_params.size());
    param_span.size = rule.param_size;
    rule_param_spans.push_back(param_span);

    rule_params.resize(rule_params.size() + rule.param_size);
    bool param_success = get_rule_parameter_info(
      rule,
      parse_res.nodes.data(),
      parse_res.parameters.data(),
      resolve_res.scopes.data(),
      rsi,
      rule_params.data() + param_span.begin);
    assert(param_success);
    (void) param_success;
  }

  result.rule_params = std::move(rule_params);
  result.rule_param_spans = std::move(rule_param_spans);
  result.rule_instruction_spans = std::move(rule_instruction_spans);
  result.rule_instructions = std::move(rule_instructions);
  result.rule_si = std::move(rule_si);
  result.num_rules = num_rules;
  return result;
}

bool is_function_type(const ResolveResult& res, uint32_t ti, const AvailableForeignFunction& avail) {
  auto* types = res.type_nodes.data();
  auto* refs = res.type_node_refs.data();
  return is_function_type(types, refs, ti, avail.arg_tis, avail.num_args, avail.ret_ti);
}

void insert_available_foreign_functions(const ResolveResult& resolve_res, StringRegistry* str_reg,
                                        AvailableForeignFuncs& into) {
  into[str_reg->emplace("urand")] = {system_urand, {}, 0, resolve_res.float_t};
  into[str_reg->emplace("urand3")] = {system_urand3, {}, 0, resolve_res.v3_t};
  into[str_reg->emplace("norm3")] = {system_norm3, {resolve_res.v3_t}, 1, resolve_res.v3_t};
  into[str_reg->emplace("print")] = {system_print, {resolve_res.float_t}, 1, resolve_res.void_t};
}

Optional<CompileParams::ForeignFunctions>
create_foreign_functions(const ResolveResult& resolve_res, const AvailableForeignFuncs& avail_funcs,
                         const StringRegistry* str_reg) {
  CompileParams::ForeignFunctions foreign_functions;

  for (auto& pend : resolve_res.pending_foreign_functions) {
    if (auto it = avail_funcs.find(pend.identifier); it != avail_funcs.end()) {
      if (is_function_type(resolve_res, pend.type_index, it->second)) {
        foreign_functions[pend] = it->second.func;
      } else {
        printf("Function \"%s\" has the wrong type.", str_reg->get(pend.identifier).c_str());
        return NullOpt{};
      }
    } else {
      printf("Missing function: \"%s\"\n", str_reg->get(pend.identifier).c_str());
      return NullOpt{};
    }
  }

  return Optional<CompileParams::ForeignFunctions>(std::move(foreign_functions));
}

void register_builtin_identifiers(LSExecutionPipeline& pipe) {
  pipe.ident_true = pipe.string_registry.emplace("true");
  pipe.ident_internode = pipe.string_registry.emplace("internode");
  pipe.ident_d = pipe.string_registry.emplace("d");
  pipe.ident_p = pipe.string_registry.emplace("p");
  pipe.ident_l = pipe.string_registry.emplace("l");
}

Optional<LSExecutionPipeline> create_execution_pipeline(const std::string& src) {
  LSExecutionPipeline result;

  auto scan_res = do_scan(src);
  if (!scan_res) {
    return NullOpt{};
  }

  if (auto parse_res = do_parse(scan_res.value(), src, &result.string_registry)) {
    result.parse_result = std::move(parse_res.value());
  } else {
    return NullOpt{};
  }

  auto res_res = do_resolve(
    scan_res.value(), result.parse_result, src, &result.string_registry, &result.type_id_store,
    result.module_meta_types, result.module_meta_type_fields);
  if (!res_res) {
    return NullOpt{};
  } else {
    result.resolve_result = std::move(res_res.value());
  }

  AvailableForeignFuncs avail_funcs;
  insert_available_foreign_functions(result.resolve_result, &result.string_registry, avail_funcs);

  auto foreign_funcs = create_foreign_functions(
    result.resolve_result, avail_funcs, &result.string_registry);
  if (!foreign_funcs) {
    return NullOpt{};
  } else {
    result.foreign_functions = std::move(foreign_funcs.value());
  }

  assert(!result.parse_result.systems.empty());
  const uint32_t sysi = result.parse_result.systems[0];
  result.system_node_index = sysi;
  result.system_scope_index = result.resolve_result.scopes_by_node.at(sysi);

  result.compile_result = do_compile(
    result.parse_result, result.resolve_result, result.foreign_functions, sysi);

  register_builtin_identifiers(result);

  return Optional<LSExecutionPipeline>(std::move(result));
}

bool gen_axiom(const LSExecutionPipeline& pipeline, ExecutionContext* ctx) {
  const auto& res_res = pipeline.resolve_result;
  const uint32_t system_index = pipeline.system_node_index;
  const uint32_t scope_index = pipeline.system_scope_index;

  {
    const Variable* var;
    uint32_t var_si;
    if (lookup_variable(res_res.scopes.data(), scope_index, pipeline.ident_true, &var, &var_si)) {
      auto* store = res_res.storage_locations.data() + var->storage;
      assert(store->size == sizeof(int32_t) && store->offset < ctx->frame_size);
      int32_t true_dat = 1;
      memcpy(ctx->frame.get() + store->offset, &true_dat, sizeof(int32_t));
    } //  else ?
  }

  const auto& sys = pipeline.parse_result.nodes[system_index];
  assert(sys.type == AstNodeType::System);

  std::vector<uint32_t> str;
  std::vector<uint8_t> str_data;
  if (sys.system.axiom_size > 0) {
    auto comp_params = to_compile_params(
      pipeline.parse_result, pipeline.resolve_result, &pipeline.foreign_functions);

    auto ai = pipeline.parse_result.axioms[sys.system.axiom_begin];
    auto axiom_res = compile_axiom(&comp_params, ai);
    auto interp_ctx = make_interpret_context(
      ctx->frame.get(), uint32_t(ctx->frame_size), ctx->stack.get(), ctx->stack_size);
    auto interp_res = interpret(
      &interp_ctx,
      axiom_res.instructions.data(),
      axiom_res.instructions.size());

    if (interp_res.ok) {
      str.resize(interp_res.succ_str_size);
      str_data.resize(interp_res.succ_str_data_size);
      memcpy(str_data.data(), interp_res.succ_str_data, interp_res.succ_str_data_size);
      memcpy(str.data(), interp_res.succ_str, interp_res.succ_str_size * sizeof(uint32_t));

    } else {
      assert(false);
      return false;
    }
  }

  ctx->axiom = std::move(str);
  ctx->axiom_data = std::move(str_data);
  return true;
}

Optional<ExecutionContext> create_execution_context(const LSExecutionPipeline& pipeline) {
  ExecutionContext result;
  result.frame_size = pipeline.resolve_result.scope_range;
  result.frame = std::make_unique<uint8_t[]>(result.frame_size);

  //  @TODO: Parameterized stack size
  const uint32_t stack_size = 1024 * 2;
  result.stack_size = stack_size;
  result.stack = std::make_unique<uint8_t[]>(stack_size);

  //  @TODO: Assumes pipeline pointer is stable
  auto& derive_ctx = result.derive_context;
  auto& res_res = pipeline.resolve_result;

  derive_ctx.scopes = res_res.scopes.data();
  derive_ctx.type_nodes = res_res.type_nodes.data();
  derive_ctx.storage = res_res.storage_locations.data();
  derive_ctx.num_rules = pipeline.compile_result.num_rules;
  derive_ctx.rule_params = pipeline.compile_result.rule_params.data();
  derive_ctx.rule_param_spans = pipeline.compile_result.rule_param_spans.data();
  derive_ctx.rule_instructions = pipeline.compile_result.rule_instructions.data();
  derive_ctx.rule_instruction_spans = pipeline.compile_result.rule_instruction_spans.data();
  derive_ctx.rule_si = pipeline.compile_result.rule_si.data();
  derive_ctx.frame = result.frame.get();
  derive_ctx.frame_size = uint32_t(result.frame_size);
  derive_ctx.stack = result.stack.get();
  derive_ctx.stack_size = stack_size;
  derive_ctx.branch_in_t = res_res.branch_in_t;
  derive_ctx.branch_out_t = res_res.branch_out_t;

  if (!gen_axiom(pipeline, &result)) {
    return NullOpt{};
  }

  return Optional<ExecutionContext>(std::move(result));
}

Optional<DeriveResult> run_system(ExecutionContext* ctx, int num_steps) {
  auto str = ctx->axiom;
  auto str_data = ctx->axiom_data;
  for (int i = 0; i < num_steps; i++) {
    DerivingString derive_str{};
    derive_str.str_data = str_data.data();
    derive_str.str_data_size = uint32_t(str_data.size());
    derive_str.str = str.data();
    derive_str.str_size = uint32_t(str.size());
    auto derive_res = derive_branched(&ctx->derive_context, &derive_str);
    str = derive_res.str;
    str_data = derive_res.str_data;
  }

  DeriveResult result;
  result.str = std::move(str);
  result.str_data = std::move(str_data);
  return Optional<DeriveResult>(std::move(result));
}

std::string debug_repr_derived_str(const LSExecutionPipeline& pipeline,
                                   const std::vector<uint32_t>& str,
                                   const std::vector<uint8_t>& str_data,
                                   bool hide_mod_contents) {
  auto dump_ctx = to_dump_context(
    pipeline.parse_result, pipeline.resolve_result, &pipeline.string_registry);
  dump_ctx.hide_module_contents = hide_mod_contents;

  std::string dump_str;
  uint32_t off{};
  for (uint32_t i = 0; i < uint32_t(str.size()); i++) {
    auto sz = module_type_size(
      pipeline.resolve_result.type_nodes.data(),
      pipeline.resolve_result.storage_locations.data(),
      str[i]).unwrap();
    dump_str += dump_module_bytes(str_data.data() + off, str[i], &dump_ctx);
    off += sz;
    if (i < str.size()-1) {
      dump_str += dump_ctx.hide_module_contents ? "," : "\n";
    }
  }

  return dump_str;
}

bool read_module_field(const ResolveResult& res, const void* src, uint32_t mod_ti, uint32_t fi,
                       uint32_t field_ti, uint32_t sz, void* dst) {
  const auto* type_nodes = res.type_nodes.data();
  const auto* store = res.storage_locations.data();
  const auto* fields = res.module_fields.data();
  return read_module_field(src, type_nodes, store, fields, mod_ti, fi, field_ti, sz, dst);
}

/*
 * extract branching structure from lsystem output
 */

struct ExtractBuiltinModuleParams {
  const ls::ResolveResult* resolve_result;
  StringRef ident_internode;
  StringRef ident_p;
  StringRef ident_d;
  StringRef ident_l;
};

struct ExtractedBuiltinModule {
  enum class Type {
    BranchIn,
    BranchOut,
    Internode,
  };

  struct Internode {
    Vec3f p;
    Vec3f d;
    float l;
  };

  Type type;
  union {
    Internode internode;
  };
};

std::vector<ExtractedBuiltinModule>
extract_builtin_modules(const DerivingString& str, const ExtractBuiltinModuleParams& params) {
  const auto& res_res = *params.resolve_result;

  const auto* type_nodes = res_res.type_nodes.data();
  const auto* store = res_res.storage_locations.data();
  const auto* fields = res_res.module_fields.data();

  const uint32_t branch_in_t = res_res.branch_in_t;
  const uint32_t branch_out_t = res_res.branch_out_t;

  const uint32_t v3_t = res_res.v3_t;
  const uint32_t v3_s = 3 * sizeof(float);

  const uint32_t float_t = res_res.float_t;
  const uint32_t float_s = sizeof(float);

  std::vector<ExtractedBuiltinModule> extracted;

  uint32_t data_off{};
  for (uint32_t i = 0; i < str.str_size; i++) {
    const uint32_t str_ti = str.str[i];
    const uint8_t* str_data = str.str_data + data_off;

    if (str_ti == branch_in_t) {
      auto& mod = extracted.emplace_back();
      mod.type = ExtractedBuiltinModule::Type::BranchIn;

    } else if (str_ti == branch_out_t) {
      auto& mod = extracted.emplace_back();
      mod.type = ExtractedBuiltinModule::Type::BranchOut;

    } else {
      bool is_inode = is_module_with_meta_type(type_nodes, str_ti, params.ident_internode);
      if (is_inode) {
        auto& mod_ty = type_nodes[str_ti].module;
        auto p_fi = get_module_field_index(mod_ty, fields, params.ident_p);
        assert(p_fi);
        auto d_fi = get_module_field_index(mod_ty, fields, params.ident_d);
        assert(d_fi);
        auto l_fi = get_module_field_index(mod_ty, fields, params.ident_l);
        assert(l_fi);

        Vec3f p{};
        Vec3f d{};
        float l{};
        bool success{};
        success = read_module_field(res_res, str_data, str_ti, p_fi.value(), v3_t, v3_s, &p.x);
        assert(success);
        success = read_module_field(res_res, str_data, str_ti, d_fi.value(), v3_t, v3_s, &d.x);
        assert(success);
        success = read_module_field(res_res, str_data, str_ti, l_fi.value(), float_t, float_s, &l);
        assert(success);

        auto& mod = extracted.emplace_back();
        mod.type = ExtractedBuiltinModule::Type::Internode;
        mod.internode = {};
        mod.internode.p = p;
        mod.internode.d = d;
        mod.internode.l = l;
        (void) success;
      }
    }

    data_off += module_type_size(type_nodes, store, str_ti).unwrap();
  }

  return extracted;
}

std::vector<ls::Internode> build_tree(const ExtractedBuiltinModule* modules, uint32_t num_modules) {
  using LateralChildren = SlotLists<int>;
  struct PendInternode {
    int module_index{};
    int parent{-1};
    int medial_child{-1};
    LateralChildren::List lateral_children;
  };

  std::vector<PendInternode> pend_internodes;
  LateralChildren store_lat_children;

  {
    Optional<int> curr_medial_parent;
    Optional<int> curr_lateral_parent;
    std::vector<Optional<int>> medial_parents;
    std::vector<Optional<int>> lateral_parents;

    uint32_t i{};
    while (i < num_modules) {
      const int mod_index = int(i);
      auto& mod = modules[i++];

      if (mod.type == ExtractedBuiltinModule::Type::BranchIn) {
        medial_parents.push_back(curr_medial_parent);
        lateral_parents.push_back(curr_lateral_parent);
        curr_lateral_parent = curr_medial_parent;
        curr_medial_parent = NullOpt{};

      } else if (mod.type == ExtractedBuiltinModule::Type::BranchOut) {
        assert(!medial_parents.empty() && !lateral_parents.empty());
        curr_medial_parent = medial_parents.back();
        medial_parents.pop_back();
        curr_lateral_parent = lateral_parents.back();
        lateral_parents.pop_back();

      } else if (mod.type == ExtractedBuiltinModule::Type::Internode) {
        int self_ind = int(pend_internodes.size());
        auto& inode = pend_internodes.emplace_back();
        inode.module_index = mod_index;

        if (curr_medial_parent) {
          assert(!curr_lateral_parent);
          const int curr_med_par = curr_medial_parent.value();
          assert(pend_internodes[curr_med_par].medial_child == -1);
          pend_internodes[curr_med_par].medial_child = self_ind;
          inode.parent = curr_med_par;
        }
        if (curr_lateral_parent) {
          assert(!curr_medial_parent);
          const int curr_lat_par = curr_lateral_parent.value();
          auto& lat = pend_internodes[curr_lat_par];
          lat.lateral_children = store_lat_children.insert(lat.lateral_children, self_ind);
          inode.parent = curr_lat_par;
          curr_lateral_parent = NullOpt{};
        }
        curr_medial_parent = self_ind;
      }
    }
  }

  //  Linearize trees
  std::vector<ls::Internode> result;
  result.reserve(pend_internodes.size());
  {
    struct ResultIndices {
      int src;
      int dst;
      int dst_parent;
    };

    std::vector<ResultIndices> pend;
    for (int i = 0; i < int(pend_internodes.size()); i++) {
      if (pend_internodes[i].parent == -1) {
        const int src_ind = i;
        const int dst_ind = int(result.size());
        result.emplace_back();
        pend.push_back({src_ind, dst_ind, -1});
      }
    }

    while (!pend.empty()) {
      const ResultIndices inds = pend.back();
      pend.pop_back();
      const auto& src = pend_internodes[inds.src];

      int med_index = -1;
      if (src.medial_child != -1) {
        med_index = int(result.size());
        ResultIndices next{};
        next.src = src.medial_child;
        next.dst = med_index;
        next.dst_parent = inds.dst;
        result.emplace_back();
        pend.push_back(next);
      }

      auto it = store_lat_children.begin(src.lateral_children);
      const int dst_beg = int(result.size());
      for (; it != store_lat_children.end(); ++it) {
        ResultIndices next{};
        next.src = *it;
        next.dst = int(result.size());
        next.dst_parent = inds.dst;
        result.emplace_back();
        pend.push_back(next);
      }
      const int dst_end = int(result.size());

      const auto& src_mod = modules[src.module_index];
      assert(src_mod.type == ExtractedBuiltinModule::Type::Internode);

      auto& dst = result[inds.dst];
      dst.p = src_mod.internode.p;
      dst.d = src_mod.internode.d;
      dst.length = src_mod.internode.l;
      assert(dst.length > 0.0f);  //  @TODO: How to ensure this is true?
      dst.diameter = 0.0f;  //  @NOTE: Assigned later.
      dst.parent = inds.dst_parent;
      dst.medial_child = med_index;
      dst.lateral_child_begin = dst_beg;
      dst.lateral_child_size = dst_end - dst_beg;
    }
  }

  return result;
}

[[maybe_unused]] bool internode_relationships_valid(const Internode* nodes, int num_nodes) {
  std::vector<bool> is_child(num_nodes);

  for (int i = 0; i < num_nodes; i++) {
    auto& node = nodes[i];
    if (node.has_medial_child()) {
      if (is_child[node.medial_child]) {
        //  duplicate child
        return false;
      } else {
        is_child[node.medial_child] = true;
      }

      if (nodes[node.medial_child].parent != i) {
        //  this node should be the parent of its child
        return false;
      }
    }

    for (int j = 0; j < node.lateral_child_size; j++) {
      const int lat = node.lateral_child_begin + j;
      if (is_child[lat]) {
        //  duplicate child
        return false;
      } else {
        is_child[lat] = true;
      }

      if (nodes[lat].parent != i) {
        //  this node should be the parent of its child
        return false;
      }
    }
  }

  for (int i = 0; i < num_nodes; i++) {
    //  nodes should be child of another node, except roots
    if (!is_child[i] && !nodes[i].is_root()) {
      return false;
    }
  }

  return true;
}

ExtractBuiltinModuleParams
to_extract_builtin_module_params(const LSExecutionPipeline& pipe) {
  ExtractBuiltinModuleParams result{};
  result.resolve_result = &pipe.resolve_result;
  result.ident_p = pipe.ident_p;
  result.ident_d = pipe.ident_d;
  result.ident_l = pipe.ident_l;
  result.ident_internode = pipe.ident_internode;
  return result;
}

DerivingString to_deriving_string(const DeriveResult& res) {
  DerivingString result{};
  result.str = res.str.data();
  result.str_size = uint32_t(res.str.size());
  result.str_data = res.str_data.data();
  result.str_data_size = uint32_t(res.str_data.size());
  return result;
}

/*
 * set properties of extracted output
 */

struct AssignDiameterParams {
  float leaf_diameter;
  float diameter_power;
};

AssignDiameterParams make_assign_diameter_params(float leaf_diam, float diam_pow) {
  AssignDiameterParams result{};
  result.leaf_diameter = leaf_diam;
  result.diameter_power = diam_pow;
  return result;
}

float assign_diameter(ls::Internode* nodes, ls::Internode* node,
                      const AssignDiameterParams& params) {
  auto leaf_diameter = [](const AssignDiameterParams& params) -> float {
    return std::pow(params.leaf_diameter, params.diameter_power);
  };

  float md = leaf_diameter(params);
  float ld = node->lateral_child_size == 0 ? md : 0.0f;
  if (node->has_medial_child()) {
    md = assign_diameter(nodes, nodes + node->medial_child, params);
  }
  for (int i = 0; i < node->lateral_child_size; i++) {
    ld += assign_diameter(nodes, nodes + i + node->lateral_child_begin, params);
  }

  auto d = md + ld;
  const auto min_diam = float(std::pow(d, 1.0 / params.diameter_power));
  assert(node->diameter == 0.0f);
  node->diameter = std::max(params.leaf_diameter, min_diam);
  assert(std::isfinite(node->diameter) && node->diameter >= 0.0f);
  return d;
}

void assign_diameter(ls::Internode* nodes, int num_nodes, const AssignDiameterParams& params) {
  for (int i = 0; i < num_nodes; i++) {
    if (nodes[i].is_root()) {
      assign_diameter(nodes, nodes + i, params);
    }
  }
}

void apply_length_scale(ls::Internode* nodes, int num_nodes, float length_scale) {
  for (int i = 0; i < num_nodes; i++) {
    nodes[i].length *= length_scale;
  }
}

void assign_position(ls::Internode* nodes, int num_nodes, const Vec3f& ori) {
  Temporary<int, 2048> store_stack;
  int* stack = store_stack.require(num_nodes);
  int s{};

  for (int i = 0; i < num_nodes; i++) {
    if (nodes[i].is_root()) {
      stack[s++] = i;
    }
  }

  while (s) {
    auto& node = nodes[stack[--s]];

    auto tip_p = node.p + node.d * node.length;
    if (node.has_medial_child()) {
      nodes[node.medial_child].p = tip_p;
      stack[s++] = node.medial_child;
    }

    for (int i = 0; i < node.lateral_child_size; i++) {
      const int lat = node.lateral_child_begin + i;
      nodes[lat].p = node.p;
      stack[s++] = lat;
    }
  }

  for (int i = 0; i < num_nodes; i++) {
    nodes[i].p += ori;
  }
}

/*
 * create mesh
 */

OBB3f make_node_obb(const ls::Internode& internode) {
  float diameter = internode.diameter;
  auto half_size_xz = diameter * 0.5f;
  auto half_size_y = internode.length * 0.5f;
  auto position = internode.p + internode.d * half_size_y;

  OBB3f res{};
  make_coordinate_system_y(internode.d, &res.i, &res.j, &res.k);
  res.position = position;
  res.half_size = Vec3f{half_size_xz, half_size_y, half_size_xz};
  return res;
}

void compute_node_frames(const ls::Internode* nodes, int num_nodes, Mat3f* dst) {
  for (int i = 0; i < num_nodes; i++) {
    if (Internode::is_axis_root(nodes, i)) {
      Mat3f root{};
      make_coordinate_system_y(nodes[i].d, &root[0], &root[1], &root[2]);
      dst[i] = root;
    }
  }

  for (int i = 0; i < num_nodes; i++) {
    auto& self_frame = dst[i];
    auto& self_node = nodes[i];

    const ls::Internode* child_node{};
    if (self_node.has_medial_child()) {
      child_node = nodes + self_node.medial_child;
    } else {
      continue;
    }

    auto& child_frame = dst[int(child_node - nodes)];
    child_frame[1] = child_node->d;
    if (std::abs(dot(child_frame[1], self_frame[2])) > 0.99f) {
      make_coordinate_system_y(child_frame[1], &child_frame[0], &child_frame[1], &child_frame[2]);
    } else {
      child_frame[0] = normalize(cross(child_frame[1], self_frame[2]));
      if (dot(child_frame[0], self_frame[0]) < 0.0f)  {
        child_frame[0] = -child_frame[0];
      }
      child_frame[2] = cross(child_frame[0], child_frame[1]);
      if (dot(child_frame[2], self_frame[2]) < 0.0f) {
        child_frame[2] = -child_frame[2];
      }
    }
  }
}

void to_render_instances(const Internode* nodes, const Mat3f* node_frames,
                         int num_nodes, bool atten_radius_by_length, float length_scale,
                         ProceduralTreeRootsRenderer::Instance* dst) {
  auto child_of = [](const Internode& node, const Internode* nodes) {
    return node.has_medial_child() ? nodes + node.medial_child : nullptr;
  };

  for (int i = 0; i < num_nodes; i++) {
    auto& inst = dst[i];
    auto& node = nodes[i];

    const auto& self_right = node_frames[i][0];
    const auto& self_up = node_frames[i][1];

    inst.self_position = node.p;
    inst.self_radius = node.diameter * 0.5f;

    if (atten_radius_by_length) {
      inst.self_radius *= (node.length / length_scale);
    }

    Vec3f child_right;
    Vec3f child_up;
    if (auto* child = child_of(node, nodes)) {
      inst.child_position = child->p;
      inst.child_radius = child->diameter * 0.5f;

      if (atten_radius_by_length) {
        inst.child_radius *= (child->length / length_scale);
      }

      int child_ind = int(child - nodes);
      child_right = node_frames[child_ind][0];
      child_up = node_frames[child_ind][1];
    } else {
      inst.child_position = node.tip_position();
      inst.child_radius = 0.0025f;
      if (atten_radius_by_length) {
        inst.child_radius *= (node.length / length_scale);
      }

      child_right = self_right;
      child_up = self_up;
    }

    ProceduralTreeRootsRenderer::encode_directions(
      self_right, self_up, child_right, child_up, &inst.directions0, &inst.directions1);
  }
}

void create_roots_drawable(const Internode* inodes, int num_nodes,
                           ProceduralTreeRootsRenderer::DrawableHandle* drawable,
                           ProceduralTreeRootsRenderer& renderer,
                           const ProceduralTreeRootsRenderer::AddResourceContext& renderer_ctx) {
  if (num_nodes == 0) {
    return;
  }

  Temporary<Mat3f, 2048> store_frames;
  Temporary<ProceduralTreeRootsRenderer::Instance, 2048> store_instances;

  auto* frames = store_frames.require(num_nodes);
  auto* insts = store_instances.require(num_nodes);

  compute_node_frames(inodes, num_nodes, frames);
  to_render_instances(inodes, frames, num_nodes, false, 1.0f, insts);

  if (!drawable->is_valid()) {
    *drawable = renderer.create(ProceduralTreeRootsRenderer::DrawableType::NoWind);
  }

  renderer.fill_activate(renderer_ctx, *drawable, insts, num_nodes);
}

Vec3f get_origin(const LSystemComponent& comp, const Terrain& terrain) {
  auto res = comp.origin;
  if (comp.lock_to_terrain) {
    res.y = terrain.height_nearest_position_xz(res);
  }
  return res;
}

} //  anon

LSystemComponent* ls::create_lsystem_component() {
  auto* res = new LSystemComponent();
  res->src_file_path = std::string{GROVE_ASSET_DIR} + "/lsystem/branch.txt";
  return res;
}

void ls::destroy_lsystem_component(LSystemComponent** comp) {
  delete *comp;
  *comp = nullptr;
}

void ls::update_lsystem_component(LSystemComponent* comp, const LSystemComponentUpdateInfo& info) {
  if (comp->need_regen_execution_context) {
    Stopwatch stopwatch;
    if (auto src = read_text_file(comp->src_file_path.c_str())) {
      comp->debug_execution_pipeline = create_execution_pipeline(src.value());
      if (comp->debug_execution_pipeline) {
        auto& pipe = comp->debug_execution_pipeline.value();
        comp->debug_execution_context = create_execution_context(pipe);
        comp->gen_execution_context_ms = float(stopwatch.delta().count() * 1e3);
      }
    }
    comp->need_regen_execution_context = false;
  }

  if (comp->need_run_system && comp->debug_execution_pipeline &&
      comp->debug_execution_context) {

    if (comp->use_rand_seed) {
      globals.rand_stream.seed(comp->rand_seed);
    }

    Stopwatch stopwatch;
    auto derive_res = run_system(&comp->debug_execution_context.value(), comp->num_steps);
    if (derive_res) {
      comp->latest_derive_result = std::move(derive_res.value());
      comp->latest_derive_result_repr = debug_repr_derived_str(
        comp->debug_execution_pipeline.value(),
        comp->latest_derive_result.str,
        comp->latest_derive_result.str_data,
        comp->hide_module_contents_in_repr);
      comp->derive_ms = float(stopwatch.delta().count() * 1e3);

      stopwatch.reset();
      auto view_res = to_deriving_string(comp->latest_derive_result);
      auto branch_ctx = to_extract_builtin_module_params(comp->debug_execution_pipeline.value());
      auto extracted_mods = extract_builtin_modules(view_res, branch_ctx);
      auto inodes = build_tree(extracted_mods.data(), uint32_t(extracted_mods.size()));
      assert(internode_relationships_valid(inodes.data(), int(inodes.size())));
      comp->build_tree_ms = float(stopwatch.delta().count() * 1e3);

      //  create mesh
      stopwatch.reset();
      apply_length_scale(inodes.data(), int(inodes.size()), comp->length_scale);
      assign_position(inodes.data(), int(inodes.size()), get_origin(*comp, info.terrain));
      auto diam_params = make_assign_diameter_params(comp->leaf_diameter, comp->diameter_power);
      assign_diameter(inodes.data(), int(inodes.size()), diam_params);
      create_roots_drawable(
        inodes.data(), int(inodes.size()), &comp->debug_drawable,
        info.roots_renderer, info.roots_renderer_context);
      comp->gen_mesh_ms = float(stopwatch.delta().count() * 1e3);

      comp->debug_internodes = std::move(inodes);
    }
    comp->need_run_system = false;
  }

  if (comp->draw_node_bounds) {
    for (auto& node : comp->debug_internodes) {
      auto obb = make_node_obb(node);
      vk::debug::draw_obb3(obb, Vec3f{1.0f, 0.0f, 0.0f});
    }
  }
}

void ls::render_lsystem_component_gui(LSystemComponent* comp) {
  ImGui::Begin("LS");

  {
    char text[1024];
    memset(text, 0, 1024);
    if (ImGui::InputText("SysP", text, 1024, ImGuiInputTextFlags_EnterReturnsTrue)) {
      comp->src_file_path = std::string{GROVE_ASSET_DIR} + "/lsystem/" + std::string{text};
    }
  }

  if (ImGui::Button("GenExecutionContext")) {
    comp->need_regen_execution_context = true;
  }

  ImGui::Checkbox("UseRandSeed", &comp->use_rand_seed);

  int s = int(comp->rand_seed);
  if (ImGui::InputInt("Seed", &s)) {
    comp->rand_seed = uint32_t(s);
    comp->need_run_system = true;
  }

  if (ImGui::SliderFloat("LeafDiameter", &comp->leaf_diameter, 0.01f, 0.06f)) {
    comp->need_run_system = true;
  }
  if (ImGui::SliderFloat("DiameterPower", &comp->diameter_power, 0.5f, 2.5f)) {
    comp->need_run_system = true;
  }
  if (ImGui::SliderFloat("LengthScale", &comp->length_scale, 0.05f, 4.0f)) {
    comp->need_run_system = true;
  }
  ImGui::Checkbox("LockToTerrain", &comp->lock_to_terrain);
  ImGui::InputFloat3("Origin", &comp->origin.x);

  ImGui::Checkbox("DrawNodeBounds", &comp->draw_node_bounds);
  ImGui::Checkbox("HideModuleContents", &comp->hide_module_contents_in_repr);
  if (ImGui::InputInt("NumSteps", &comp->num_steps)) {
    comp->num_steps = clamp(comp->num_steps, 0, 128);
    comp->need_run_system = true;
  }

  if (comp->debug_execution_pipeline && comp->debug_execution_context) {
    if (ImGui::Button("RunSystem")) {
      comp->need_run_system = true;
    }
  }

  ImGui::Text("Num Modules: %d", int(comp->latest_derive_result.str.size()));
  ImGui::Text("Num Internodes: %d", int(comp->debug_internodes.size()));
  ImGui::Text("%s", comp->latest_derive_result_repr.c_str());
  ImGui::Text("GenSysIn: %0.3fms", comp->gen_execution_context_ms);
  ImGui::Text("DeriveIn: %0.3fms", comp->derive_ms);
  ImGui::Text("BuiltTreeIn: %0.3fms", comp->build_tree_ms);
  ImGui::Text("GenMeshIn: %0.3fms", comp->gen_mesh_ms);
  ImGui::End();
}

GROVE_NAMESPACE_END
