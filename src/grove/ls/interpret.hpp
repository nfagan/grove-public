#pragma once

#include "common.hpp"

namespace grove::ls {

struct InterpretContext {
  uint8_t* frame;
  size_t frame_size;
  uint8_t* stack;
  size_t stack_size;
};

struct InterpretResult {
  bool ok;
  bool match;
  const uint8_t* succ_str;
  uint32_t succ_str_size;
  const uint8_t* succ_str_data;
  size_t succ_str_data_size;
  const uint8_t* res_str_data;
  size_t res_str_data_size;
  const uint8_t* res_str;
  uint32_t res_str_size;
};

InterpretResult interpret(InterpretContext* context, const uint8_t* inst, size_t inst_size);
uint32_t ith_return_string_ti(const uint8_t* str, uint32_t i);
void return_str_tis(const uint8_t* str, uint32_t n, uint32_t* out);

InterpretContext make_interpret_context(uint8_t* frame,
                                        uint32_t frame_size,
                                        uint8_t* stack,
                                        size_t stack_size);

}