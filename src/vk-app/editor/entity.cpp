#include "entity.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * Entity
 */

Entity Entity::create() {
  return {detail::EntityIDStore::create()};
}

/*
 * EntityIDStore
 */

std::atomic<Entity::ID> detail::EntityIDStore::next_id{Entity::first_valid_id()};

Entity::ID detail::EntityIDStore::create() {
  return next_id++;
}

GROVE_NAMESPACE_END
