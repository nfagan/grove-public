#pragma once

#include "common.hpp"
#include <vector>

namespace grove::ls {

struct DeriveContext {
  const Scope* scopes;
  const TypeNode* type_nodes;
  const StorageLocation* storage;
  uint32_t num_rules;
  const RuleParameter* rule_params;
  const Span* rule_param_spans;
  const uint8_t* rule_instructions;
  const Span* rule_instruction_spans;
  const uint32_t* rule_si;
  uint8_t* frame;
  uint32_t frame_size;
  uint8_t* stack;
  uint32_t stack_size;

  uint32_t branch_in_t;
  uint32_t branch_out_t;
};

struct ResultStringSpans {
  Span str;
  Span data;
};

struct DeriveResult {
  std::vector<uint32_t> str;
  std::vector<uint8_t> str_data;
  std::vector<uint32_t> result_strs;
  std::vector<uint8_t> result_str_datas;
  std::vector<ResultStringSpans> result_spans;
};

DeriveResult derive(DeriveContext* ctx, const DerivingString* str);

}