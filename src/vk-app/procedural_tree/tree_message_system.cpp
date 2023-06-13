#include "tree_message_system.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/ease.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace msg;
using namespace tree;

[[maybe_unused]] void write_float(MessageData* dst, float data) {
  static_assert(MessageData::num_bytes >= sizeof(float));
  memcpy(&dst->bytes, &data, sizeof(float));
}

void read_float(const MessageData& src, float* payload) {
  static_assert(MessageData::num_bytes >= sizeof(float));
  memcpy(payload, src.bytes, sizeof(float));
}

void write_float3(MessageData& dst, const float* data) {
  static_assert(MessageData::num_bytes >= sizeof(float) * 3);
  memcpy(&dst.bytes, data, sizeof(float) * 3);
}

void read_float3(const MessageData& src, float* payload) {
  static_assert(MessageData::num_bytes >= sizeof(float) * 3);
  memcpy(payload, src.bytes, sizeof(float) * 3);
}

MessageData make_zero_message_data() {
  MessageData data{};
  return data;
}

Message make_message(float size, float speed, MessageData data) {
  Message result{};
  result.id = MessageID::next();
  result.domain = MessageDomain::Tree;
  result.size = size;
  result.speed = speed;
  result.data = data;
  return result;
}

Vec3f child_position(const Internode* internodes, const Internode* curr) {
  if (curr->has_medial_child()) {
    return internodes[curr->medial_child].position;
  } else {
    return curr->tip_position();
  }
}

Vec3f internode_position(const Internode* internodes, const Internode* curr, float f) {
  auto p0 = curr->position;
  auto p1 = child_position(internodes, curr);
  return (p1 - p0) * f + p0;
}

bool move_towards_root(const Internode* internodes, int num_internodes, const Internode** curr,
                       float* curr_frac, float remaining_dist) {
  auto p0 = (*curr)->position;
  auto p1 = child_position(internodes, *curr);

  assert(*curr_frac >= 0.0f && *curr_frac <= 1.0f);
  auto to_p1 = p1 - p0;
  auto curr_p = to_p1 * *curr_frac + p0;
  auto to_next = p0 - curr_p;
  auto to_next_len = to_next.length();

  if (remaining_dist < to_next_len) {
    float f_rem = remaining_dist / to_p1.length();
    *curr_frac -= f_rem;
    *curr_frac = clamp01_open(*curr_frac);
    return false;

  } else {
    if (!(*curr)->has_parent()) {
      *curr_frac = 0.0f;
      return true;
    }
    remaining_dist -= to_next_len;
    *curr_frac = 1.0f;
    *curr = internodes + (*curr)->parent;
    return move_towards_root(internodes, num_internodes, curr, curr_frac, remaining_dist);
  }
}

bool move_towards_leaves(const Internode* internodes, int num_internodes, const Internode** curr,
                         float* curr_frac, float remaining_dist, float p_lat) {
  auto p0 = (*curr)->position;
  auto p1 = child_position(internodes, *curr);

  assert(*curr_frac >= 0.0f && *curr_frac <= 1.0f);
  auto to_p1 = p1 - p0;
  auto curr_p = to_p1 * *curr_frac + p0;
  auto to_next = p1 - curr_p;
  auto to_next_len = to_next.length();

  if (remaining_dist < to_next_len) {
    float f_rem = remaining_dist / to_p1.length();
    *curr_frac += f_rem;
    *curr_frac = clamp01_open(*curr_frac);
    return false;

  } else {
    if (!(*curr)->has_medial_child()) {
      *curr_frac = 1.0f;
      return true;
    }
    remaining_dist -= to_next_len;
    *curr_frac = 0.0f;
    auto* next = internodes + (*curr)->medial_child;
    if (urandf() < p_lat && (*curr)->has_lateral_child()) {
      next = internodes + (*curr)->lateral_child;
    }
    *curr = next;
    return move_towards_leaves(internodes, num_internodes, curr, curr_frac, remaining_dist, p_lat);
  }
}

struct FindNearbyLeafParams {
  Vec3f tip_position;
  float search_radius;
  bounds::ElementID tree_parent_id;
  bounds::ElementTag tree_tag;
};

struct FoundNearbyLeaf {
  bounds::ElementID parent_id;
  bounds::ElementID internode_id;
};

Optional<FoundNearbyLeaf> find_nearby_leaf(const bounds::Accel* accel,
                                           std::vector<const bounds::Element*>& hits,
                                           const FindNearbyLeafParams& params) {
  auto aabb = OBB3f::axis_aligned(params.tip_position, Vec3f{params.search_radius});
  auto query = bounds::make_query_element(aabb);

  hits.clear();
  accel->intersects(query, hits);

  Temporary<const bounds::Element*, 2048> candidate_hits;
  auto candidate_stack = candidate_hits.view_stack();

  for (auto& hit : hits) {
    if (hit->tag == params.tree_tag.id && hit->parent_id != params.tree_parent_id.id) {
      *candidate_stack.push(1) = hit;
    }
  }

  assert(!candidate_stack.heap && "Allocation required.");

  if (candidate_stack.size == 0) {
    return NullOpt{};
  } else {
    auto* selected = *uniform_array_sample(candidate_stack.begin(), candidate_stack.size);
    FoundNearbyLeaf found{};
    found.parent_id = bounds::ElementID{selected->parent_id};
    found.internode_id = bounds::ElementID{selected->id};
    return Optional<FoundNearbyLeaf>(found);
  }
}

struct BoundsToTreeInstance {
  TreeInstanceHandle tree;
  TreeInternodeID internode_id;
};

Optional<BoundsToTreeInstance> to_tree_instance(const tree::TreeSystem* sys,
                                                const FoundNearbyLeaf& ids) {
  TreeInstanceHandle tree;
  Internode internode;
  int internode_index;
  bool found = tree::lookup_by_bounds_element_ids(
    sys, ids.parent_id, ids.internode_id, &tree, &internode, &internode_index);
  if (found) {
    BoundsToTreeInstance result{};
    result.tree = tree;
    result.internode_id = internode.id;
    return Optional<BoundsToTreeInstance>(result);
  } else {
    return NullOpt{};
  }
}

const tree::Internode* find_current_internode(const TreeMessageSystem::Message& message,
                                              const Internodes& inodes) {
  auto inode_it = std::find_if(
    inodes.begin(), inodes.end(), [id = message.internode_id](const tree::Internode& node) {
      return node.id == id;
    });
  if (inode_it == inodes.end()) {
    return nullptr;
  } else {
    return &*inode_it;
  }
}

bool state_moving_between_branches(TreeMessageSystem::Message& message,
                                   const TreeMessageSystemUpdateInfo& info) {
  auto target_tree = read_tree(info.tree_sys, message.tree);
  if (!target_tree.nodes) {
    return true;
  }

  const Internode* target_inode = find_current_internode(message, target_tree.nodes->internodes);
  if (!target_inode) {
    return true;
  }

  constexpr float travel_time = 3.0f;
  message.traveled_distance += float(message.message.speed * info.dt);
  message.traveled_distance = clamp(message.traveled_distance, 0.0f, travel_time);
  const float t = ease::in_out_expo(message.traveled_distance / travel_time);

  if (message.traveled_distance == travel_time) {
    message.traveled_distance = 0.0f;
    message.message_state = TreeMessageSystem::MessageState::TravelingAlongBranch;
    message.events.just_reached_new_leaf = true;
  }

  auto target_p = internode_position(target_tree.nodes->internodes.data(), target_inode, 1.0f);
  message.position = lerp(t, message.src_position, target_p);
  return false;
}

bool state_traveling_along_branch(TreeMessageSystem* sys, TreeMessageSystem::Message& message,
                                  const bounds::Accel* accel,
                                  const TreeMessageSystemUpdateInfo& info) {
  auto tree = read_tree(info.tree_sys, message.tree);
  if (!tree.nodes) {
    return true;
  }

  assert(tree.bounds_element_id.is_valid());
  const Internode* curr = find_current_internode(message, tree.nodes->internodes);
  if (!curr) {
    return true;
  }

  const auto dist = float(message.message.speed * info.dt);
  bool reached_root{};
  if (message.target_distance_to_root) {
    reached_root = move_towards_root(
      tree.nodes->internodes.data(),
      int(tree.nodes->internodes.size()),
      &curr,
      &message.frac_next_internode, dist);
    message.traveled_distance += dist;
    if (reached_root || message.traveled_distance >= message.target_distance_to_root.value()) {
      message.target_distance_to_root = NullOpt{};
      message.traveled_distance = 0.0f;
    }
  }

  bool reached_leaf_tip{};
  if (!message.target_distance_to_root && !reached_root) {
    reached_leaf_tip = move_towards_leaves(
      tree.nodes->internodes.data(),
      int(tree.nodes->internodes.size()),
      &curr,
      &message.frac_next_internode, dist, 0.25f);
  }

  message.internode_id = curr->id;
  message.position = internode_position(
    tree.nodes->internodes.data(), curr, message.frac_next_internode);

  bool erase = reached_root || reached_leaf_tip;
  //  @TODO something like: if (!accel) { state == AwaitingBounds }
  if (reached_leaf_tip && accel) {
    FindNearbyLeafParams find_params{};
    find_params.tree_parent_id = tree.bounds_element_id;
    find_params.tree_tag = tree::get_bounds_tree_element_tag(info.tree_sys);
    find_params.tip_position = curr->tip_position();
    find_params.search_radius = 8.0f;

    if (auto found = find_nearby_leaf(accel, sys->intersect_query_storage, find_params)) {
      if (auto tree_inst = to_tree_instance(info.tree_sys, found.value())) {
        auto& inst = tree_inst.value();
        message.src_position = message.position;
        message.internode_id = inst.internode_id;
        message.tree = inst.tree;
        message.frac_next_internode = 1.0f;
        message.target_distance_to_root = 16.0f;
        message.traveled_distance = 0.0f;
        message.message_state = TreeMessageSystem::MessageState::MovingBetweenBranches;
        if (urandf() < 0.1f) {
          //  almost certainly reach root.
          message.target_distance_to_root = 100000.0f;
        }
        erase = false;
      }
    }
  }

  return erase;
}

bool state_dispatch(TreeMessageSystem* sys, TreeMessageSystem::Message& message,
                    const bounds::Accel* accel, const TreeMessageSystemUpdateInfo& info) {
  switch (message.message_state) {
    case TreeMessageSystem::MessageState::TravelingAlongBranch:
      return state_traveling_along_branch(sys, message, accel, info);
    case TreeMessageSystem::MessageState::MovingBetweenBranches:
      return state_moving_between_branches(message, info);
    default:
      assert(false);
      return false;
  }
}

void remove_deleted(TreeMessageSystem* sys, const TreeSystem::DeletedInstances& just_deleted) {
  if (just_deleted.empty()) {
    return;
  }

  auto it = sys->messages.begin();
  while (it != sys->messages.end()) {
    if (just_deleted.count(it->tree)) {
      it = sys->messages.erase(it);
    } else {
      ++it;
    }
  }
}

void update_messages(TreeMessageSystem* sys, const bounds::Accel* accel,
                     const TreeMessageSystemUpdateInfo& info) {
  auto msg_it = sys->messages.begin();
  while (msg_it != sys->messages.end()) {
    if (state_dispatch(sys, *msg_it, accel, info)) {
      msg_it = sys->messages.erase(msg_it);
    } else {
      ++msg_it;
    }
  }
}

} //  anon

std::atomic<uint64_t> msg::MessageID::next_id{1};
MessageID msg::MessageID::next() {
  return MessageID{next_id++};
}

Message tree::make_zero_message(float size, float speed) {
  return make_message(size, speed, make_zero_message_data());
}

TreeMessageSystem::Message tree::make_tree_message(msg::Message message, TreeInstanceHandle tree,
                                                   TreeInternodeID src_internode, const Vec3f& pos) {
  TreeMessageSystem::Message result{};
  result.message = message;
  result.tree = tree;
  result.internode_id = src_internode;
  result.position = pos;
  result.message_state = TreeMessageSystem::MessageState::TravelingAlongBranch;
  return result;
}

void tree::push_message(TreeMessageSystem* sys, const TreeMessageSystem::Message& message) {
  sys->messages.push_back(message);
}

ArrayView<const TreeMessageSystem::Message> tree::read_messages(const TreeMessageSystem* sys) {
  return make_view(sys->messages);
}

ArrayView<TreeMessageSystem::Message> tree::get_messages(TreeMessageSystem* sys) {
  return make_mut_view(sys->messages);
}

void tree::update(TreeMessageSystem* sys, const TreeMessageSystemUpdateInfo& info) {
  const bounds::Accel* accel{};
  if (!sys->messages.empty()) {
    accel = bounds::request_read(info.bounds_sys, info.accel_handle, sys->bounds_accessor_id);
  }

  for (auto& msg : sys->messages) {
    msg.events = {};
  }

  remove_deleted(sys, *info.just_deleted);
  update_messages(sys, accel, info);

  if (accel) {
    bounds::release_read(info.bounds_sys, info.accel_handle, sys->bounds_accessor_id);
  }
}

float MessageData::read_float() const {
  float res;
  grove::read_float(*this, &res);
  return res;
}

Vec3f MessageData::read_vec3f() const {
  Vec3f result;
  grove::read_float3(*this, &result.x);
  return result;
}

void MessageData::write_vec3f(const Vec3f& data) {
  grove::write_float3(*this, &data.x);
}

GROVE_NAMESPACE_END
