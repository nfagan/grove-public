#pragma once

#include "common.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove::ls {

class StringRegistry;
struct ParseResult;
struct ResolveResult;

struct DumpContext {
  ArrayView<const AstNode> nodes;
  ArrayView<const TypeNode> type_nodes;
  ArrayView<const uint32_t> parameters;
  ArrayView<const uint32_t> subscripts;
  ArrayView<const uint32_t> statement_blocks;
  ArrayView<const uint32_t> module_strings;
  ArrayView<const uint32_t> rules;
  ArrayView<const uint32_t> systems;
  ArrayView<const uint32_t> modules;
  ArrayView<const ModuleField> module_fields;
  ArrayView<const StorageLocation> storage;
  const StringRegistry* registry{};
  uint32_t float_t;
  uint32_t int_t;
  uint32_t bool_t;
  int tab{};
  bool parens_expr{true};
  bool hide_module_contents{};
};

std::string dump_system(uint32_t sys, DumpContext* ctx);
std::string dump_rule(uint32_t ri, DumpContext* ctx);
std::string dump_module_bytes(const uint8_t* data, uint32_t ti, DumpContext* ctx);

DumpContext to_dump_context(const ParseResult& parse_res,
                            const ResolveResult& resolve_res,
                            const StringRegistry* registry);

}