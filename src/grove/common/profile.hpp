#pragma once

#include "config.hpp"
#include "DynamicArray.hpp"
#include "RingBuffer.hpp"
#include "Optional.hpp"
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <functional>
#include <unordered_set>
#include <string_view>
#include <string>

namespace grove::profile {

struct ProfileParameters {
  int64_t num_samples{32};
};

struct Sample {
  double elapsed_ms{0.0};
};

struct Samples {
public:
  double last_elapsed_ms() const;
  double mean_elapsed_ms() const;
  double min_elapsed_ms() const;
  double max_elapsed_ms() const;
  int num_samples() const {
    return int(samples.size());
  }
  std::string stat_str() const;

public:
  DynamicArray<Sample, 64> samples;
};

struct SampleInfoQuery {
public:
  struct Hash {
    inline std::size_t operator()(const SampleInfoQuery& query) const noexcept {
      return std::hash<std::string_view>{}(query.profile_id);
    }
  };

  friend inline bool operator==(const SampleInfoQuery& a, const SampleInfoQuery& b) {
    return a.profile_id == b.profile_id;
  }
  friend inline bool operator!=(const SampleInfoQuery& a, const SampleInfoQuery& b) {
    return !(a == b);
  }

public:
  std::string_view profile_id{};
};

struct SampleQueryMatch {
public:
  std::string_view id{};
  Samples samples;
};

struct SampleInfoResponse {
  SampleInfoQuery query{};
  DynamicArray<SampleQueryMatch, 2> query_matches;
  bool success{false};
};

struct SampleInfoRequest {
public:
  void reset() {
    complete.store(false);
    queries.clear();
    responses.clear();
  }

public:
  DynamicArray<SampleInfoQuery, 2> queries;
  DynamicArray<SampleInfoResponse, 2> responses;
  std::atomic<bool> complete{false};
};

/*
 * Profiler
 */

class Profiler {
public:
  using Clock = std::chrono::high_resolution_clock;

  static constexpr std::chrono::milliseconds refresh_interval() {
    return std::chrono::milliseconds(20);
  }

public:
  bool read_samples(SampleInfoRequest* request);
  void update();

  bool tic(std::string_view id, const Clock::time_point& now);
  bool toc(std::string_view id, const Clock::time_point& now,
           const ProfileParameters& params = ProfileParameters{});

private:
  Optional<SampleInfoResponse> find_samples(std::string_view id);

private:
  std::unordered_map<std::string_view, Samples> samples;
  std::unordered_map<std::string_view, Clock::time_point> tics;
  RingBuffer<SampleInfoRequest*, 16> sample_info_requests;

  std::atomic<bool> in_use{false};
};

/*
 * globals
 */

void set_global_profiler(Profiler* profiler);
Profiler* get_global_profiler();

void start_profiling();
void stop_profiling();

bool tic(std::string_view id);
bool toc(std::string_view id, const ProfileParameters& params = ProfileParameters{});

/*
 * Listener
 */

class Listener {
public:
  Listener() : info_request{std::make_unique<SampleInfoRequest>()} {
    //
  }

  void update();
  void request(std::string_view profile_id);

  const SampleInfoResponse* find_response(std::string_view id) const;
  const SampleQueryMatch* find_first_query_match(std::string_view id) const;

private:
  std::unique_ptr<SampleInfoRequest> info_request;
  std::unordered_set<SampleInfoQuery, SampleInfoQuery::Hash> pending_queries;
  bool expecting_response{false};

public:
  DynamicArray<SampleInfoResponse, 8> responses;
};

/*
 * Runner
 */

class Runner {
public:
  Runner() {
    start_profiling();
  }
  ~Runner() {
    stop_profiling();
  }
};

/*
 * ScopedStopwatch
 */

class ScopeStopwatch {
public:
  template <typename... Args>
  ScopeStopwatch(std::string_view id, Args&&... args) :
    began_profiling{grove::profile::tic(id)},
    id{id},
    params{std::forward<Args>(args)...} {
    //
  }

  ~ScopeStopwatch() {
    if (began_profiling) {
      grove::profile::toc(id, params);
    }
  }

private:
  bool began_profiling;
  std::string_view id;
  grove::profile::ProfileParameters params;
};

}

#if GROVE_PROFILING_ENABLED == 1

#define GROVE_PROFILE_TIC(id) grove::profile::tic((id))
#define GROVE_PROFILE_TOC(id) grove::profile::toc((id))
#define GROVE_PROFILE_TOC_PARAMS(id, params) grove::profile::toc((id), (params))
#define GROVE_PROFILE_REQUEST(listener, profile_id) \
  (listener).request((profile_id))

#define GROVE_PROFILE_SCOPE_TIC_TOC(id) grove::profile::ScopeStopwatch{id}
#define GROVE_PROFILE_SCOPE_TIC_TOC_PARAMS(id, params) grove::profile::ScopeStopwatch{id, params}

#else

#define GROVE_PROFILE_TIC(id) 0
#define GROVE_PROFILE_TOC(id) 0
#define GROVE_PROFILE_TOC_PARAMS(id, params) 0
#define GROVE_PROFILE_REQUEST(listener, profile_id) do {} while (0)

#define GROVE_PROFILE_SCOPE_TIC_TOC(id) 0
#define GROVE_PROFILE_SCOPE_TIC_TOC_PARAMS(id, params) 0

#endif