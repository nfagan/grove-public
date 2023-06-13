#include "bounds_system.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace bounds;

BoundsSystem::Instance* find_instance(BoundsSystem* sys, AccelInstanceHandle handle) {
  for (auto& inst : sys->instances) {
    if (inst->id == handle.id) {
      return inst.get();
    }
  }
  return nullptr;
}

void deactivate(Accel* accel, const std::vector<bounds::ElementID>& ids) {
  std::unordered_set<bounds::ElementID, bounds::ElementID::Hash> set{ids.begin(), ids.end()};
  auto f = [&set](const bounds::Element* el) -> bool {
    return set.count(bounds::ElementID{el->id}) > 0;
  };
  accel->deactivate_if(f);
}

size_t deactivate(Accel* accel, bounds::ElementID id) {
  return accel->deactivate_if([id](const bounds::Element* el) {
    return el->id == id.id;
  });
}

const Accel* request_read(BoundsSystem::Instance* inst, AccessorID id) {
  if (!inst->current_writer) {
    auto it = std::find(inst->current_readers.begin(), inst->current_readers.end(), id);
    if (it == inst->current_readers.end()) {
      inst->current_readers.push_back(id);
    } else {
      assert(false && "Call `release_read` before requesting read access again.");
    }
    return &inst->accel;
  } else {
    return nullptr;
  }
}

void release_read(BoundsSystem::Instance* inst, AccessorID id) {
  auto it = std::find(inst->current_readers.begin(), inst->current_readers.end(), id);
  if (it != inst->current_readers.end()) {
    inst->current_readers.erase(it);
  } else {
    assert(false && "Tried to release read access, but it was not yet acquired.");
  }
}

Accel* request_write(BoundsSystem::Instance* inst, AccessorID id) {
  const bool read_crit = inst->current_readers.empty();
  const bool write_crit = !inst->current_writer;
  if (!read_crit || !write_crit) {
    return nullptr;
  } else {
    inst->current_writer = id;
    return &inst->accel;
  }
}

void release_write(BoundsSystem::Instance* inst, AccessorID id) {
  assert(inst->current_writer && inst->current_writer.value() == id);
  inst->current_writer = NullOpt{};
  (void) id;
}

bool can_launch(const BoundsSystem::Instance& inst) {
  return !inst.rebuilding_accel && !inst.deactivating;
}

template <typename F>
void async_launch(BoundsSystem::Instance* inst, F&& task) {
  inst->async_complete.store(false);
  inst->async_future = std::async(std::launch::async, std::forward<F>(task));
}

void on_async_write_complete(BoundsSystem::Instance* inst) {
  inst->async_future.get();
  release_write(inst, inst->self_id);
}

bool async_complete(BoundsSystem::Instance* inst) {
  return inst->async_complete.load();
}

void update_pending_deactivation(BoundsSystem::Instance* inst) {
  if (can_launch(*inst) && !inst->pending_deactivation.empty()) {
    if (auto* accel = request_write(inst, inst->self_id)) {
      auto task = [accel, inst, ids = std::move(inst->pending_deactivation)]() {
        deactivate(accel, ids);
        inst->async_complete.store(true);
      };
      async_launch(inst, std::move(task));
      inst->pending_deactivation.clear();
      inst->deactivating = true;
    }
  }
  if (inst->deactivating && async_complete(inst)) {
    on_async_write_complete(inst);
    inst->deactivating = false;
    inst->need_check_auto_rebuild = true;
  }
}

void update_rebuild(BoundsSystem::Instance* inst) {
  if (can_launch(*inst) && inst->need_rebuild_accel) {
    if (auto* accel = request_write(inst, inst->self_id)) {
      auto task = [accel, inst, params = inst->rebuild_params]() {
        *accel = Accel::rebuild_active(
          std::move(*accel),
          params.initial_span_size,
          params.max_span_size_split);
        inst->async_complete.store(true);
      };
      async_launch(inst, std::move(task));
      inst->need_rebuild_accel = false;
      inst->rebuilding_accel = true;
    }
  }
  if (inst->rebuilding_accel && async_complete(inst)) {
    on_async_write_complete(inst);
    inst->rebuilding_accel = false;
  }
}

void update_trigger_auto_rebuild(BoundsSystem::Instance* inst) {
  if (!inst->need_check_auto_rebuild) {
    return;
  }
  if (auto* accel = request_read(inst, inst->self_id)) {
    auto num_els = double(accel->num_elements());
    if (num_els > 0.0) {
      auto num_inactive = double(accel->num_inactive());
      if (float(num_inactive / num_els) >= inst->auto_rebuild_proportion_threshold) {
        inst->need_rebuild_accel = true;
      }
    }
    release_read(inst, inst->self_id);
    inst->need_check_auto_rebuild = false;
  }
}

std::unique_ptr<BoundsSystem::Instance>
make_instance(uint32_t id, const CreateAccelInstanceParams& params) {
  assert(params.initial_span_size > 0.0f);
  assert(params.max_span_size_split > 0.0f);
  auto res = std::make_unique<BoundsSystem::Instance>();
  res->id = id;
  res->self_id = AccessorID::create();
  res->accel = bounds::Accel{
    params.initial_span_size,
    params.max_span_size_split
  };
  res->rebuild_params = params;
  return res;
}

} //  anon

const Accel* bounds::request_read(BoundsSystem* sys, AccelInstanceHandle handle, AccessorID id) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  return grove::request_read(inst, id);
}

void bounds::release_read(BoundsSystem* sys, AccelInstanceHandle handle, AccessorID id) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  grove::release_read(inst, id);
}

Accel* bounds::request_write(BoundsSystem* sys, AccelInstanceHandle handle, AccessorID id) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  return grove::request_write(inst, id);
}

void bounds::release_write(BoundsSystem* sys, AccelInstanceHandle handle, AccessorID id) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  grove::release_write(inst, id);
}

Accel* bounds::request_transient_write(BoundsSystem* sys, AccelInstanceHandle instance) {
  return request_write(sys, instance, sys->self_accessor_id);
}

void bounds::release_transient_write(BoundsSystem* sys, AccelInstanceHandle instance) {
  release_write(sys, instance, sys->self_accessor_id);
}

AccelInstanceHandle bounds::create_instance(BoundsSystem* sys,
                                            const CreateAccelInstanceParams& params) {
  uint32_t id = sys->next_instance_id++;
  sys->instances.emplace_back() = make_instance(id, params);
  AccelInstanceHandle handle{id};
  return handle;
}

void bounds::push_pending_deactivation(BoundsSystem* sys, AccelInstanceHandle handle,
                                       const ElementID* ids, uint32_t num_ids) {
  if (auto* inst = find_instance(sys, handle)) {
    if (num_ids > 0) {
      auto& pend = inst->pending_deactivation;
      auto curr_size = pend.size();
      pend.resize(curr_size + num_ids);
      memcpy(pend.data() + curr_size, ids, num_ids * sizeof(ElementID));
    }
  } else {
    assert(false);
  }
}

void bounds::push_pending_deactivation(BoundsSystem* sys, AccelInstanceHandle handle,
                                       std::vector<ElementID>&& ids) {
  push_pending_deactivation(sys, handle, ids.data(), uint32_t(ids.size()));
  ids.clear();
}

size_t bounds::deactivate_element(bounds::Accel* accel, bounds::ElementID id) {
  return deactivate(accel, id);
}

void bounds::rebuild_accel(BoundsSystem* sys, AccelInstanceHandle handle,
                           const CreateAccelInstanceParams& params) {
  auto* inst = find_instance(sys, handle);
  inst->rebuild_params = params;
  inst->need_rebuild_accel = true;
}

void bounds::update(BoundsSystem* sys) {
  for (auto& inst : sys->instances) {
    update_rebuild(inst.get());
    update_pending_deactivation(inst.get());
    update_trigger_auto_rebuild(inst.get());
  }
}

GROVE_NAMESPACE_END
