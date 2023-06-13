#include "random.hpp"
#include "grove/common/common.hpp"
#include <cstdlib>
#include <random>

GROVE_NAMESPACE_BEGIN

float rand() {
  return float(double(std::rand()) / double(RAND_MAX));
}

void srand(unsigned int seed) {
  std::srand(seed);
}

double urand() {
  thread_local static std::random_device rd;
  thread_local static std::mt19937 gen(rd());
  thread_local static std::uniform_real_distribution<> dis(0.0, 1.0);

  return dis(gen);
}

double urand_closed() {
  thread_local static std::random_device rd;
  thread_local static std::mt19937 gen(rd());
  thread_local static std::uniform_real_distribution<> dis(0.0,
    std::nextafter(1.0, std::numeric_limits<double>::max()));

  return dis(gen);
}

double urand_11() {
  return urand() * 2.0 - 1.0;
}

float urandf() {
  return float(urand());
}

float urand_11f() {
  return float(urand_11());
}

GROVE_NAMESPACE_END
