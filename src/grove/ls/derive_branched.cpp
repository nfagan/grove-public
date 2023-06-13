#include "derive_branched.hpp"
#include "derive.hpp"
#include "resolve.hpp"
#include "interpret.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

struct RuleMatch {
  bool is_pred;
  bool is_first_pred;
  uint32_t rule_index;
  uint32_t rule_arg_begin;
  uint32_t rule_arg_size;
  uint32_t rule_pred_offset;
  uint32_t rule_pred_size;
};

void find_pred(const RuleParameter* params, uint32_t param_size, uint32_t* first_pred,
               uint32_t* pred_size) {
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
  assert(*pred_size > 0);
}

Optional<uint32_t> look_ahead_n(const DeriveContext* ctx, const uint32_t* str, uint32_t str_size,
                                uint32_t str_p, uint32_t n) {
  //  find nth character of string on this axis, looking ahead from str_p.
  //  Return null if reach a stopping point (end of string or branch out) before n.
  assert(str[str_p] != ctx->branch_in_t && str[str_p] != ctx->branch_out_t);

  uint32_t i{};
  int depth{};
  while (str_p + 1 < str_size && i < n) {
    uint32_t s = str[++str_p];
    if (s == ctx->branch_in_t) {
      assert(str_p < str_size);
      depth++;
    } else if (s == ctx->branch_out_t) {
      if (depth == 0) {
        //  Reached end of branch.
        return NullOpt{};
      } else {
        --depth;
      }
    } else if (depth == 0) {
      i++;
    }
  }

  return i == n ? Optional<uint32_t>(str_p) : NullOpt{};
}

Optional<uint32_t> look_back_n(const DeriveContext* ctx, const uint32_t* str,
                               uint32_t str_p, uint32_t n) {
  //  find nth character of string on this axis, looking back from str_p.
  //  Return null if reach a stopping point (0 or branch in) before n.
  assert(str[str_p] != ctx->branch_in_t && str[str_p] != ctx->branch_out_t);

  uint32_t i{};
  int depth{};
  while (str_p && i < n) {
    uint32_t s = str[--str_p];
    if (s == ctx->branch_out_t) {
      assert(str_p > 0);
      depth++;
    } else if (s == ctx->branch_in_t) {
      if (depth == 0) {
        //  Reached start of branch.
        return NullOpt{};
      } else {
        --depth;
      }
    } else if (depth == 0) {
      i++;
    }
  }

  return i == n ? Optional<uint32_t>(str_p) : NullOpt{};
}

auto match(DeriveContext* ctx, const DerivingString* deriving_str) {
  struct Result {
    std::vector<RuleMatch> matches;
  };

  const uint32_t* str = deriving_str->str;
  const uint32_t str_size = deriving_str->str_size;

  //  For each module in the string, potential_matches[i].is_pred is true if the module will be
  //  spliced out in the next derivation step. Otherwise, it will be copied forward.
  //  potential_matches[i].is_first_pred is true if the module is the first predecessor argument
  //  to a rule, in which case the rule should be executed.
  std::vector<RuleMatch> potential_matches(str_size);

  int branch_depth{};
  for (uint32_t str_p = 0; str_p < str_size; str_p++) {
    if (str[str_p] == ctx->branch_in_t) {
      assert(str_p > 0 && str_p + 2 < str_size && str[str_p + 1] != ctx->branch_out_t);
      branch_depth++;
      continue;
    } else if (str[str_p] == ctx->branch_out_t) {
      assert(branch_depth > 0);
      branch_depth--;
      continue;
    }

    if (potential_matches[str_p].is_pred) {
      continue;
    }

    Optional<RuleMatch> best_match;
    for (uint32_t ri = 0; ri < ctx->num_rules; ri++) {
      auto& rule_params = ctx->rule_param_spans[ri];
      assert(rule_params.size > 0);
      const auto* rule_p = ctx->rule_params + rule_params.begin;

      uint32_t pred_off{};
      uint32_t pred_size{};
      find_pred(rule_p, rule_params.size, &pred_off, &pred_size);
      auto pre_p = look_back_n(ctx, str, str_p, pred_off);
      if (!pre_p) {
        continue;
      }

      uint32_t i;
      for (i = 0; i < rule_params.size; i++) {
        Optional<uint32_t> curr_p = look_ahead_n(ctx, str, str_size, pre_p.value(), i);
        if (!curr_p || str[curr_p.value()] != rule_p[i].type) {
          break;
        }
      }

      const bool matched = i == rule_params.size;
      if (matched && (!best_match || rule_params.size > best_match.value().rule_arg_size)) {
        RuleMatch rule_match{};
        rule_match.rule_index = ri;
        rule_match.rule_arg_begin = pre_p.value();
        rule_match.rule_arg_size = rule_params.size;
        rule_match.rule_pred_offset = pred_off;
        rule_match.rule_pred_size = pred_size;
        best_match = rule_match;
      }
    }

    if (best_match) {
      RuleMatch match = best_match.value(); //  @note by value
      match.is_pred = true;
      match.is_first_pred = true;

      for (uint32_t i = 0; i < match.rule_pred_size; i++) {
        uint32_t n = i + match.rule_pred_offset;
        Optional<uint32_t> dst_p = look_ahead_n(ctx, str, str_size, match.rule_arg_begin, n);
        assert(dst_p && !potential_matches[dst_p.value()].is_pred);
        potential_matches[dst_p.value()] = match;
        match.is_first_pred = false;
      }
    }
  }

  Result result{};
  result.matches = std::move(potential_matches);
  return result;
}

void append_successor_str(DeriveResult& result, const InterpretResult& interp_res) {
  auto& dst_str = result.str;
  auto& dst_data = result.str_data;

  const size_t curr_str_size = dst_str.size();
  dst_str.resize(curr_str_size + interp_res.succ_str_size);
  const size_t copy_size = interp_res.succ_str_size * sizeof(uint32_t);
  memcpy(dst_str.data() + curr_str_size, interp_res.succ_str, copy_size);

  const size_t curr_data_size = dst_data.size();
  dst_data.resize(curr_data_size + interp_res.succ_str_data_size);
  memcpy(dst_data.data() + curr_data_size, interp_res.succ_str_data, interp_res.succ_str_data_size);
}

} //  anon

ls::DeriveResult ls::derive_branched(DeriveContext* ctx, const DerivingString* deriving_str) {
  auto match_res = match(ctx, deriving_str);

  const uint32_t* str = deriving_str->str;
  const uint32_t str_size = deriving_str->str_size;
  const uint8_t* str_data = deriving_str->str_data;
  const uint32_t str_data_size = deriving_str->str_data_size;
  (void) str_data_size;

  std::vector<size_t> module_data_sizes(str_size);
  std::vector<size_t> module_data_offsets(str_size);
  {
    size_t cum_off{};
    for (uint32_t i = 0; i < str_size; i++) {
      module_data_sizes[i] = module_type_size(ctx->type_nodes, ctx->storage, str[i]).unwrap();
      module_data_offsets[i] = cum_off;
      cum_off += module_data_sizes[i];
    }
  }

  ls::DeriveResult result;
  auto& dst_str = result.str;
  auto& dst_data = result.str_data;

  size_t str_data_p{};
  for (uint32_t str_p = 0; str_p < str_size; str_p++) {
    const auto& match_info = match_res.matches[str_p];
    auto mod_size = module_data_sizes[str_p];
    assert(str_data_p + mod_size <= str_data_size);

    if (!match_info.is_pred) {
      //  Not a predecessor, so copy to derived string.
      assert(!match_info.is_first_pred);
      dst_str.push_back(str[str_p]);
      size_t dst_off = dst_data.size();
      dst_data.resize(dst_data.size() + mod_size);
      memcpy(dst_data.data() + dst_off, str_data + str_data_p, mod_size);

    } else if (match_info.is_first_pred) {
      //  Evaluate the rule and produce the successor string.
      assert(match_info.rule_index < ctx->num_rules);
      auto& instr_span = ctx->rule_instruction_spans[match_info.rule_index];
      const uint8_t* rule_inst = ctx->rule_instructions + instr_span.begin;
      const uint32_t rule_inst_size = instr_span.size;
      auto& rule_scope = ctx->scopes[ctx->rule_si[match_info.rule_index]];

      //  Copy the module arguments from the existing string's data into the rule's stack frame.
      uint32_t rule_off = rule_scope.stack_offset;
      for (uint32_t i = 0; i < match_info.rule_arg_size; i++) {
        Optional<uint32_t> curr_p = look_ahead_n(ctx, str, str_size, match_info.rule_arg_begin, i);
        assert(curr_p);
        auto curr_mod_size = module_data_sizes[curr_p.value()];
        assert(rule_off + curr_mod_size <= ctx->frame_size);
        memcpy(ctx->frame + rule_off, str_data + module_data_offsets[curr_p.value()], curr_mod_size);
        rule_off += uint32_t(curr_mod_size);
      }

      //  Evaluate the rule.
      auto interp_ctx = make_interpret_context(
        ctx->frame, ctx->frame_size, ctx->stack, uint32_t(ctx->stack_size));
      auto interp_res = interpret(&interp_ctx, rule_inst, rule_inst_size);
      assert(interp_res.ok && interp_res.match);
      append_successor_str(result, interp_res);
    }

    str_data_p += mod_size;
  }

  return result;
}

GROVE_NAMESPACE_END
