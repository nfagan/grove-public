#pragma once

#include "components.hpp"
#include "grove/common/Stopwatch.hpp"
#include <thread>
#include <atomic>

namespace grove::tree {

class GrowthSystem {
public:
  enum class State {
    Idle,
    Growing
  };

  struct GrowableTree {
    TreeNodeStore* nodes{};
    const SpawnInternodeParams* spawn_params{};
    const DistributeBudQParams* bud_q_params{};
    std::function<std::vector<Vec3f>()>* make_attraction_points{};
    int max_num_internodes{};
    int last_num_internodes{};
  };

  struct Context {
    std::vector<GrowableTree> trees;
    EnvironmentInputs environment_input;
    AttractionPoints* attraction_points{};
    SenseContext sense_context;
    Stopwatch stopwatch;
    double growth_time{};
  };

  struct Fence {
    bool is_ready() const {
      return ready.load();
    }
    void signal() {
      ready.store(true);
    }
    void reset() {
      ready.store(false);
    }

    std::atomic<bool> ready{true};
  };

  struct UpdateResult {
    bool finished_growing{};
    double growth_time{};
  };

public:
  ~GrowthSystem();
  UpdateResult update();
  void initialize();
  void terminate();

  void fill_context(AttractionPoints* attraction_points,
                    std::vector<GrowableTree>&& growable_trees);

  bool is_idle() const;
  void submit();

  bool worker_keep_processing() const;
  bool worker_start_growing();

private:
  std::thread work_thread;
  std::atomic<bool> keep_processing{false};
  std::atomic<bool> start_growing{false};

  State state{State::Idle};

public:
  Fence fence;
  Context context;
};

}