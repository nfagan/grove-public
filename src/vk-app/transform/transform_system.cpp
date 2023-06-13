#include "transform_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"

GROVE_NAMESPACE_BEGIN

using namespace transform;

TransformInstance* transform::TransformSystem::create(const TRS<float>& source) {
  return allocator.create_instance(this, source);
}

void transform::TransformSystem::destroy(TransformInstance* inst) {
  if (inst->parent) {
    inst->parent->remove_child(inst);
  }
  for (auto* child : inst->children) {
    child->parent = nullptr;
  }
  allocator.destroy_instance(inst);
}

void transform::TransformSystem::push_pending(TransformInstance* inst) {
#ifdef GROVE_DEBUG
  auto pend_it = std::find(pending_update.begin(), pending_update.begin() + num_pending, inst);
  assert(pend_it == pending_update.begin() + num_pending);
#endif
  if (num_pending == int(pending_update.size())) {
    pending_update.push_back(nullptr);
  }
  pending_update[num_pending++] = inst;
}

void transform::TransformSystem::update() {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("TransformSystem/update");
  (void) profiler;
  processed.clear();
  for (int pend_ind = 0; pend_ind < num_pending; pend_ind++) {
    TransformInstance* next = pending_update[pend_ind];
    int temp_ind{};
    while (true) {
      auto proc_it = processed.find(next);
      if (proc_it != processed.end()) {
        break;
      } else {
        processed[next] = std::pair<TRS<float>, bool>{next->source, false};
      }
      if (temp_ind == int(temporary.size())) {
        temporary.push_back(nullptr);
      }
      temporary[temp_ind++] = next;
      if (next->parent) {
        next = next->parent;
      } else {
        break;
      }
    }
    const int temp_count = temp_ind;
    auto current = TRS<float>::identity();
    for (int i = 0; i < temp_count; i++) {
      int curr_ind = temp_count - i - 1;
      TransformInstance* curr = temporary[curr_ind];
      auto& proc = processed.at(curr);
      if (!proc.second) {
        if (curr->parent) {
          auto& proc_parent = processed.at(curr->parent);
          assert(proc_parent.second);
          current = proc_parent.first;
        }
        current = apply(current, curr->source);
        proc.second = true;
        proc.first = current;
      } else {
        current = proc.first;
      }
      curr->current = current;
      curr->clear_pushed_pending();
    }
  }
  num_pending = 0;
}

GROVE_NAMESPACE_END

