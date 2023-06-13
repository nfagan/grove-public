#pragma once

#include <cstddef>

namespace grove {

float rand();
void srand(unsigned int seed);

double urand();
double urand_closed();
double urand_11();

float urandf();
float urand_11f();

template <typename T>
T* uniform_array_sample(T* array, size_t size) {
  return size == 0 ? nullptr : array + size_t(double(size) * urand());
}

}
