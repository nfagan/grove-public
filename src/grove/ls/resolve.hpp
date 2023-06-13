#pragma once

#include "common.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Optional.hpp"
#include <unordered_set>

namespace grove::ls {

class StringRegistry;
struct ParseResult;
struct ResolveParams;
struct ResolveResult;

struct ResolveError {
  std::string message;
  uint32_t token;
};

struct ResolveContext {
  const ResolveParams* params;
  ResolveResult* result;
  std::vector<Scope> scopes;
  std::vector<TypeNode> type_nodes;
  std::vector<StorageLocation> storage_locations;
  std::vector<ModuleField> module_fields;
  std::vector<uint32_t> type_node_refs;
  std::unordered_map<uint32_t, uint32_t> scopes_by_node;
  std::unordered_map<uint32_t, uint32_t> types_by_node;
  std::unordered_set<PendingForeignFunction,
    PendingForeignFunction::Hash,
    PendingForeignFunction::Equal> pending_foreign_functions;

  uint32_t root_scope;
  uint32_t scope_range;

  uint32_t branch_in_t;
  uint32_t branch_out_t;
  uint32_t float_t;
  uint32_t bool_t;
  uint32_t int_t;
  uint32_t v3_t;
  uint32_t void_t;
};

struct ResolveResult {
  std::vector<ResolveError> errors;
  uint32_t root_scope;
  std::vector<Scope> scopes;
  std::vector<TypeNode> type_nodes;
  std::vector<StorageLocation> storage_locations;
  std::vector<ModuleField> module_fields;
  std::vector<uint32_t> type_node_refs;
  std::unordered_map<uint32_t, uint32_t> scopes_by_node;
  std::unordered_map<uint32_t, uint32_t> types_by_node;
  std::unordered_set<PendingForeignFunction,
                     PendingForeignFunction::Hash,
                     PendingForeignFunction::Equal> pending_foreign_functions;
  uint32_t scope_range;

  uint32_t branch_in_t;
  uint32_t branch_out_t;
  uint32_t float_t;
  uint32_t bool_t;
  uint32_t int_t;
  uint32_t v3_t;
  uint32_t void_t;
};

struct ResolveParams {
  ArrayView<const AstNode> nodes;
  ArrayView<const uint32_t> parameters;
  ArrayView<const uint32_t> subscripts;
  ArrayView<const uint32_t> statement_blocks;
  ArrayView<const uint32_t> module_strings;
  ArrayView<const uint32_t> rules;
  ArrayView<const uint32_t> systems;
  ArrayView<const uint32_t> modules;
  ArrayView<const uint32_t> axioms;
  ArrayView<const uint32_t> module_meta_type_labels;

  ArrayView<const ModuleFieldDescriptor> module_meta_type_fields;
  ArrayView<const ModuleDescriptor> module_meta_types;

  StringRegistry* registry;
  TypeIDStore* type_ids;
};

bool init_resolve_context(ResolveContext* ctx, const ResolveParams* params);
ResolveResult resolve(ResolveContext* context);
ResolveParams to_resolve_params(const ParseResult& res, StringRegistry* registry,
                                TypeIDStore* store);

bool lookup_variable(Scope* scopes, uint32_t si, StringRef name,
                     Variable** var, uint32_t* in_scope);
bool lookup_variable(const Scope* scopes, uint32_t si, StringRef name,
                     const Variable** var, uint32_t* in_scope);
Optional<uint32_t> lookup_type(const Scope* scopes, uint32_t si, StringRef name);
Optional<uint32_t> lookup_field(const ModuleField* fields, StringRef name,
                                uint32_t f_beg, uint32_t f_size);

//  Returns 0-based index of module field, i.e., returns i, where i is the i-th field of the module.
//  For indexing into fields, add mod_ty.field_begin
Optional<uint32_t> get_module_field_index(const TypeNode::Module& mod_ty, const ModuleField* fields,
                                          StringRef name);

uint32_t type_size(const TypeNode* type_nodes, const StorageLocation* locations, uint32_t ti);
Optional<uint32_t> module_type_size(const TypeNode* type_nodes,
                                    const StorageLocation* locations, uint32_t ti);
Optional<uint32_t> sum_module_type_sizes(const TypeNode* type_nodes,
                                         const StorageLocation* locations,
                                         const uint32_t* tis, uint32_t num_tis);
bool is_module_with_meta_type(const TypeNode* types, uint32_t ti, StringRef meta_label);
bool is_function_type(const TypeNode* type_nodes, const uint32_t* type_node_refs, uint32_t ti,
                      const uint32_t* arg_tis, uint32_t num_args, uint32_t result_ti);

bool get_rule_parameter_info(const AstNode::Rule& rule_node, const AstNode* nodes,
                             const uint32_t* params, const Scope* scopes, uint32_t si,
                             RuleParameter* info);

bool read_module_field(const void* data, const TypeNode* types, const StorageLocation* locations,
                       const ModuleField* fields, uint32_t ti, uint32_t fi,
                       uint32_t expect_field_type, uint32_t expect_field_size, void* dst);

bool write_module_field(void* data, const TypeNode* types, const StorageLocation* locations,
                        const ModuleField* fields, uint32_t ti, uint32_t fi,
                        uint32_t expect_field_type, uint32_t expect_field_size, const void* src);

bool read_module_fieldf(const void* data, const TypeNode* types, uint32_t float_ti,
                        const StorageLocation* locations, const ModuleField* fields,
                        uint32_t ti, uint32_t fi, float* dst);

}