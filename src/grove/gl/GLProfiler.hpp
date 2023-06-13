#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string_view>

#define GROVE_GL_PROFILING_ENABLED (1)

namespace grove {

struct GLProfileHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(GLProfileHandle, id)
  uint64_t id;
};

class GLProfiler {
public:
  struct Sample {
    uint64_t time{};
    bool is_new{};
  };

  struct TicRecord {
    int pool_handle_index{};
    void* sync{};
    Sample sample{};
  };

public:
  void initialize();
  void begin_frame();
  void end_frame();
  GLProfileHandle create();
  void destroy(GLProfileHandle handle);
  void tic(GLProfileHandle handle);
  void toc(GLProfileHandle handle);
  Optional<Sample> get(GLProfileHandle handle);
  void set_enabled(bool v);
  bool is_enabled() const {
    return enabled;
  }

  static void set_global_instance(GLProfiler* profiler);
  static void set_global_profiler_enabled(bool v);
  static bool get_global_profiler_enabled();
  static void tic(std::string_view id);
  static void toc(std::string_view id);
  static Optional<Sample> get(std::string_view id);
  static Optional<float> get_latest_ms(std::string_view id);

private:
  std::unordered_map<uint64_t, TicRecord> active;
  std::vector<unsigned int> query_pool;
  std::vector<int> free_list;
  uint64_t next_handle_id{1};
  int tic_depth{};
  bool enabled{};
  Optional<bool> set_enable;
};

namespace detail {

class GLProfileScopeHelper {
public:
  GLProfileScopeHelper(std::string_view id) : id{id} {
    GLProfiler::tic(this->id);
  }
  ~GLProfileScopeHelper() {
    GLProfiler::toc(id);
  }
private:
  std::string_view id;
};

#if GROVE_GL_PROFILING_ENABLED == 1
  #define GROVE_GL_PROFILE_SCOPE(id) grove::detail::GLProfileScopeHelper{id}
#else
  #define GROVE_GL_PROFILE_SCOPE(id) 0
#endif

}

}