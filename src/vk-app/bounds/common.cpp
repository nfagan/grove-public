#include "common.hpp"
#include "grove/common/common.hpp"
#include <atomic>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace bounds;

std::atomic<uint32_t> next_accessor_id{1};
std::atomic<uint32_t> next_element_id{1};
std::atomic<uint32_t> next_element_tag{1};

} //  anon

AccessorID bounds::AccessorID::create() {
  return AccessorID{next_accessor_id++};
}

ElementID bounds::ElementID::create() {
  return ElementID{next_element_id++};
}

ElementTag bounds::ElementTag::create() {
  return ElementTag{next_element_tag++};
}

GROVE_NAMESPACE_END
