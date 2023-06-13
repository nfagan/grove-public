#pragma once

#include "entity.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Optional.hpp"
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <functional>

namespace grove {

/*
 * EditorPropertyData
 */

struct EditorPropertyData;
struct EditorPropertyDescriptor;

struct CustomEditorPropertyData {
public:
  using BoxedSelf = std::unique_ptr<CustomEditorPropertyData>;

public:
  virtual ~CustomEditorPropertyData() = default;
  virtual BoxedSelf clone() const = 0;
  virtual Optional<grove::EditorPropertyData> gui_render(const EditorPropertyDescriptor& descriptor) const = 0;
};

namespace properties {
  struct Mat4 : public CustomEditorPropertyData {
  public:
    Mat4(const Mat4f& m) : m{m} {
      //
    }
    ~Mat4() override = default;
    BoxedSelf clone() const override {
      return std::make_unique<Mat4>(m);
    }
    Optional<grove::EditorPropertyData>
    gui_render(const EditorPropertyDescriptor& descriptor) const override;

  public:
    Mat4f m;
  };
}

struct EditorPropertyData {
public:
  using CustomPropertyData =
    std::unique_ptr<CustomEditorPropertyData>;

  enum class Type : uint8_t {
    Float,
    Int,
    Bool,
    Vec3,
    Custom
  };

  union Data {
    Data() : f{} {
      //
    }
    ~Data() {}

    float f;
    int i;
    bool b;
    Vec3f v;
    CustomPropertyData custom;
  };

public:
  explicit EditorPropertyData(float f);
  explicit EditorPropertyData(int i);
  explicit EditorPropertyData(bool b);
  explicit EditorPropertyData(const Vec3f& v);
  explicit EditorPropertyData(CustomPropertyData custom);

  EditorPropertyData() = default;
  ~EditorPropertyData();

  EditorPropertyData(const EditorPropertyData& other);
  EditorPropertyData& operator=(const EditorPropertyData& other);

  EditorPropertyData(EditorPropertyData&& other) noexcept;
  EditorPropertyData& operator=(EditorPropertyData&& other) noexcept;

  Optional<Vec3f> read_vec3() const;
  Optional<float> read_float() const;
  Optional<int> read_int() const;
  Optional<bool> read_bool() const;
  Optional<CustomPropertyData> read_custom() const;

  template <typename T>
  T read_or_default(const T& v) const;

public:
  Type type{Type::Float};
  Data data{};
};

namespace detail {
  template <typename T>
  struct OptionalReader {};

  template <>
  struct OptionalReader<float> {
    static bool maybe_read(float* out, const EditorPropertyData& data) {
      if (auto v = data.read_float()) {
        *out = v.value();
        return true;
      } else {
        return false;
      }
    }
  };

  template <>
  struct OptionalReader<int> {
    static bool maybe_read(int* out, const EditorPropertyData& data) {
      if (auto v = data.read_int()) {
        *out = v.value();
        return true;
      } else {
        return false;
      }
    }
  };

  template <>
  struct OptionalReader<bool> {
    static bool maybe_read(bool* out, const EditorPropertyData& data) {
      if (auto v = data.read_bool()) {
        *out = v.value();
        return true;
      } else {
        return false;
      }
    }
  };

  template <>
  struct OptionalReader<Vec3f> {
    static bool maybe_read(Vec3f* out, const EditorPropertyData& data) {
      if (auto v = data.read_vec3()) {
        *out = v.value();
        return true;
      } else {
        return false;
      }
    }
  };

  template <>
  struct OptionalReader<EditorPropertyData::CustomPropertyData> {
    static bool maybe_read(EditorPropertyData::CustomPropertyData* out,
                           const EditorPropertyData& data) {
      if (auto v = data.read_custom()) {
        *out = std::move(v.value());
        return true;
      } else {
        return false;
      }
    }
  };
}

template <typename T>
T EditorPropertyData::read_or_default(const T& v) const {
  T res;
  if (detail::OptionalReader<T>::maybe_read(&res, *this)) {
    return res;
  } else {
    return v;
  }
}

/*
 * EditorPropertyIDs
 */

struct EditorPropertyIDs {
  Entity parent{};
  Entity self{};

  friend inline bool operator==(const EditorPropertyIDs& a, const EditorPropertyIDs& b) {
    return a.parent == b.parent && a.self == b.self;
  }

  friend inline bool operator!=(const EditorPropertyIDs& a, const EditorPropertyIDs& b) {
    return !(a == b);
  }

  friend inline bool operator<(const EditorPropertyIDs& a, const EditorPropertyIDs& b) {
    return std::tie(a.parent, a.self) < std::tie(b.parent, b.self);
  }
};

/*
 * EditorPropertyDescriptor
 */

struct EditorPropertyDescriptor {
  EditorPropertyIDs ids;
  const char* label;
};

/*
 * EditorPropertyChange
 */

struct EditorPropertyChange {
  EditorPropertyDescriptor descriptor;
  EditorPropertyData value;
  bool by_history{false};
};

/*
 * EditorProperty
 */

struct EditorProperty {
public:
  template <typename T>
  auto read_or_default(T&& v) const {
    return data.read_or_default(std::forward<T>(v));
  }

  template <typename T>
  auto make_change(T&& v, bool by_history = false) const {
    return EditorPropertyChange{descriptor, EditorPropertyData{std::forward<T>(v)}, by_history};
  }

public:
  EditorPropertyDescriptor descriptor;
  EditorPropertyData data;
};

inline EditorPropertyDescriptor
make_editor_property_descriptor(Entity parent, Entity self, const char* label) {
  return {{parent, self}, label};
}

template <typename T>
EditorProperty make_editor_property(const EditorPropertyDescriptor& descriptor, T&& data) {
  return EditorProperty{descriptor, EditorPropertyData{std::forward<T>(data)}};
}

inline EditorPropertyData make_mat4_editor_property_data(const Mat4f& m) {
  return EditorPropertyData{std::make_unique<properties::Mat4>(m)};
}


#define GROVE_MAKE_NEW_EDITOR_PROPERTY(name, parent_id, value) \
  make_editor_property(make_editor_property_descriptor((parent_id), Entity::create(), (name)), (value))

/*
 * EditorPropertySet
 */

struct EditorPropertySet {
public:
  struct HashParent {
    inline std::size_t operator()(const EditorPropertySet& self) const noexcept {
      return Entity::Hash{}(self.parent);
    }
  };
  struct EqualParent {
    inline bool operator()(const EditorPropertySet& a, const EditorPropertySet& b) const noexcept {
      return a.parent == b.parent;
    }
  };

  using Properties = DynamicArray<EditorProperty, 8>;

public:
  EditorPropertySet() = default;
  explicit EditorPropertySet(Entity parent) : parent{parent} {
    //
  }

public:
  Entity parent;
  Properties properties;
};

/*
 * EditorPropertyChangeView
 */

struct EditorPropertyChangeView {
public:
  int64_t size() const noexcept {
    return end_ - begin_;
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  const EditorPropertyChange& operator[](int index) const noexcept {
    assert(index >= 0 && index < size());
    return begin_[index];
  }

  const EditorPropertyChange* begin() const {
    return begin_;
  }
  const EditorPropertyChange* end() const {
    return end_;
  }

  template <typename T>
  bool maybe_apply(T* out) const {
    bool any_applied = false;

    for (auto& change : *this) {
      if (detail::OptionalReader<T>::maybe_read(out, change.value)) {
        any_applied = true;
      }
    }

    return any_applied;
  }

  bool maybe_apply(EditorProperty& prop) const {
    bool any_applied = false;
    for (auto& change : *this) {
      if (change.descriptor.ids == prop.descriptor.ids) {
        prop.data = change.value;
        any_applied = true;
      }
    }
    return any_applied;
  }

  bool maybe_apply(EditorProperty& prop, EditorPropertyData* original,
                   const EditorPropertyChange** src_change) const {
    bool any_applied = false;
    for (auto& change : *this) {
      if (change.descriptor.ids == prop.descriptor.ids) {
        *original = prop.data;
        *src_change = &change;
        prop.data = change.value;
        any_applied = true;
      }
    }
    return any_applied;
  }

  EditorPropertyChangeView view_by_parent(const Entity& parent) const;
  EditorPropertyChangeView view_by_self(const Entity& self) const;

  EditorPropertyChangeView view_by_self(const EditorPropertyDescriptor& descriptor) const {
    return view_by_self(descriptor.ids.self);
  }

public:
  const EditorPropertyChange* begin_{};
  const EditorPropertyChange* end_{};
};

/*
 * EditorPropertyChanges
 */

struct EditorPropertyChanges {
public:
  void sort() noexcept {
    std::sort(changes.begin(), changes.end(), [](auto&& a, auto&& b) {
      return a.descriptor.ids < b.descriptor.ids;
    });
  }

  void clear() noexcept {
    changes.clear();
  }

public:
  DynamicArray<EditorPropertyChange, 8> changes;
};

/*
 * EditorPropertyManager
 */

struct EditorPropertyHistoryItem {
  EditorPropertyDescriptor descriptor;
  EditorPropertyData original_value;
  EditorPropertyData new_value;
};

class EditorPropertyManager {
public:
  struct History {
    static constexpr int max_num_items = 10;

  public:
    void push(EditorPropertyHistoryItem item);
    void clear();
    Optional<EditorPropertyHistoryItem> pop();

  public:
    DynamicArray<EditorPropertyHistoryItem, max_num_items> items;
  };

public:
  void update();

  void push_change(EditorPropertyChange change);
  void commit(EditorPropertyHistoryItem item);
  void undo();
  void redo();

  EditorPropertyChangeView read_changes() const {
    return {read->changes.begin(), read->changes.end()};
  }

private:
  EditorPropertyChanges changes0;
  EditorPropertyChanges changes1;

  EditorPropertyChanges* write{&changes0};
  EditorPropertyChanges* read{&changes1};

  History undo_history;
  History redo_history;
};

/*
 * EditorPropertySetManager
 */

class EditorPropertySetManager {
public:
  void push_new_set(EditorPropertySet&& set);
  void remove_set(Entity parent_id);
  void clear_sets();

  const std::vector<EditorPropertySet>& read_new_sets() const {
    return new_editor_property_sets;
  }
  const std::vector<Entity>& read_sets_to_remove() const {
    return remove_editor_property_sets;
  }

private:
  std::vector<EditorPropertySet> new_editor_property_sets;
  std::vector<Entity> remove_editor_property_sets;
};

namespace detail {
  struct PropertyDataUpdateResult {
    bool modified{};
    bool committed{};
  };
}

template <typename Array>
detail::PropertyDataUpdateResult
maybe_update_property_data(const EditorPropertyChangeView& changes,
                           EditorProperty* prop, Array& to_commit) {
  EditorPropertyData original_value;
  const EditorPropertyChange* src_change;
  detail::PropertyDataUpdateResult result{};

  if (changes.maybe_apply(*prop, &original_value, &src_change)) {
    result.modified = true;

    if (!src_change->by_history) {
      EditorPropertyHistoryItem commit{prop->descriptor, std::move(original_value), prop->data};
      to_commit.push_back(std::move(commit));
      result.committed = true;
    }
  }

  return result;
}


}