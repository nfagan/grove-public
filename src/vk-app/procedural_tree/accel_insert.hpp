#pragma once

#include "components.hpp"
#include "../bounds/bounds_system.hpp"
#include "grove/common/Future.hpp"

namespace grove::tree {

struct AccelInsertAndPruneResult {
  tree::Internodes src_internodes;
  tree::Internodes pruned_internodes;
  std::vector<bounds::ElementID> pruned_internode_element_ids;
  std::vector<int> pruned_to_src;
  std::vector<OBB3f> src_leaf_bounds;
  std::vector<bounds::ElementID> pruned_leaf_element_ids;
};

struct AccelInsertAndPruneParams {
  tree::Internodes internodes;
  std::vector<OBB3f> leaf_bounds;
  bounds::ElementTag tree_element_tag;
  bounds::ElementTag leaf_element_tag;
  bounds::ElementID parent_element_id;
  bounds::AccelInstanceHandle accel;
};

using FutureInsertAndPruneResult = std::shared_ptr<Future<AccelInsertAndPruneResult>>;

struct AccelInsertAndPrune {
public:
  enum class Type {
    InternodeInsertAndPrune,
    LeafInsert
  };

  struct Instance {
    Type type;
    bounds::ElementID parent_element_id;
    bounds::ElementTag tree_element_tag;
    bounds::ElementTag leaf_element_tag;
    tree::Internodes src_internodes;
    std::vector<OBB3f> src_leaf_bounds;
    FutureInsertAndPruneResult future_result;
  };

  struct Processing {
    bounds::AccelInstanceHandle accel_handle;
    std::vector<Instance> instances;
    std::future<void> task_future;
    std::atomic<bool> finished{};
  };

  struct UpdateInfo {
    bounds::BoundsSystem* bounds_system;
  };

public:
  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
  std::unordered_map<bounds::AccelInstanceHandle,
                     std::vector<Instance>,
                     bounds::AccelInstanceHandle::Hash> pending_accel_insert;
  std::vector<std::unique_ptr<Processing>> processing;
};

[[nodiscard]] FutureInsertAndPruneResult
push_internode_accel_insert_and_prune(AccelInsertAndPrune* sys, AccelInsertAndPruneParams&& params);

[[nodiscard]] FutureInsertAndPruneResult
push_leaf_accel_insert(AccelInsertAndPrune* sys, AccelInsertAndPruneParams&& params);

void update(AccelInsertAndPrune* sys, const AccelInsertAndPrune::UpdateInfo& info);

}