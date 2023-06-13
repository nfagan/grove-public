#pragma once

#include <memory>

namespace grove::io {

std::unique_ptr<float[]> read_float_array(const char* file, bool* success, uint64_t* size);

}