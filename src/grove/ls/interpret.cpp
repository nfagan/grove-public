#include "interpret.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

template <typename T>
T readi(const uint8_t* insts, size_t inst_size, uint32_t* ip) {
  assert(*ip + sizeof(T) <= inst_size);
  (void) inst_size;
  T v;
  memcpy(&v, insts + *ip, sizeof(T));
  (*ip) += sizeof(T);
  return v;
}

template <typename T>
T reads(const uint8_t* stack, uint32_t* sp) {
  assert(*sp >= sizeof(T));
  (*sp) -= sizeof(T);
  T v;
  memcpy(&v, stack + *sp, sizeof(T));
  return v;
}

void reads_float2(const uint8_t* stack, uint32_t* sp, float* arg1, float* arg2) {
  *arg2 = reads<float>(stack, sp);
  *arg1 = reads<float>(stack, sp);
}

void reads_v3_2(const uint8_t* stack, uint32_t* sp, float* arg1, float* arg2) {
  arg2[2] = reads<float>(stack, sp);
  arg2[1] = reads<float>(stack, sp);
  arg2[0] = reads<float>(stack, sp);

  arg1[2] = reads<float>(stack, sp);
  arg1[1] = reads<float>(stack, sp);
  arg1[0] = reads<float>(stack, sp);
}

} //  anon

uint32_t ls::ith_return_string_ti(const uint8_t* str, uint32_t i) {
  uint32_t res;
  memcpy(&res, str + i * sizeof(uint32_t), sizeof(uint32_t));
  return res;
}

void ls::return_str_tis(const uint8_t* str, uint32_t n, uint32_t* out) {
  for (uint32_t i = 0; i < n; i++) {
    out[i] = ith_return_string_ti(str, i);
  }
}

InterpretContext ls::make_interpret_context(uint8_t* frame, uint32_t frame_size,
                                            uint8_t* stack, size_t stack_size) {
  InterpretContext res;
  res.frame = frame;
  res.frame_size = frame_size;
  res.stack = stack;
  res.stack_size = stack_size;
  return res;
}

InterpretResult ls::interpret(InterpretContext* context, const uint8_t* insts, size_t inst_size) {
  InterpretResult result{};
  uint32_t ip{};
  uint32_t sp{};
  uint8_t* stack = context->stack;
  const size_t stack_size = context->stack_size;
  (void) stack_size;
  uint8_t* frame = context->frame;
  while (ip < inst_size) {
    auto inst = insts[ip++];
    switch (inst) {
      case Instructions::load: {
        const auto off = readi<uint16_t>(insts, inst_size, &ip);
        const auto sz = readi<uint16_t>(insts, inst_size, &ip);
        assert(off + sz <= context->frame_size && sp + sz <= context->stack_size);
        memcpy(stack + sp, frame + off, sz);
        sp += sz;
        break;
      }
      case Instructions::store: {
        const auto off = readi<uint16_t>(insts, inst_size, &ip);
        const auto sz = readi<uint16_t>(insts, inst_size, &ip);
        assert(sp >= sz);
        memcpy(frame + off, stack + (sp - sz), sz);
        sp -= sz;
        break;
      }
      case Instructions::constantf: {
        assert(sp + sizeof(float) <= context->stack_size);
        const auto f = readi<float>(insts, inst_size, &ip);
        memcpy(stack + sp, &f, sizeof(float));
        sp += sizeof(float);
        break;
      }
      case Instructions::addf:
      case Instructions::subf:
      case Instructions::mulf:
      case Instructions::divf: {
        float a;
        float b;
        reads_float2(stack, &sp, &a, &b);
        float res;
        switch (inst) {
          case Instructions::addf:
            res = a + b;
            break;
          case Instructions::subf:
            res = a - b;
            break;
          case Instructions::mulf:
            res = a * b;
            break;
          case Instructions::divf:
            res = a / b;
            break;
          default:
            assert(false);
        }
        assert(sp + sizeof(float) <= stack_size);
        memcpy(stack + sp, &res, sizeof(float));
        sp += sizeof(float);
        break;
      }
      case Instructions::vop: {
        auto vec_len = readi<uint8_t>(insts, inst_size, &ip);
        assert(vec_len == 3); //  @TODO: v2 and v4
        (void) vec_len;

        float a[4];
        float b[4];
        float r[4];
        reads_v3_2(stack, &sp, a, b);
        const auto vi = readi<uint8_t>(insts, inst_size, &ip);
        switch (vi) {
          case Instructions::addf:
            r[0] = a[0] + b[0];
            r[1] = a[1] + b[1];
            r[2] = a[2] + b[2];
            break;
          case Instructions::subf:
            r[0] = a[0] - b[0];
            r[1] = a[1] - b[1];
            r[2] = a[2] - b[2];
            break;
          case Instructions::mulf:
            r[0] = a[0] * b[0];
            r[1] = a[1] * b[1];
            r[2] = a[2] * b[2];
            break;
          case Instructions::divf:
            r[0] = a[0] / b[0];
            r[1] = a[1] / b[1];
            r[2] = a[2] / b[2];
            break;
          default:
            assert(false);
        }
        assert(sp + sizeof(float) * vec_len <= stack_size);
        memcpy(stack + sp, r, sizeof(float) * vec_len);
        sp += sizeof(float) * vec_len;
        break;
      }
      case Instructions::testf:
      case Instructions::gtf:
      case Instructions::ltf:
      case Instructions::gef:
      case Instructions::lef: {
        float a;
        float b;
        reads_float2(stack, &sp, &a, &b);
        int32_t res;
        switch (inst) {
          case Instructions::gtf:
            res = int32_t(a > b);
            break;
          case Instructions::ltf:
            res = int32_t(a < b);
            break;
          case Instructions::gef:
            res = int32_t(a >= b);
            break;
          case Instructions::lef:
            res = int32_t(a <= b);
            break;
          case Instructions::testf:
            res = int32_t(a == b);
            break;
          default:
            assert(false);
        }
        assert(sp + sizeof(int32_t) <= stack_size);
        memcpy(stack + sp, &res, sizeof(int32_t));
        sp += sizeof(int32_t);
        break;
      }
      case Instructions::jump_if: {
        const auto cond = reads<int32_t>(stack, &sp);
        assert(cond == 1 || cond == 0);
        auto else_off = readi<uint16_t>(insts, inst_size, &ip);
        if (!cond) {
          ip = else_off;
        }
        break;
      }
      case Instructions::jump: {
        ip = readi<uint16_t>(insts, inst_size, &ip);
        break;
      }
      case Instructions::ret: {
        const auto match = readi<uint8_t>(insts, inst_size, &ip);
        const auto succ_str_sz_bytes = readi<uint32_t>(insts, inst_size, &ip);
        const auto succ_str_sz = readi<uint32_t>(insts, inst_size, &ip);
        const auto res_str_sz_bytes = readi<uint32_t>(insts, inst_size, &ip);
        const auto res_str_sz = readi<uint32_t>(insts, inst_size, &ip);
        assert(sp >= succ_str_sz_bytes + res_str_sz_bytes);
        assert(ip + succ_str_sz * sizeof(uint32_t) + res_str_sz * sizeof(uint32_t) <= inst_size);

        result.ok = true;
        result.match = match;
        result.succ_str = insts + ip;
        result.succ_str_size = succ_str_sz;
        result.succ_str_data = stack + (sp - (succ_str_sz_bytes + res_str_sz_bytes));
        result.succ_str_data_size = succ_str_sz_bytes;

        result.res_str = insts + ip + sizeof(uint32_t) * succ_str_sz;
        result.res_str_size = res_str_sz;
        result.res_str_data = stack + (sp - res_str_sz_bytes);
        result.res_str_data_size = res_str_sz_bytes;

        ip = uint32_t(inst_size);
        break;
      }
      case Instructions::call: {
        auto* ptr = (ForeignFunction*) readi<uint64_t>(insts, inst_size, &ip);
        const auto arg_sz = readi<uint16_t>(insts, inst_size, &ip);
        const auto ret_sz = readi<uint16_t>(insts, inst_size, &ip);
        assert(sp >= arg_sz && (sp - arg_sz + ret_sz) <= stack_size);
        sp -= arg_sz;
        ptr(arg_sz, ret_sz, stack + sp);
        sp += ret_sz;
        break;
      }
      default: {
        assert(false && "Unhandled.");
      }
    }
  }
  return result;
}

GROVE_NAMESPACE_END
