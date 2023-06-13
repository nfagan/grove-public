#include "properties.hpp"
#include "property_inspector.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

inline void copy_other(EditorPropertyData& self, const EditorPropertyData& other) {
  using Type = EditorPropertyData::Type;
  using Custom = EditorPropertyData::CustomPropertyData;

  switch (other.type) {
    case Type::Float:
      self.data.f = other.data.f;
      break;
    case Type::Int:
      self.data.i = other.data.i;
      break;
    case Type::Bool:
      self.data.b = other.data.b;
      break;
    case Type::Vec3:
      self.data.v = other.data.v;
      break;
    case Type::Custom:
      new (&self.data.custom) Custom{other.data.custom->clone()};
      break;
    default:
      assert(false);
      self.data.f = 0.0f;
  }
}

inline void maybe_move_other(EditorPropertyData& self, EditorPropertyData&& other) noexcept {
  using Type = EditorPropertyData::Type;
  using Custom = EditorPropertyData::CustomPropertyData;

  switch (self.type) {
    case Type::Float:
      self.data.f = other.data.f;
      break;
    case Type::Int:
      self.data.i = other.data.i;
      break;
    case Type::Bool:
      self.data.b = other.data.b;
      break;
    case Type::Vec3:
      self.data.v = other.data.v;
      break;
    case Type::Custom:
      new (&self.data.custom) Custom{std::move(other.data.custom)};
      break;
  }
}

using EntityFromDescr = std::function<Entity(const EditorPropertyDescriptor& descriptor)>;
using FindChangeBegin =
  std::function<const EditorPropertyChange*(const EditorPropertyChange*,
                                            const EditorPropertyChange*,
                                            Entity)>;

inline EditorPropertyChangeView make_sub_view(const EditorPropertyChangeView& view,
                                              const Entity& entity,
                                              const FindChangeBegin& find_change_begin,
                                              const EntityFromDescr& get_entity) {
  auto* begin = view.begin_;
  auto* end = view.end_;

  auto sub_begin = find_change_begin(begin, end, entity);

  int64_t ind_beg = sub_begin - begin;
  int64_t ind_end = ind_beg;
  const int64_t num_changes = end - begin;

  while (ind_end < num_changes &&
         get_entity(begin[ind_end].descriptor) == entity) {
    ind_end++;
  }

  return {begin + ind_beg, begin + ind_end};
}

} //  anon

Optional<grove::EditorPropertyData>
properties::Mat4::gui_render(const EditorPropertyDescriptor& descriptor) const {
  return imgui_render_mat4_property_data(descriptor, m);
}

/*
 * EditorPropertyData
 */

EditorPropertyData::EditorPropertyData(float f) : type{Type::Float} {
  data.f = f;
}

EditorPropertyData::EditorPropertyData(int i) : type{Type::Int} {
  data.i = i;
}

EditorPropertyData::EditorPropertyData(bool b) : type{Type::Bool} {
  data.b = b;
}

EditorPropertyData::EditorPropertyData(const Vec3f& v) : type{Type::Vec3} {
  data.v = v;
}

EditorPropertyData::EditorPropertyData(CustomPropertyData custom) : type{Type::Custom} {
  new (&data.custom) CustomPropertyData{std::move(custom)};
}

EditorPropertyData::~EditorPropertyData() {
  if (type == Type::Custom) {
    data.custom.~CustomPropertyData();
  }
}

EditorPropertyData::EditorPropertyData(EditorPropertyData&& other) noexcept :
  type{other.type} {
  //
  maybe_move_other(*this, std::move(other));
}

EditorPropertyData& EditorPropertyData::operator=(EditorPropertyData&& other) noexcept {
  assert(this != &other);
  this->~EditorPropertyData();
  type = other.type;
  maybe_move_other(*this, std::move(other));
  return *this;
}

EditorPropertyData::EditorPropertyData(const EditorPropertyData& other) :
  type{other.type} {
  //
  copy_other(*this, other);
}

EditorPropertyData& EditorPropertyData::operator=(const EditorPropertyData& other) {
  assert(this != &other);
  this->~EditorPropertyData();
  type = other.type;
  copy_other(*this, other);
  return *this;
}

Optional<float> EditorPropertyData::read_float() const {
  return type == Type::Float ? Optional<float>(data.f) : NullOpt{};
}

Optional<int> EditorPropertyData::read_int() const {
  return type == Type::Int ? Optional<int>(data.i) : NullOpt{};
}

Optional<bool> EditorPropertyData::read_bool() const {
  return type == Type::Bool ? Optional<bool>(data.b) : NullOpt{};
}

Optional<Vec3f> EditorPropertyData::read_vec3() const {
  return type == Type::Vec3 ? Optional<Vec3f>(data.v) : NullOpt{};
}

Optional<EditorPropertyData::CustomPropertyData> EditorPropertyData::read_custom() const {
  if (type == Type::Custom) {
    return Optional<CustomPropertyData>(data.custom->clone());
  } else {
    return NullOpt{};
  }
}

/*
 * EditorPropertyChangeView
 */

EditorPropertyChangeView EditorPropertyChangeView::view_by_parent(const Entity& parent) const {
  const auto get_entity = [](auto& descriptor) {
    return descriptor.ids.parent;
  };

  const auto find_begin = [&get_entity](auto* b, auto* e, const Entity& entity) {
    return std::lower_bound(b, e, entity, [&](auto& change, const Entity& search) {
      return get_entity(change.descriptor) < search;
    });
  };

  return make_sub_view(*this, parent, find_begin, get_entity);
}

EditorPropertyChangeView EditorPropertyChangeView::view_by_self(const Entity& self) const {
  const auto get_entity = [](auto& descriptor) {
    return descriptor.ids.self;
  };

  const auto find_begin = [&get_entity](auto* b, auto* e, const Entity& entity) {
    return std::find_if(b, e, [&](const auto& change) {
      return get_entity(change.descriptor) == entity;
    });
  };

  return make_sub_view(*this, self, find_begin, get_entity);
}

/*
 * EditorPropertyManager
 */

Optional<EditorPropertyHistoryItem> EditorPropertyManager::History::pop() {
  if (items.empty()) {
    return NullOpt{};

  } else {
    auto item = Optional<EditorPropertyHistoryItem>(std::move(items.back()));
    items.pop_back();
    return item;
  }
}

void EditorPropertyManager::History::clear() {
  items.clear();
}

void EditorPropertyManager::History::push(EditorPropertyHistoryItem item) {
  if (items.size() == max_num_items) {
    for (int i = 1; i < max_num_items; i++) {
      items[i-1] = std::move(items[i]);
    }
    items[max_num_items-1] = std::move(item);

  } else {
    items.push_back(std::move(item));
  }
}

void EditorPropertyManager::update() {
  read->clear();
  write->sort();

  std::swap(write, read);
}

void EditorPropertyManager::push_change(EditorPropertyChange change) {
  write->changes.push_back(std::move(change));
}

void EditorPropertyManager::commit(EditorPropertyHistoryItem item) {
  undo_history.push(std::move(item));
  redo_history.clear();
}

namespace {

EditorPropertyChange push_to_history(EditorPropertyHistoryItem&& val,
                                     EditorPropertyManager::History& history) {
  history.push(EditorPropertyHistoryItem{
    val.descriptor, std::move(val.new_value), val.original_value
  });

  return EditorPropertyChange{val.descriptor, std::move(val.original_value), true};
}

} //  anon

void EditorPropertyManager::undo() {
  if (auto item = undo_history.pop()) {
    push_change(push_to_history(std::move(item.value()), redo_history));
  }
}

void EditorPropertyManager::redo() {
  if (auto item = redo_history.pop()) {
    push_change(push_to_history(std::move(item.value()), undo_history));
  }
}

/*
 * EditorPropertySetManager
 */

void EditorPropertySetManager::push_new_set(EditorPropertySet&& set) {
  new_editor_property_sets.push_back(std::move(set));
}

void EditorPropertySetManager::remove_set(Entity parent_id) {
  remove_editor_property_sets.push_back(parent_id);
}

void EditorPropertySetManager::clear_sets() {
  new_editor_property_sets.clear();
  remove_editor_property_sets.clear();
}

GROVE_NAMESPACE_END
