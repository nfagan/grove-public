#include "GLProfiler.hpp"
#include "grove/common/common.hpp"
#include <glad/glad.h>
#include <numeric>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr int pool_size = 8;

GLProfiler* global_instance{};
std::unordered_map<std::string_view, GLProfileHandle> to_profile_handle;

} //  anon

void GLProfiler::set_global_instance(GLProfiler* profiler) {
  global_instance = profiler;
}

void GLProfiler::set_global_profiler_enabled(bool v) {
  if (global_instance) {
    global_instance->set_enabled(v);
  }
}

bool GLProfiler::get_global_profiler_enabled() {
  if (global_instance) {
    return global_instance->is_enabled();
  } else {
    return false;
  }
}

void GLProfiler::tic(std::string_view id) {
  if (global_instance && global_instance->is_enabled()) {
    GLProfileHandle handle{};
    if (auto it = to_profile_handle.find(id); it != to_profile_handle.end()) {
      handle = it->second;
    } else {
      handle = global_instance->create();
      to_profile_handle[id] = handle;
    }
    global_instance->tic(handle);
  }
}

void GLProfiler::toc(std::string_view id) {
  if (global_instance && global_instance->is_enabled()) {
    if (auto it = to_profile_handle.find(id); it != to_profile_handle.end()) {
      global_instance->toc(it->second);
    } else {
      assert(false);
    }
  }
}

Optional<GLProfiler::Sample> GLProfiler::get(std::string_view id) {
  if (global_instance && global_instance->is_enabled()) {
    if (auto it = to_profile_handle.find(id); it != to_profile_handle.end()) {
      return global_instance->get(it->second);
    }
  }
  return NullOpt{};
}

Optional<float> GLProfiler::get_latest_ms(std::string_view id) {
  if (auto res = GLProfiler::get(id)) {
    return Optional<float>(float(double(res.value().time) / 1e6));
  } else {
    return NullOpt{};
  }
}

void GLProfiler::initialize() {
#if GROVE_GL_PROFILING_ENABLED
  query_pool.resize(pool_size);
  free_list.resize(pool_size);
  std::iota(free_list.begin(), free_list.end(), 0);
  glGenQueries(pool_size, query_pool.data());
#else
  (void) pool_size;
  (void) tic_depth;
#endif
}

void GLProfiler::begin_frame() {
  if (set_enable) {
    enabled = set_enable.value();
    set_enable = NullOpt{};
  }

  for (auto& [id, record] : active) {
    record.sample.is_new = false;
    if (record.sync) {
      auto sync = (GLsync) record.sync;
      int status;
      glGetSynciv(sync, GL_SYNC_STATUS, 1, nullptr, &status);
      if (status == GL_SIGNALED) {
        glDeleteSync(sync);
        record.sync = nullptr;
        //
        uint64_t elapsed_time{};
        glGetQueryObjectui64v(query_pool[record.pool_handle_index], GL_QUERY_RESULT, &elapsed_time);
        record.sample.time = elapsed_time;
        record.sample.is_new = true;
      } else {
        assert(status == GL_UNSIGNALED);
      }
    }
  }
}

void GLProfiler::end_frame() {
  //
}

GLProfileHandle GLProfiler::create() {
#if GROVE_GL_PROFILING_ENABLED
  if (free_list.empty()) {
    auto curr_size = int(query_pool.size());
    auto new_size = curr_size + pool_size;
    query_pool.resize(new_size);
    glGenQueries(pool_size, query_pool.data() + curr_size);
    for (int i = 0; i < pool_size; i++) {
      free_list.push_back(curr_size + i);
    }
  }

  GLProfileHandle handle{next_handle_id++};
  auto index = free_list.back();
  free_list.pop_back();
  TicRecord record{};
  record.pool_handle_index = index;
  active[handle.id] = record;
  return handle;
#else
  return GLProfileHandle{next_handle_id++};
#endif
}

void GLProfiler::destroy(GLProfileHandle handle) {
#if GROVE_GL_PROFILING_ENABLED
  assert(tic_depth == 0);
  if (auto it = active.find(handle.id); it != active.end()) {
    auto& record = it->second;
    if (record.sync) {
      glDeleteSync((GLsync) record.sync);
    }
    free_list.push_back(record.pool_handle_index);
    active.erase(it);
  } else {
    assert(false);
  }
#else
  (void) handle;
#endif
}

void GLProfiler::tic(GLProfileHandle handle) {
#if GROVE_GL_PROFILING_ENABLED
  assert(tic_depth == 0);
  if (auto it = active.find(handle.id); it != active.end()) {
    auto& record = it->second;
    if (!record.sync) {
      glBeginQuery(GL_TIME_ELAPSED, query_pool[record.pool_handle_index]);
    }
  } else {
    assert(false);
  }
  tic_depth++;
#else
  (void) handle;
#endif
}

void GLProfiler::toc(GLProfileHandle handle) {
#if GROVE_GL_PROFILING_ENABLED
  assert(tic_depth == 1);
  if (auto it = active.find(handle.id); it != active.end()) {
    auto& record = it->second;
    if (!record.sync) {
      glEndQuery(GL_TIME_ELAPSED);
      record.sync = (void*) glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
  } else {
    assert(false);
  }
  tic_depth--;
#else
  (void) handle;
#endif
}

Optional<GLProfiler::Sample> GLProfiler::get(GLProfileHandle handle) {
  if (auto it = active.find(handle.id); it != active.end()) {
    return Optional<GLProfiler::Sample>(it->second.sample);
  } else {
    return NullOpt{};
  }
}

void GLProfiler::set_enabled(bool v) {
  set_enable = v;
}

GROVE_NAMESPACE_END
