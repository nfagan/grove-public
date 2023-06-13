#pragma once

#include "common.hpp"

namespace grove::ls {

struct MatchContext {
  const uint32_t* str_tis;
  uint32_t str_size;
  const RuleParameter* rule_parameters; //  all parameters stored contiguously
  const Span* rule_spans;               //  each span is a rule header, e.g. rule (a: A, b: B)
  uint32_t num_rules;
  uint32_t branch_in_t;
  uint32_t branch_out_t;
};

uint32_t match(const MatchContext& context, StringSplice* dst_splices);

}