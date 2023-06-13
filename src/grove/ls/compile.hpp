#pragma once

#include "common.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove::ls {

struct ParseResult;
struct ResolveResult;

struct CompileParams {
  using ForeignFunctions = std::unordered_map<
    PendingForeignFunction,
    ForeignFunction*,
    PendingForeignFunction::Hash,
    PendingForeignFunction::Equal>;

  ArrayView<const AstNode> nodes;
  ArrayView<const TypeNode> type_nodes;
  ArrayView<const StorageLocation> storage;
  ArrayView<const ModuleField> module_fields;
  ArrayView<const Scope> scopes;
  ArrayView<const uint32_t> statement_blocks;
  ArrayView<const uint32_t> subscripts;
  ArrayView<const uint32_t> module_strings;
  ArrayView<const uint32_t> type_node_refs;
  const std::unordered_map<uint32_t, uint32_t>* scopes_by_node;
  const std::unordered_map<uint32_t, uint32_t>* types_by_node;
  const ForeignFunctions* foreign_functions;

  uint32_t branch_in_t;
  uint32_t branch_out_t;
  uint32_t float_t;
  uint32_t v3_t;
  uint32_t void_t;
};

struct CompileResult {
  std::vector<uint8_t> instructions;
};

CompileResult compile_rule(const CompileParams* params, uint32_t ri);
CompileResult compile_axiom(const CompileParams* params, uint32_t ai);
CompileParams to_compile_params(const ParseResult& parse_res,
                                const ResolveResult& resolve_res,
                                const CompileParams::ForeignFunctions* foreign_funcs);

}