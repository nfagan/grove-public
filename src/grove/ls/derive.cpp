#include "derive.hpp"
#include "match.hpp"
#include "resolve.hpp"
#include "interpret.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

struct CopyResultString {
  std::vector<uint8_t> str_data;
  std::vector<uint32_t> str;
  std::vector<ResultStringSpans> spans;
};

Span add_result_str(std::vector<uint32_t>* dst,
                    const uint8_t* str,
                    uint32_t str_size) {
  auto dst_str_beg = dst->size();
  dst->resize(dst->size() + str_size);
  if (str_size > 0) {
    memcpy(dst->data() + dst_str_beg, str, str_size * sizeof(uint32_t));
  }
  return Span{uint32_t(dst_str_beg), str_size};
}

Span add_result_str_data(std::vector<uint8_t>* dst,
                         const uint8_t* str_data,
                         uint32_t str_data_size) {
  auto dst_dat_beg = dst->size();
  dst->resize(dst->size() + str_data_size);
  if (str_data_size > 0) {
    memcpy(dst->data() + dst_dat_beg, str_data, str_data_size);
  }
  return Span{uint32_t(dst_dat_beg), str_data_size};
}

uint32_t sum_module_type_sizes(const DeriveContext* ctx, const uint32_t* str, uint32_t str_size) {
  return sum_module_type_sizes(
    ctx->type_nodes,
    ctx->storage,
    str,
    str_size).unwrap();
}

CopyResultString interpret(DeriveContext* ctx, const DerivingString* str,
                           const StringSplice* splices, uint32_t num_matches,
                           DeriveResult* result) {
  CopyResultString copy_succ_str;
  for (uint32_t i = 0; i < num_matches; i++) {
    auto& splice = splices[i];
    assert(splice.size > 0);
    auto& instr_span = ctx->rule_instruction_spans[splice.rule];
    const uint8_t* rule_inst = ctx->rule_instructions + instr_span.begin;
    const uint32_t rule_inst_size = instr_span.size;
    auto& rule_scope = ctx->scopes[ctx->rule_si[splice.rule]];

    const uint32_t str_off =
      grove::sum_module_type_sizes(ctx, str->str, splice.str_begin);
    const uint32_t str_sz =
      grove::sum_module_type_sizes(ctx, str->str + splice.str_begin, splice.size);
    assert(str_sz <= rule_scope.stack_size);

    const uint32_t rule_off = rule_scope.stack_offset;
    assert(str_off + str_sz <= str->str_data_size);
    assert(rule_off + str_sz <= ctx->frame_size);
    memcpy(ctx->frame + rule_off, str->str_data + str_off, str_sz);

    auto interp_ctx = make_interpret_context(
      ctx->frame,
      ctx->frame_size,
      ctx->stack,
      uint32_t(ctx->stack_size));
    auto interp_res = interpret(&interp_ctx, rule_inst, rule_inst_size);

    ResultStringSpans succ_spans{};
    succ_spans.str = add_result_str(
      &copy_succ_str.str,
      interp_res.succ_str,
      interp_res.succ_str_size);
    succ_spans.data = add_result_str_data(
      &copy_succ_str.str_data,
      interp_res.succ_str_data,
      interp_res.succ_str_data_size);
    copy_succ_str.spans.push_back(succ_spans);

    ResultStringSpans res_spans{};
    res_spans.str = add_result_str(
      &result->result_strs, interp_res.res_str, interp_res.res_str_size);
    res_spans.data = add_result_str_data(
      &result->result_str_datas, interp_res.res_str_data, interp_res.res_str_data_size);
    result->result_spans.push_back(res_spans);
  }
  return copy_succ_str;
}

void splice_string(DeriveContext* ctx, const DerivingString* str,
                   const StringSplice* splices, uint32_t num_matches,
                   const CopyResultString* copy_succ_str,
                   DeriveResult* result) {
  for (uint32_t i = 0; i < num_matches; i++) {
    const uint32_t prev_end = i == 0 ? 0 :
      splices[i-1].str_begin + splices[i-1].size;
    const uint32_t str_beg = splices[i].str_begin;
    const uint32_t copy_str_off =
      grove::sum_module_type_sizes(ctx, str->str, prev_end);
    const uint32_t copy_str_sz =
      grove::sum_module_type_sizes(ctx, str->str + prev_end, str_beg - prev_end);
    //  Copy old string types
    for (uint32_t j = prev_end; j < str_beg; j++) {
      result->str.push_back(str->str[j]);
    }
    //  Copy old string data
    auto str_data_beg = result->str_data.size();
    result->str_data.resize(result->str_data.size() + copy_str_sz);
    if (copy_str_sz > 0) {
      memcpy(result->str_data.data() + str_data_beg, str->str_data + copy_str_off, copy_str_sz);
    }
#ifdef GROVE_DEBUG
    {
      auto next_sz = grove::sum_module_type_sizes(ctx, result->str.data(), result->str.size());
      assert(next_sz == result->str_data.size());
    }
#endif

    //  Splice in new types
    auto& spans = copy_succ_str->spans[i];
    auto& succ_span = spans.str;
    auto& succ_data_span = spans.data;
    result->str.insert(
      result->str.end(),
      copy_succ_str->str.data() + succ_span.begin,
      copy_succ_str->str.data() + succ_span.begin + succ_span.size);
    result->str_data.insert(
      result->str_data.end(),
      copy_succ_str->str_data.data() + succ_data_span.begin,
      copy_succ_str->str_data.data() + succ_data_span.begin + succ_data_span.size);
#ifdef GROVE_DEBUG
    {
      auto next_sz = grove::sum_module_type_sizes(ctx, result->str.data(), result->str.size());
      assert(next_sz == result->str_data.size());
    }
#endif
  }
}

auto match(DeriveContext* ctx, const DerivingString* str, uint32_t* num_matches) {
  std::vector<StringSplice> splices(str->str_size);
  MatchContext match_ctx{};
  match_ctx.str_tis = str->str;
  match_ctx.str_size = str->str_size;
  match_ctx.rule_parameters = ctx->rule_params;
  match_ctx.rule_spans = ctx->rule_param_spans;
  match_ctx.num_rules = ctx->num_rules;
  match_ctx.branch_in_t = ctx->branch_in_t;
  match_ctx.branch_out_t = ctx->branch_out_t;
  *num_matches = match(match_ctx, splices.data());
  return splices;
}

} //  anon

DeriveResult ls::derive(DeriveContext* ctx, const DerivingString* str) {
  DeriveResult result;
  uint32_t num_matches;
  auto splices = grove::match(ctx, str, &num_matches);
  auto copy_succ_str = grove::interpret(ctx, str, splices.data(), num_matches, &result);
  splice_string(ctx, str, splices.data(), num_matches, &copy_succ_str, &result);
  return result;
}

GROVE_NAMESPACE_END
