#pragma once

#include "transform_allocator.hpp"
#include <unordered_map>

namespace grove::transform {

class TransformSystem {
public:
  TransformInstance* create(const TRS<float>& source);
  void destroy(TransformInstance* inst);
  void push_pending(TransformInstance* inst);
  void update();

private:
  TransformAllocator allocator;
  std::vector<TransformInstance*> pending_update;
  int num_pending{};
  std::vector<TransformInstance*> temporary;
  std::unordered_map<TransformInstance*, std::pair<TRS<float>, bool>> processed;
};

}