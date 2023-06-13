#include "logging.hpp"
#include "common.hpp"
#include "fs.hpp"
#include <iostream>
#include <ctime>
#include <string>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {
Log* global_logger_instance = nullptr;

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
[[maybe_unused]] std::string time_now_as_string() {
  char buffer[128];
  std::time_t time_result = std::time(nullptr);
  const char* format = "%b %d %H:%M:%S";
  std::strftime(&buffer[0], 128, format, std::localtime(&time_result));
  return std::string(buffer);
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

bool meta_has_required_message_components(const Log::MetaData& meta) {
  return meta.tag && meta.function && meta.file;
}

std::string maybe_make_meta_string(const Log::MetaData& meta, const char* message) {
  if (meta_has_required_message_components(meta)) {
    std::string msg("(");
    msg += meta.function;
    msg += ", ";

    if (meta.file_name_only) {
      msg += fs::file_name(meta.file);
    } else {
      msg += meta.file;
    }

    msg += ":";
    msg += std::to_string(meta.line);
    msg += "): ";
    msg += message;

    return msg;
  } else {
    return {};
  }
}

template <typename T>
inline void cout_log(const char* kind, T&& message) {
#if 1
  std::cout << time_now_as_string() << " | " << kind << ": "
            << std::forward<T>(message) << std::endl;
#else
  (void) kind;
  (void) message;
#endif
}

} //  anon

const char* Log::tag_string(grove::Log::Tag tag) {
  switch (tag) {
    case Tag::Profile:
      return "profile";
    default:
      assert(false);
      return "";
  }
}

/*
 * Meta
 */

Log::MetaData::MetaData(const char* tag, const char* func,
                        const char* file, int line) :
  tag(tag),
  function(func),
  file(file),
  line(line),
  file_name_only(true) {
  //
}

Log::MetaData::MetaData(const char* tag) :
  MetaData(tag, nullptr, nullptr, 0) {
  //
}

Log::MetaData::MetaData(grove::Log::Tag tag) :
  MetaData(tag_string(tag)) {
  //
}

/*
 * Log
 */

void Log::error(const char* message) const {
  cout_log("ERROR", message);
}

void Log::info(const char* message) const {
  cout_log("INFO", message);
}

void Log::warning(const char* message) const {
  cout_log("WARNING", message);
}

void Log::error(const char* message, const grove::Log::MetaData& meta) const {
  auto maybe_str = maybe_make_meta_string(meta, message);
  if (maybe_str.empty()) {
    error(message);
  } else {
    error(maybe_str.c_str());
  }
}

void Log::info(const char* message, const grove::Log::MetaData& meta) const {
  auto maybe_str = maybe_make_meta_string(meta, message);
  if (maybe_str.empty()) {
    info(message);
  } else {
    info(maybe_str.c_str());
  }
}

void Log::warning(const char* message, const grove::Log::MetaData& meta) const {
  auto maybe_str = maybe_make_meta_string(meta, message);
  if (maybe_str.empty()) {
    warning(message);
  } else {
    warning(maybe_str.c_str());
  }
}

void Log::severe(const char* message, const MetaData& meta) const {
  auto maybe_str = maybe_make_meta_string(meta, message);
  if (maybe_str.empty()) {
    cout_log("SEVERE", message);
  } else {
    cout_log("SEVERE", maybe_str);
  }
}

Log* Log::create_default_global_instance() {
  delete_default_global_instance();
  global_logger_instance = new Log();
  return global_logger_instance;
}

Log* Log::require_global_instance() {
  if (global_logger_instance == nullptr) {
    return create_default_global_instance();
  }
  
  return global_logger_instance;
}

void Log::delete_default_global_instance() {
  delete global_logger_instance;
}

void Log::set_global_instance(Log* logger) {
  global_logger_instance = logger;
}

Log* Log::get_global_instance() {
  return global_logger_instance;
}

GROVE_NAMESPACE_END
