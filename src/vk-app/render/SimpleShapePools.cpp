#include "SimpleShapePools.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using Pool = SimpleShapePools::Pool;

uint32_t find_next_instance_index(const Pool& pool) {
  auto in_use_it = std::find_if(pool.in_use.begin(), pool.in_use.end(), [](bool in_use) {
    return !in_use;
  });
  GROVE_ASSERT(in_use_it != pool.in_use.end());
  return uint32_t(in_use_it - pool.in_use.begin());
}

} //  anon

SimpleShapePools::SimpleShapePools(SimpleShapeRenderer::GeometryHandle geom,
                                   int pool_size,
                                   ReleaseEnabled enable_release,
                                   SimpleShapeRenderer::PipelineType pipeline_type) :
                                   geometry{geom},
                                   pool_size{pool_size},
                                   release_enabled{enable_release},
                                   pipeline_type{pipeline_type} {
  //
}

bool SimpleShapePools::is_valid() const {
  return geometry.has_value();
}

Optional<SimpleShapePools::Handle>
SimpleShapePools::acquire(const Context& context, SimpleShapeRenderer& renderer) {
  if (!geometry) {
    return NullOpt{};
  }

  PoolID id;
  int free_pool_ind;
  if (free_pools.empty()) {
    auto inst_res = renderer.add_instances(context, geometry.value(), pool_size, pipeline_type);
    if (!inst_res) {
      return NullOpt{};
    }
    Pool pool{};
    pool.handle = inst_res.value();
    pool.in_use.resize(pool_size);
    id = PoolID{next_pool_id++};
    pools[id] = std::move(pool);
    free_pool_ind = int(free_pools.size());
    free_pools.push_back(id);
  } else {
    id = free_pools.back();
    free_pool_ind = int(free_pools.size() - 1);
  }

  auto& pool = pools.at(id);
  const auto instance_ind = release_enabled == ReleaseEnabled::Yes ?
    find_next_instance_index(pool) :
    pool.size;
  pool.in_use[instance_ind] = true;
  GROVE_ASSERT(pool.size < pool_size);
  pool.size++;
  if (pool.size == pool_size) {
    free_pools.erase(free_pools.begin() + free_pool_ind);
  }
  if (!pool.is_active) {
    renderer.add_active_drawable(pool.handle);
    pool.is_active = true;
  }

  Handle result{};
  result.pool_id = id;
  result.instance_index = instance_ind;
  result.drawable_handle = pool.handle;
  return Optional<Handle>(result);
}

void SimpleShapePools::release(SimpleShapeRenderer& renderer, Handle handle) {
  GROVE_ASSERT(release_enabled == ReleaseEnabled::Yes &&
               pools.count(handle.pool_id) > 0 &&
               handle.instance_index < uint32_t(pool_size));
  auto& pool = pools.at(handle.pool_id);
  GROVE_ASSERT(pool.in_use[handle.instance_index] && pool.size > 0);
  pool.size--;
  pool.in_use[handle.instance_index] = false;
  renderer.set_active_instance(handle.drawable_handle, handle.instance_index, false);

  const auto free_it = std::find(free_pools.begin(), free_pools.end(), handle.pool_id);
  if (pool.size == 0) {
    renderer.destroy_instances(pool.handle);
    pools.erase(handle.pool_id);
    if (free_it != free_pools.end()) {
      free_pools.erase(free_it);
    }
  } else {
    if (free_it == free_pools.end()) {
      free_pools.push_back(handle.pool_id);
    }
  }
}

void SimpleShapePools::reset(SimpleShapeRenderer& renderer) {
  free_pools.clear();
  for (auto& [id, pool] : pools) {
    if (pool.is_active) {
      renderer.clear_active_instances(pool.handle);
      renderer.remove_active_drawable(pool.handle);
      pool.is_active = false;
    }
    pool.size = 0;
    std::fill(pool.in_use.begin(), pool.in_use.end(), false);
    free_pools.push_back(id);
  }
}

GROVE_NAMESPACE_END
