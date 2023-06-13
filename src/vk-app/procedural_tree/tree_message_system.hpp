#pragma once

#include "tree_system.hpp"
#include <atomic>

namespace grove::msg {

enum class MessageDomain {
  None,
  Tree
};

struct MessageID {
  static std::atomic<uint64_t> next_id;
  static MessageID next();
  GROVE_INTEGER_IDENTIFIER_EQUALITY(MessageID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(MessageID, id)
  uint64_t id;
};

struct MessageData {
  float read_float() const;
  Vec3f read_vec3f() const;
  void write_vec3f(const Vec3f& data);

  static constexpr int num_bytes = 16;
  unsigned char bytes[num_bytes];
};

struct Message {
  MessageID id;
  MessageDomain domain;
  float size;
  float speed;
  MessageData data;
};

} //  msg

namespace grove::tree {

struct TreeMessageSystemUpdateInfo {
  bounds::BoundsSystem* bounds_sys;
  bounds::AccelInstanceHandle accel_handle;
  const tree::TreeSystem* tree_sys;
  const tree::TreeSystem::DeletedInstances* just_deleted;
  double dt;
};

struct TreeMessageSystem {
  enum class MessageState {
    Idle,
    TravelingAlongBranch,
    MovingBetweenBranches
  };

  struct Events {
    bool just_reached_new_leaf;
  };

  struct Message {
    msg::Message message;
    MessageState message_state;
    tree::TreeInstanceHandle tree;
    tree::TreeInternodeID internode_id;
    Vec3f src_position;
    Vec3f position;
    float frac_next_internode;
    Optional<float> target_distance_to_root;
    float traveled_distance;
    Events events;
  };

  std::vector<Message> messages;
  std::vector<const bounds::Element*> intersect_query_storage;
  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
};

msg::Message make_zero_message(float size, float speed);
TreeMessageSystem::Message make_tree_message(msg::Message message, TreeInstanceHandle tree,
                                             TreeInternodeID src_internode, const Vec3f& pos);

void update(TreeMessageSystem* sys, const TreeMessageSystemUpdateInfo& info);
void push_message(TreeMessageSystem* sys, const TreeMessageSystem::Message& message);
ArrayView<const TreeMessageSystem::Message> read_messages(const TreeMessageSystem* sys);
ArrayView<TreeMessageSystem::Message> get_messages(TreeMessageSystem* sys);

} //  tree