#pragma once

#include <string>

namespace grove {

template <typename T>
class Optional;

namespace fs {

extern const char file_separator;

bool file_exists(const std::string& file_path);
std::string file_name(const std::string& file_path);
bool file_size(const std::string& file_path, size_t* size);
bool read_bytes(const std::string& file_path, void* data, size_t data_capacity, size_t* want_write);

}

std::string read_text_file(const char* file_path, bool* success);
Optional<std::string> read_text_file(const char* file_path);

bool write_text_file(const std::string& text, const char* file_path);

}
