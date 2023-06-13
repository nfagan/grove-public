#include "Directional.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

input::Directional::Directional() : x(0.0), z(0.0) {
  //
}

void input::Directional::set_x(double amount) {
  x = clamp(amount, -1.0, 1.0);
}

void input::Directional::set_z(double amount) {
  z = clamp(amount, -1.0, 1.0);
}

void input::Directional::add_x(double amount) {
  x = clamp(x + amount, -1.0, 1.0);
}

void input::Directional::add_z(double amount) {
  z = clamp(z + amount, -1.0, 1.0);
}

double input::Directional::get_x() const {
  return x;
}

double input::Directional::get_z() const {
  return z;
}

void input::Directional::clear() {
  x = 0.0;
  z = 0.0;
}

GROVE_NAMESPACE_END
