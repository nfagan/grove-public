#include "fs.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"
#include "grove/common/platform.hpp"
#include "grove/common/config.hpp"
#include "grove/common/Optional.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

#if defined(GROVE_UNIX)
#include <sys/stat.h>
#elif defined(GROVE_WIN)
#include <windows.h>
#endif

GROVE_NAMESPACE_BEGIN

#ifdef GROVE_UNIX
const char fs::file_separator = '/';
#else
const char fs::file_separator = '\\';
#endif

#if defined(GROVE_UNIX)
bool fs::file_exists(const std::string& path) {
  struct stat sb;
  const int status = stat(path.c_str(), &sb);
  if (status != 0) {
    return false;
  }
  return (sb.st_mode & S_IFMT) == S_IFREG;
}
#elif defined(GROVE_WIN)
bool fs::file_exists(const std::string& path) {
  return !(INVALID_FILE_ATTRIBUTES == GetFileAttributes(path.c_str()) && 
         GetLastError() == ERROR_FILE_NOT_FOUND);
}
#else
#error "Expected one of Unix or Windows for OS."
#endif

namespace {
  std::string reverse(const std::string& a) {
    auto j = int(a.size()) - 1;
    std::string out;
    out.resize(a.size());

    for (int i = 0; i < int(a.size()); i++) {
      out[i] = a[j];
      j--;
    }

    return out;
  }
}

std::string fs::file_name(const std::string& file_path) {
  const auto rev = reverse(file_path);

  auto first_slash = std::find(rev.begin(), rev.end(), file_separator);

  if (first_slash == rev.end()) {
    return file_path;
  } else if (first_slash == rev.begin()) {
    return {};
  }

  auto offset = file_path.size() - (first_slash - rev.begin());
  return file_path.substr(offset, file_path.size() - offset);
}

bool fs::file_size(const std::string& file_path, size_t* size) {
  *size = 0;

  std::ifstream file;
  try {
    file.open(file_path.c_str(), std::ios_base::in | std::ios_base::binary);
  } catch (...) {
    return false;
  }

  if (!file.good()) {
    return false;
  }

  file.seekg(0, file.end);
  const int64_t length = file.tellg();
  file.seekg(0, file.beg);
  *size = size_t(length);
  return true;
}

bool fs::read_bytes(const std::string& file_path, void* data, size_t data_capacity,
                    size_t* want_write) {
  *want_write = 0;

  std::ifstream file;
  try {
    file.open(file_path.c_str(), std::ios_base::in | std::ios_base::binary);
  } catch (...) {
    return false;
  }

  if (!file.good()) {
    return false;
  }

  file.seekg(0, file.end);
  const int64_t length = file.tellg();
  file.seekg(0, file.beg);
  *want_write = size_t(length);

  if (*want_write <= data_capacity) {
    file.read(static_cast<char*>(data), length);
    return true;
  } else {
    return false;
  }
}

std::string read_text_file(const char* file_path, bool* success) {
  std::ifstream file;
  std::stringstream file_stream;
  
  *success = false;
  
  try {
    file.open(file_path);
    
    if (!file) {
#ifdef GROVE_DEBUG
      std::string msg("Failed to open file: ");
      msg += file_path;
      GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "");
#endif
      return "";
    }
    
    file_stream << file.rdbuf();
    file.close();
    std::string file_contents = file_stream.str();
    
    *success = true;
    
    return file_contents;
  } catch (...) {
    GROVE_LOG_ERROR_CAPTURE_META("An error occurred when attempting to read a file.", "");
    
    return "";
  }
}

Optional<std::string> read_text_file(const char* file_path) {
  bool success;
  auto res = read_text_file(file_path, &success);
  if (success) {
    return Optional<std::string>(std::move(res));
  } else {
    return NullOpt{};
  }
}

bool write_text_file(const std::string& text, const char* file_path) {
  std::ofstream file;

  try {
    file.open(file_path);

    if (!file) {
#ifdef GROVE_DEBUG
      std::string msg("Failed to open file: ");
      msg += file_path;
      GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), "");
#endif
      return false;
    }

    file << text;
    file.close();
    return true;

  } catch (...) {
    GROVE_LOG_ERROR_CAPTURE_META("An error occurred when attempting to write a file.", "");
    return false;
  }
}

GROVE_NAMESPACE_END
