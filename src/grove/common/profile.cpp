#include "profile.hpp"
#include <mutex>
#include <thread>

namespace grove::profile {

struct TryLock {
  TryLock(std::atomic<bool>& in_use) : in_use(in_use) {
    acquired = in_use.compare_exchange_strong(acquired, true);
  }

  ~TryLock() {
    if (acquired) {
      in_use.store(false);
    }
  }

  std::atomic<bool>& in_use;
  bool acquired{false};
};

/*
 * Samples
 */

double Samples::min_elapsed_ms() const {
  if (samples.empty()) {
    return 0.0;
  } else {
    return std::min_element(samples.begin(), samples.end(), [](auto& a, auto& b) {
      return a.elapsed_ms < b.elapsed_ms;
    })->elapsed_ms;
  }
}

double Samples::max_elapsed_ms() const {
  if (samples.empty()) {
    return 0.0;
  } else {
    return std::max_element(samples.begin(), samples.end(), [](auto& a, auto& b) {
      return a.elapsed_ms < b.elapsed_ms;
    })->elapsed_ms;
  }
}

double Samples::mean_elapsed_ms() const {
  if (samples.empty()) {
    return 0.0;
  }

  double mean = 0.0;
  double iters = 0.0;

  for (auto& sample : samples) {
    mean += sample.elapsed_ms;
    iters += 1.0;
  }

  return mean / iters;
}

double Samples::last_elapsed_ms() const {
  return samples.empty() ? 0.0 : samples.back().elapsed_ms;
}

std::string Samples::stat_str() const {
  constexpr int data_size = 1024;
  char data[data_size];
  auto num_written = std::snprintf(
    data, data_size, "mean: %0.2fms, min: %0.2fms, max: %0.2fms, last: %0.2fms",
    mean_elapsed_ms(), min_elapsed_ms(), max_elapsed_ms(), last_elapsed_ms());

  if (num_written < data_size && num_written > 0) {
    return std::string{data};
  } else {
    return {};
  }
}

/*
 * Profiler
 */

Optional<SampleInfoResponse> Profiler::find_samples(std::string_view id) {
  Samples out_samples;
  bool found_samples = false;

  auto maybe_samples = samples.find(id);
  if (maybe_samples != samples.end()) {
    out_samples = maybe_samples->second;
    found_samples = true;
  }

  if (found_samples) {
    SampleQueryMatch match{};

    match.id = id;
    match.samples = std::move(out_samples);

    SampleInfoResponse response{};
    response.query_matches.push_back(std::move(match));
    response.success = true;

    return Optional<SampleInfoResponse>(std::move(response));
  } else {
    return NullOpt{};
  }
}

bool Profiler::tic(std::string_view id, const Clock::time_point& now) {
  TryLock lock{in_use};
  if (!lock.acquired) {
    return false;
  }

  tics[id] = now;
  return true;
}

bool Profiler::toc(std::string_view id, const Clock::time_point& now,
                   const ProfileParameters& params) {
  TryLock lock{in_use};
  if (!lock.acquired) {
    return false;
  }

  auto maybe_tic = tics.find(id);

  if (maybe_tic == tics.end() ||
      maybe_tic->second == Clock::time_point{} ||
      params.num_samples <= 0) {
    //  tic() should precede call to toc()
    return false;
  }

  auto maybe_samples = samples.find(id);
  if (maybe_samples == samples.end()) {
    samples[id] = {};
    maybe_samples = samples.find(id);
  }

  auto elapsed_ms = std::chrono::duration<double>(now - maybe_tic->second).count() * 1e3;
  Sample sample{elapsed_ms};

  auto& curr_samples = maybe_samples->second.samples;
  if (curr_samples.size() < params.num_samples) {
    curr_samples.push_back(sample);

  } else if (curr_samples.size() > params.num_samples) {
    curr_samples.clear();
    curr_samples.push_back(sample);

  } else {
    for (int i = 1; i < curr_samples.size(); i++) {
      curr_samples[i-1] = curr_samples[i];
    }

    curr_samples.back() = sample;
  }

  tics[id] = Clock::time_point{};
  return true;
}

bool Profiler::read_samples(SampleInfoRequest* request) {
  return sample_info_requests.maybe_write(request);
}

void Profiler::update() {
  if (sample_info_requests.size() == 0) {
    return;
  }

  TryLock lock{in_use};
  if (!lock.acquired) {
    return;
  }

  //  Process only one request per update.
  auto req = sample_info_requests.read();

  for (auto& query : req->queries) {
    SampleInfoResponse query_response{};

    auto maybe_response = find_samples(query.profile_id);
    if (maybe_response) {
      query_response = std::move(maybe_response.value());
    }

    query_response.query = query;
    req->responses.push_back(std::move(query_response));
  }

  req->complete.store(true);
}

/*
 * global utils
 */

namespace globals {
  std::atomic<Profiler*> profiler{nullptr};
  std::thread profile_thread;
  std::atomic<bool> keep_profiling{false};
}

void set_global_profiler(Profiler* profiler) {
  globals::profiler.store(profiler);
}

Profiler* get_global_profiler() {
  return globals::profiler.load();
}

void start_profiling() {
  if (globals::keep_profiling.load()) {
    return;
  } else {
    globals::keep_profiling.store(true);
  }

  globals::profile_thread = std::thread([&]() {
    while (globals::keep_profiling.load()) {
      if (auto* profiler = globals::profiler.load()) {
        profiler->update();
      }

      std::this_thread::sleep_for(Profiler::refresh_interval());
    }
  });
}

void stop_profiling() {
  if (!globals::keep_profiling.load()) {
    return;
  } else {
    globals::keep_profiling.store(false);
  }

  if (globals::profile_thread.joinable()) {
    globals::profile_thread.join();
  }
}

const SampleInfoResponse* Listener::find_response(std::string_view id) const {
  for (auto& response : responses) {
    if (response.query.profile_id == id) {
      return &response;
    }
  }
  return nullptr;
}

const SampleQueryMatch* Listener::find_first_query_match(std::string_view id) const {
  auto maybe_response = find_response(id);
  if (!maybe_response || !maybe_response->success || maybe_response->query_matches.empty()) {
    return nullptr;
  }
  return &maybe_response->query_matches[0];
}

void Listener::request(std::string_view profile_id) {
  pending_queries.insert({profile_id});
}

void Listener::update() {
  auto* profiler = get_global_profiler();
  if (!profiler) {
    return;
  }

  if (!expecting_response) {
    info_request->queries.clear();
    for (auto& query : pending_queries) {
      info_request->queries.push_back(query);
    }

    if (profiler->read_samples(info_request.get())) {
      expecting_response = true;
    }

  } else if (info_request->complete.load()) {
    expecting_response = false;
    responses.clear();

    for (auto& response : info_request->responses) {
      if (response.success) {
        responses.push_back(response);
      }
    }

    info_request->reset();
  }
}

bool tic(std::string_view id) {
  auto now = Profiler::Clock::now();
  if (auto* profiler = get_global_profiler()) {
    return profiler->tic(id, now);
  } else {
    return false;
  }
}

bool toc(std::string_view id, const ProfileParameters& params) {
  auto now = Profiler::Clock::now();
  if (auto* profiler = get_global_profiler()) {
    return profiler->toc(id, now, params);
  } else {
    return false;
  }
}

}
