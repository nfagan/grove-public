#include "accel_insert.hpp"
#include "bounds.hpp"
#include "utility.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using UpdateInfo = AccelInsertAndPrune::UpdateInfo;
using Instances = std::vector<AccelInsertAndPrune::Instance>;
using ElementIDSet = std::unordered_set<bounds::ElementID, bounds::ElementID::Hash>;
using ElementIDVec = std::vector<bounds::ElementID>;

std::vector<OBB3f> gather_internode_bounds(const tree::Internodes& src) {
  std::vector<OBB3f> result(src.size());
  for (size_t i = 0; i < src.size(); i++) {
    result[i] = tree::internode_obb(src[i]);
  }
  return result;
}

tree::InsertInternodeBoundsParams prepare_insert_params(bounds::Accel* accel,
                                                        bounds::ElementID tree_element_id,
                                                        bounds::ElementTag tree_element_tag,
                                                        bounds::ElementTag leaf_element_tag) {
  assert(leaf_element_tag != tree_element_tag &&
         leaf_element_tag.is_valid() &&
         tree_element_tag.is_valid() &&
         tree_element_id.is_valid());
  tree::InsertInternodeBoundsParams params{};
  params.accel = accel;
  params.tree_element_id = tree_element_id;
  params.tree_element_tag = tree_element_tag;
  params.leaf_element_tag = leaf_element_tag;
  return params;
}

void fill_insert_params(tree::InsertInternodeBoundsParams& params,
                        const std::vector<OBB3f>& bounds,
                        bool* inserted,
                        bounds::ElementID* dst_element_ids) {
  params.bounds = bounds.data();
  params.inserted = inserted;
  params.dst_element_ids = dst_element_ids;
  params.num_bounds = int(bounds.size());
}

void keep_only_inserted(bounds::Accel* accel, const ElementIDSet& kept,
                        const ElementIDVec& possibly_inserted) {
  size_t num_invalid{};
  size_t num_deactivated{};
  for (const bounds::ElementID id : possibly_inserted) {
    if (!id.is_valid()) {
      num_invalid++;
    } else if (kept.count(id) == 0) {
      const size_t num_match = accel->deactivate_if([id](const bounds::Element* el) {
        return el->id == id.id;
      });
      assert(num_match == 1);
      num_deactivated += num_match;
    }
  }
  assert(num_deactivated + num_invalid + kept.size() == possibly_inserted.size());
}

struct InsertInternodesResult {
  tree::Internodes pruned_internodes;
  std::vector<bounds::ElementID> pruned_internode_element_ids;
  std::vector<int> pruned_to_src;
};

InsertInternodesResult
insert_and_prune_internodes(tree::InsertInternodeBoundsParams& insert_params,
                            const tree::Internodes& src_internodes) {
  const int num_internodes = int(src_internodes.size());

  auto accept = std::make_unique<bool[]>(num_internodes);
  std::vector<bounds::ElementID> dst_internode_element_ids(num_internodes);
  std::vector<int> pruned_to_src(num_internodes);
  std::vector<OBB3f> src_internode_bounds = gather_internode_bounds(src_internodes);
  tree::Internodes dst_internodes(num_internodes);

  fill_insert_params(
    insert_params,
    src_internode_bounds,
    accept.get(),
    dst_internode_element_ids.data());
  const int num_accepted = insert_internode_bounds(insert_params);
  (void) num_accepted;

  const int num_kept = tree::prune_rejected_axes(
    src_internodes.data(),
    accept.get(),
    num_internodes,
    dst_internodes.data(),
    pruned_to_src.data());

  dst_internodes.resize(num_kept);
  pruned_to_src.resize(num_kept);
  std::vector<bounds::ElementID> pruned_element_ids(num_kept);

  for (int i = 0; i < num_kept; i++) {
    pruned_element_ids[i] = dst_internode_element_ids[pruned_to_src[i]];
    assert(pruned_element_ids[i].id > 0);
  }

  if (num_kept != num_internodes) {
    //  Keep only the bounds elements associated with `pruned_element_ids`.
    ElementIDSet kept{pruned_element_ids.begin(), pruned_element_ids.end()};
    keep_only_inserted(insert_params.accel, kept, dst_internode_element_ids);
  }

  InsertInternodesResult result;
  result.pruned_internodes = std::move(dst_internodes);
  result.pruned_internode_element_ids = std::move(pruned_element_ids);
  result.pruned_to_src = std::move(pruned_to_src);
  return result;
}

std::vector<bounds::ElementID>
insert_leaves(tree::InsertInternodeBoundsParams& params, const std::vector<OBB3f>& bounds) {
  auto inserted = std::make_unique<bool[]>(bounds.size());
  std::vector<bounds::ElementID> el_ids(bounds.size());

  fill_insert_params(params, bounds, inserted.get(), el_ids.data());
  const int num_inserted = insert_leaf_bounds(params);

  std::vector<bounds::ElementID> result(num_inserted);
  int ni{};
  for (size_t i = 0; i < bounds.size(); i++) {
    if (inserted[i]) {
      result[ni++] = el_ids[i];
    }
  }

  assert(ni == num_inserted);
  return result;
}

AccelInsertAndPruneResult internode_insert_and_prune(bounds::Accel* accel,
                                                     AccelInsertAndPrune::Instance&& instance) {
  auto insert_params = prepare_insert_params(
    accel,
    instance.parent_element_id,
    instance.tree_element_tag,
    instance.leaf_element_tag);

  auto src_internodes = std::move(instance.src_internodes);
  auto internode_res = insert_and_prune_internodes(insert_params, src_internodes);

  AccelInsertAndPruneResult result;
  result.src_internodes = std::move(src_internodes);
  result.pruned_internodes = std::move(internode_res.pruned_internodes);
  result.pruned_internode_element_ids = std::move(internode_res.pruned_internode_element_ids);
  result.pruned_to_src = std::move(internode_res.pruned_to_src);
  return result;
}

AccelInsertAndPruneResult leaf_insert(bounds::Accel* accel,
                                      AccelInsertAndPrune::Instance&& instance) {
  auto insert_params = prepare_insert_params(
    accel,
    instance.parent_element_id,
    instance.tree_element_tag,
    instance.leaf_element_tag);

  auto src_leaf_bounds = std::move(instance.src_leaf_bounds);
  auto leaf_res = insert_leaves(insert_params, src_leaf_bounds);

  AccelInsertAndPruneResult result;
  result.src_leaf_bounds = std::move(src_leaf_bounds);
  result.pruned_leaf_element_ids = std::move(leaf_res);
  return result;
}

void process_dispatch(bounds::Accel* accel, AccelInsertAndPrune::Instance* inst) {
  switch (inst->type) {
    case AccelInsertAndPrune::Type::InternodeInsertAndPrune: {
      inst->future_result->data = internode_insert_and_prune(accel, std::move(*inst));
      break;
    }
    case AccelInsertAndPrune::Type::LeafInsert: {
      inst->future_result->data = leaf_insert(accel, std::move(*inst));
      break;
    }
    default: {
      assert(false);
    }
  }
}

std::unique_ptr<AccelInsertAndPrune::Processing>
launch(Instances&& instances, bounds::Accel* accel, bounds::AccelInstanceHandle accel_handle) {
  auto process = std::make_unique<AccelInsertAndPrune::Processing>();
  process->accel_handle = accel_handle;
  process->instances = std::move(instances);

  auto* process_ptr = process.get();
  auto task = [accel, process_ptr]() {
    for (auto& inst : process_ptr->instances) {
      process_dispatch(accel, &inst);
    }
    process_ptr->finished.store(true);
  };

  process->task_future = std::async(std::launch::async, std::move(task));
  return process;
}

void launch_pending(AccelInsertAndPrune* sys, const UpdateInfo& info) {
  auto pend_insert_it = sys->pending_accel_insert.begin();
  while (pend_insert_it != sys->pending_accel_insert.end()) {
    const bounds::AccelInstanceHandle accel_handle = pend_insert_it->first;
    const bounds::AccessorID accessor_id = sys->bounds_accessor_id;
    auto* accel = bounds::request_write(info.bounds_system, accel_handle, accessor_id);
    if (accel) {
      sys->processing.emplace_back() = launch(
        std::move(pend_insert_it->second),
        accel,
        accel_handle);
      pend_insert_it = sys->pending_accel_insert.erase(pend_insert_it);
    } else {
      ++pend_insert_it;
    }
  }
}

void check_finished(AccelInsertAndPrune* sys, const UpdateInfo& info) {
  auto proc_it = sys->processing.begin();
  while (proc_it != sys->processing.end()) {
    auto& process = *proc_it;
    if (process->finished.load()) {
      process->task_future.get(); //  should not block.
      for (auto& inst : process->instances) {
        inst.future_result->mark_ready();
      }
      bounds::release_write(info.bounds_system, process->accel_handle, sys->bounds_accessor_id);
      proc_it = sys->processing.erase(proc_it);
    } else {
      ++proc_it;
    }
  }
}

AccelInsertAndPrune::Instance make_instance(AccelInsertAndPrune::Type type,
                                            AccelInsertAndPruneParams&& params,
                                            FutureInsertAndPruneResult fut_result) {
  AccelInsertAndPrune::Instance inst{};
  inst.type = type;
  inst.parent_element_id = params.parent_element_id;
  inst.tree_element_tag = params.tree_element_tag;
  inst.leaf_element_tag = params.leaf_element_tag;
  inst.src_internodes = std::move(params.internodes);
  inst.src_leaf_bounds = std::move(params.leaf_bounds);
  inst.future_result = std::move(fut_result);
  return inst;
}

std::vector<AccelInsertAndPrune::Instance>* require_pending(AccelInsertAndPrune* sys,
                                                            bounds::AccelInstanceHandle accel) {
  auto accel_it = sys->pending_accel_insert.find(accel);
  if (accel_it == sys->pending_accel_insert.end()) {
    sys->pending_accel_insert[accel] = {};
    return &sys->pending_accel_insert.at(accel);
  } else {
    return &accel_it->second;
  }
}

FutureInsertAndPruneResult push_pending(AccelInsertAndPrune* sys,
                                        AccelInsertAndPrune::Type type,
                                        AccelInsertAndPruneParams&& params) {
  auto fut_res = std::make_shared<Future<AccelInsertAndPruneResult>>();
  bounds::AccelInstanceHandle accel = params.accel;
  auto inst = make_instance(type, std::move(params), fut_res);
  require_pending(sys, accel)->push_back(std::move(inst));
  return fut_res;
}

} //  anon

FutureInsertAndPruneResult
tree::push_internode_accel_insert_and_prune(AccelInsertAndPrune* sys,
                                            AccelInsertAndPruneParams&& params) {
  assert(params.leaf_bounds.empty());
  return push_pending(
    sys, AccelInsertAndPrune::Type::InternodeInsertAndPrune, std::move(params));
}

FutureInsertAndPruneResult
tree::push_leaf_accel_insert(AccelInsertAndPrune* sys, AccelInsertAndPruneParams&& params) {
  assert(params.internodes.empty());
  return push_pending(
    sys, AccelInsertAndPrune::Type::LeafInsert, std::move(params));
}

void tree::update(AccelInsertAndPrune* sys, const AccelInsertAndPrune::UpdateInfo& info) {
  launch_pending(sys, info);
  check_finished(sys, info);
}

GROVE_NAMESPACE_END
