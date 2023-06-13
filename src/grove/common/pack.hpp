#pragma once

#include <cstdint>

namespace grove::pack {

//  2 floats in [0.0f, 1.0f] to 1 uint32
uint32_t pack_2fn_1u32(float a, float b);
//  1 uint32 to 2 floats in [0.0f, 1.0f]
void unpack_1u32_2fn(uint32_t v, float* af, float* bf);

uint32_t pack_4u8_1u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void unpack_1u32_4u8(uint32_t v, uint8_t* a, uint8_t* b, uint8_t* c, uint8_t* d);

}