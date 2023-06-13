#pragma once

#include "SimpleShapeRenderer.hpp"

namespace grove {

class SimpleShapePools {
public:
  enum class ReleaseEnabled {
    No,
    Yes
  };

  struct PoolID {
    GROVE_INTEGER_IDENTIFIER_EQUALITY(PoolID, id)
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, PoolID, id)
    uint32_t id;
  };

  struct Handle {
    SimpleShapeRenderer::DrawableHandle drawable_handle;
    PoolID pool_id{};
    uint32_t instance_index{};
  };

  struct Pool {
    SimpleShapeRenderer::DrawableHandle handle;
    bool is_active{};
    int size{};
    std::vector<bool> in_use;
  };

  using Context = SimpleShapeRenderer::AddResourceContext;

public:
  SimpleShapePools() = default;
  SimpleShapePools(SimpleShapeRenderer::GeometryHandle geom,
                   int pool_size,
                   ReleaseEnabled enable_release,
                   SimpleShapeRenderer::PipelineType pipeline_type);
  bool is_valid() const;
  Optional<Handle> acquire(const Context& context, SimpleShapeRenderer& renderer);
  void release(SimpleShapeRenderer& renderer, Handle handle);
  void reset(SimpleShapeRenderer& renderer);

private:
  Optional<SimpleShapeRenderer::GeometryHandle> geometry;
  std::unordered_map<PoolID, Pool, PoolID::Hash> pools;
  std::vector<PoolID> free_pools;
  int pool_size{};
  ReleaseEnabled release_enabled{};
  SimpleShapeRenderer::PipelineType pipeline_type{};
  uint32_t next_pool_id{1};
};

}