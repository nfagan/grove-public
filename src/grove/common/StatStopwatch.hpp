#pragma once

#include <chrono>
#include <vector>
#include <string>

namespace grove {
  class StatStopwatch;
}

class grove::StatStopwatch {
public:
  using duration_t = std::chrono::duration<double>;
  
  struct Stats {
    Stats();
    std::string to_string_in_ms() const;

    duration_t mean;
    duration_t max;
    duration_t min;
    double iters;
  };
  
public:
  StatStopwatch();
  explicit StatStopwatch(int num_history_samples);
  
  void tick();
  duration_t tock();
  
  const Stats& get_lifetime_stats() const {
    return stats;
  }
  
  Stats get_history_stats() const;
  void summarize_stats() const;
  void summarize_stats(const char* message) const;
  void summarize_stats(std::stringstream& into, const char* message = nullptr) const;
  
private:
  void update_history(const duration_t& elapsed);
  void update_lifetime_stats(const duration_t& elapsed);
  
private:
  std::chrono::high_resolution_clock::time_point t0;
  Stats stats;
  std::vector<duration_t> history;
  int history_sample_index;
};
