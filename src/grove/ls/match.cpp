#include "match.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

struct MatchResult {
  uint32_t next_pp;
  StringSplice splice;
};

bool find_pred(const RuleParameter* params, uint32_t param_size,
               uint32_t* first_pred, uint32_t* pred_size) {
  for (uint32_t i = 0; i < param_size; i++) {
    if (params[i].marked_pred) {
      *first_pred = i;
      while (i < param_size) {
        if (params[i++].marked_pred) {
          (*pred_size)++;
        } else {
          break;
        }
      }
      break;
    }
  }
  return *pred_size > 0;
}

Optional<MatchResult> match_rule(const uint32_t* str, uint32_t str_size,
                                 const RuleParameter* params, uint32_t param_size,
                                 uint32_t pp, uint32_t ri) {
  if (param_size == 0) {
    return NullOpt{};
  }

  uint32_t pred_size{};
  uint32_t first_pred{};
  if (!find_pred(params, param_size, &first_pred, &pred_size)) {
    assert(false && "No pred marked.");
    return NullOpt{};
  } else if (first_pred > pp) {
    return NullOpt{};
  }

  uint32_t strp = pp - first_pred;
  uint32_t paramp{};
  while (strp < str_size && paramp < param_size) {
    if (str[strp++] != params[paramp++].type) {
      return NullOpt{};
    }
  }
  if (paramp != param_size) {
    return NullOpt{};
  }
  MatchResult res{};
  res.splice.rule = ri;
  res.splice.param_begin = first_pred;
  res.splice.str_begin = pp - first_pred;
  res.splice.size = pred_size;
  res.next_pp = pp + pred_size;
  return Optional<MatchResult>(res);
}

} //  anon

uint32_t ls::match(const MatchContext& context, StringSplice* splices) {
  const uint32_t str_size = context.str_size;
  const uint32_t* str_tis = context.str_tis;
  const RuleParameter* rule_params = context.rule_parameters;
  const Span* rule_spans = context.rule_spans;
  const uint32_t num_rules = context.num_rules;
  assert(context.branch_in_t > 0 && context.branch_out_t > 0);

  uint32_t pp{};
  uint32_t num_splices{};
  while (pp < str_size) {
    Optional<StringSplice> candidate_splice;
    uint32_t candidate_size{};
    uint32_t candidate_pp{};
    for (uint32_t i = 0; i < num_rules; i++) {
      //  rule (a: A, b: B)
      //  `rule_param` points to `a`, `span_size` is 2.
      auto& span = rule_spans[i];
      const RuleParameter* rule_param = rule_params + span.begin;
      const uint32_t span_size = span.size;
      if (auto match_res = match_rule(str_tis, str_size, rule_param, span_size, pp, i)) {
        if (!candidate_splice || span_size > candidate_size) {
          candidate_splice = match_res.value().splice;
          candidate_pp = match_res.value().next_pp;
          candidate_size = span_size;
        }
      }
    }
    if (candidate_splice) {
      assert(num_splices < str_size);
      splices[num_splices++] = candidate_splice.value();
      pp = candidate_pp;
    } else {
      pp++;
    }
  }
  return num_splices;
}

GROVE_NAMESPACE_END
