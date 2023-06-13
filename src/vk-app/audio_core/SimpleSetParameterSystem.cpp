#include "SimpleSetParameterSystem.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

struct ParamNode {
  AudioNodeStorage::NodeID id;
  const char* name;
  Optional<AudioParameterDescriptor> found_desc;
};

struct SimpleSetParameterSystem {
  const AudioNodeStorage* node_storage{};
  AudioParameterSystem* param_sys{};
  std::vector<ParamNode> nodes;
};

namespace {

bool less_by_node_id(const ParamNode& a, AudioNodeStorage::NodeID id) {
  return a.id < id;
}

auto find_node_begin(std::vector<ParamNode>& params, AudioNodeStorage::NodeID id) {
  return std::lower_bound(params.begin(), params.end(), id, less_by_node_id);
}

bool require_param_desc(SimpleSetParameterSystem* sys, ParamNode* node, const char* pname) {
  if (node->found_desc) {
    return true;
  }

  Temporary<AudioParameterDescriptor, 256> store_param_descs;
  auto param_descs = store_param_descs.view_stack();
  sys->node_storage->audio_parameter_descriptors(node->id, param_descs);

  for (auto& p : param_descs) {
    if (std::strcmp(p.name, pname) == 0) {
      node->found_desc = p;
      return true;
    }
  }

#ifdef GROVE_DEBUG
  if (sys->node_storage->is_instance_created(node->id)) {
    std::string warn{"No such parameter: "};
    warn += pname;
    GROVE_LOG_WARNING_CAPTURE_META(warn.c_str(), "SimpleSetParameterSystem");
  }
#endif

  return false;
}

ParamNode* require_param_node(
  SimpleSetParameterSystem* sys, AudioNodeStorage::NodeID node, const char* pname) {
  //
  auto it = find_node_begin(sys->nodes, node);
  while (it != sys->nodes.end() && it->id == node) {
    if (it->name == pname) {
      return &*it;
    }
    ++it;
  }

  ParamNode pnode{};
  pnode.id = node;
  pnode.name = pname;
  ParamNode* res = &*sys->nodes.insert(it, pnode);
  assert(std::is_sorted(sys->nodes.begin(), sys->nodes.end(), [](auto& a, auto& b) {
    return a.id < b.id;
  }));
  return res;
}

struct {
  SimpleSetParameterSystem sys;
} globals;

} //  anon

SimpleSetParameterSystem* param_system::get_global_simple_set_parameter_system() {
  return &globals.sys;
}

void param_system::ui_initialize(
  SimpleSetParameterSystem* sys, const AudioNodeStorage* node_storage,
  AudioParameterSystem* param_sys) {
  //
  sys->node_storage = node_storage;
  sys->param_sys = param_sys;
}

bool param_system::ui_set_int_value(
  SimpleSetParameterSystem* sys, AudioNodeStorage::NodeID node, const char* pname, int v) {
  //
  ParamNode* pnode = require_param_node(sys, node, pname);
  if (!require_param_desc(sys, pnode, pname)) {
    return false;
  }

  assert(pnode->found_desc);
  auto& desc = pnode->found_desc.value();
  if (!desc.is_int()) {
    assert(false);
    return false;
  }

  assert(v >= desc.min.i && v <= desc.max.i);
  auto val = make_int_parameter_value(v);
  return param_system::ui_set_value_if_no_other_writer(sys->param_sys, desc.ids, val);
}

bool param_system::ui_set_float_value_from_fraction(
  SimpleSetParameterSystem* sys, AudioNodeStorage::NodeID node, const char* pname, float v01) {
  //
  assert(v01 >= 0.0f && v01 <= 1.0f);

  ParamNode* pnode = require_param_node(sys, node, pname);
  if (!require_param_desc(sys, pnode, pname)) {
    return false;
  }

  assert(pnode->found_desc);
  auto& desc = pnode->found_desc.value();
  if (!desc.is_float()) {
    assert(false);
    return false;
  }

  auto val = make_interpolated_parameter_value_from_descriptor(desc, v01);
  return param_system::ui_set_value_if_no_other_writer(sys->param_sys, desc.ids, val);
}

void param_system::ui_evaluate_deleted_nodes(
  SimpleSetParameterSystem* sys, const ArrayView<AudioNodeStorage::NodeID>& del) {
  //
  for (AudioNodeStorage::NodeID id : del) {
    auto it = find_node_begin(sys->nodes, id);
    if (it == sys->nodes.end() || it->id != id) {
      continue;
    }
    auto beg = it;
    while (it != sys->nodes.end() && it->id == id) {
      ++it;
    }
    sys->nodes.erase(beg, it);
  }
}

GROVE_NAMESPACE_END
