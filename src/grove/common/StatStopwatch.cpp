#include "StatStopwatch.hpp"
#include "grove/common/common.hpp"
#include "grove/math/constants.hpp"
#include "logging.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

GROVE_NAMESPACE_BEGIN

/*
 * Stats
 */

StatStopwatch::Stats::Stats() :
  mean(grove::nan()),
  max(grove::nan()),
  min(grove::nan()),
  iters(0.0) {
  //
}

std::string StatStopwatch::Stats::to_string_in_ms() const {
  std::stringstream stream;

  double mean_ms = std::round(mean.count() * 1e5) / 1e2;
  double min_ms = std::round(min.count() * 1e5) / 1e2;
  double max_ms = std::round(max.count() * 1e5) / 1e2;

  stream << "mean: " << std::setprecision(3) << mean_ms
         << "ms max: " << max_ms << "ms min: " << min_ms << "ms";

  return stream.str();
}

/*
 * StatStopwatch
 */

StatStopwatch::StatStopwatch() : StatStopwatch(0) {
  //
}

StatStopwatch::StatStopwatch(int num_history_samples) :
  t0(std::chrono::high_resolution_clock::now()),
  history(num_history_samples),
  history_sample_index(0) {
  //
}

void StatStopwatch::tick() {
  t0 = std::chrono::high_resolution_clock::now();
}

StatStopwatch::duration_t StatStopwatch::tock() {
  duration_t elapsed = std::chrono::high_resolution_clock::now() - t0;
  update_history(elapsed);
  update_lifetime_stats(elapsed);
  
  return elapsed;
}

StatStopwatch::Stats StatStopwatch::get_history_stats() const {
  Stats result;
  
  for (int i = 0; i < history_sample_index; i++) {
    if (i == 0) {
      result.mean = history[i];
      result.max = history[i];
      result.min = history[i];
    } else {
      result.mean = (result.mean * result.iters + history[i]) / (result.iters + 1.0);
      
      if (history[i] < result.min) {
        result.min = history[i];
      }
      
      if (history[i] > result.max) {
        result.max = history[i];
      }
    }
    
    result.iters = double(i + 1);
  }
  
  return result;
}

void StatStopwatch::update_history(const duration_t& elapsed) {
  const int max_num_samples = int(history.size());
  
  if (history_sample_index < max_num_samples) {
    history[history_sample_index++] = elapsed;
  } else {
    for (int i = history_sample_index-1; i > 0; i--) {
      history[i-1] = history[i];
    }
    
    if (history_sample_index > 0) {
      history[history_sample_index-1] = elapsed;
    }
  }
}

void StatStopwatch::update_lifetime_stats(const duration_t& elapsed) {
  if (stats.iters == 0.0) {
    stats.mean = elapsed;
    stats.max = elapsed;
    stats.min = elapsed;
  } else {
    stats.mean = (stats.mean * stats.iters + elapsed) / (stats.iters + 1.0);
    
    if (elapsed > stats.max) {
      stats.max = elapsed;
    }
    
    if (elapsed < stats.min) {
      stats.min = elapsed;
    }
  }
  
  stats.iters += 1.0;
}

void StatStopwatch::summarize_stats() const {
  summarize_stats(nullptr);
}

void StatStopwatch::summarize_stats(const char* message) const {
  std::stringstream stream;
  summarize_stats(stream, message);
  std::cout << stream.str() << std::endl;
}

void StatStopwatch::summarize_stats(std::stringstream& stream, const char* message) const {
  auto lifetime_stats = get_lifetime_stats();
  auto history_stats = get_history_stats();

  if (message != nullptr) {
    stream << message;
  }

  double mean_ms = std::round(lifetime_stats.mean.count() * 1e5) / 1e2;
  double min_ms = std::round(history_stats.min.count() * 1e5) / 1e2;
  double max_ms = std::round(history_stats.max.count() * 1e5) / 1e2;
  double latest_mean_ms = std::round(history_stats.mean.count() * 1e5) / 1e2;

  stream << "mean: " << std::setprecision(3)
         << mean_ms << "ms mean(" << history.size() <<  "): "
         << latest_mean_ms << "ms max: " << max_ms << "ms min: " << min_ms << "ms";
}

GROVE_NAMESPACE_END

