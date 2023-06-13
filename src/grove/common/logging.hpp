#pragma once

#include "config.hpp"

namespace grove {
  class Log;
}

class grove::Log {
public:
  enum class Tag {
    Profile
  };

  static const char* tag_string(Tag tag);

  class MetaData {
  public:
    MetaData() = delete;
    MetaData(const char* tag);
    MetaData(grove::Log::Tag tag);
    MetaData(const char* tag, const char* func, const char* file, int line);

  public:
    const char* tag;
    const char* function;
    const char* file;
    int line;
    bool file_name_only;
  };

public:
  virtual ~Log() = default;
  
  virtual void info(const char* message) const;
  virtual void error(const char* message) const;
  virtual void warning(const char* message) const;

  virtual void info(const char* message, const MetaData& meta) const;
  virtual void error(const char* message, const MetaData& meta) const;
  virtual void warning(const char* message, const MetaData& meta) const;
  virtual void severe(const char* message, const MetaData& meta) const;
  
  static void delete_default_global_instance();
  static void set_global_instance(Log* logger);
  static Log* get_global_instance();
  static Log* require_global_instance();

private:
  static Log* create_default_global_instance();
};

#if GROVE_LOGGING_ENABLED == 1
#define GROVE_LOG_ERROR_META(message, meta) \
  grove::Log::require_global_instance()->error((message), (meta))
#define GROVE_LOG_INFO_META(message, meta) \
  grove::Log::require_global_instance()->info((message), (meta))
#define GROVE_LOG_WARNING_META(message, meta) \
  grove::Log::require_global_instance()->warning((message), (meta))

#define GROVE_LOG_ERROR_CAPTURE_META(message, tag) \
  grove::Log::require_global_instance()->error((message), \
    grove::Log::MetaData((tag), __func__, __FILE__, __LINE__))
#define GROVE_LOG_INFO_CAPTURE_META(message, tag) \
  grove::Log::require_global_instance()->info((message), \
    grove::Log::MetaData((tag), __func__, __FILE__, __LINE__))
#define GROVE_LOG_WARNING_CAPTURE_META(message, tag) \
  grove::Log::require_global_instance()->warning((message), \
    grove::Log::MetaData((tag), __func__, __FILE__, __LINE__))

#define GROVE_LOG_ERROR(message) \
  grove::Log::require_global_instance()->error((message))
#define GROVE_LOG_INFO(message) \
  grove::Log::require_global_instance()->info((message))
#define GROVE_LOG_WARNING(message) \
  grove::Log::require_global_instance()->warning((message))
#else
#define GROVE_LOG_ERROR(message) \
  do {} while (0)
#define GROVE_LOG_INFO(message) \
  do {} while (0)
#define GROVE_LOG_WARNING(message) \
  do {} while (0)

#define GROVE_LOG_ERROR_META(message, meta) \
  do {} while (0)
#define GROVE_LOG_INFO_META(message, meta) \
  do {} while (0)
#define GROVE_LOG_WARNING_META(message, meta) \
  do {} while (0)

#define GROVE_LOG_ERROR_CAPTURE_META(message, meta) \
  do {} while (0)
#define GROVE_LOG_INFO_CAPTURE_META(message, meta) \
  do {} while (0)
#define GROVE_LOG_WARNING_CAPTURE_META(message, meta) \
  do {} while (0)

#endif

#define GROVE_LOG_SEVERE_CAPTURE_META(message, tag) \
  grove::Log::require_global_instance()->severe((message), \
    grove::Log::MetaData((tag), __func__, __FILE__, __LINE__))