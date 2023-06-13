#pragma once

#include "grove/common/identifier.hpp"
#include <cstdint>
#include <atomic>
#include <functional>

namespace grove {

struct Entity {
public:
  using ID = uint64_t;

  static constexpr ID null_id() {
    return 0;
  }
  static constexpr ID first_valid_id() {
    return 1;
  }

  struct Hash {
    inline std::size_t operator()(const Entity& ent) const noexcept {
      return std::hash<decltype(ent.id)>{}(ent.id);
    }
  };

public:
  static Entity create();

  GROVE_INTEGER_IDENTIFIER_EQUALITY(Entity, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(Entity, id)

public:
  ID id{null_id()};
};

namespace detail {

class EntityIDStore {
private:
  friend struct grove::Entity;
  static Entity::ID create();

private:
  static std::atomic<Entity::ID> next_id;
};

}

}