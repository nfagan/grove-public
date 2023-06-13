#include "oscillator.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"

namespace grove {

/*
 * Sin
 */

osc::Sin::Sin() :
  period_over_sr(0.0),
  current_phase(0.0),
  frequency(0.0) {
  //
}

osc::Sin::Sin(double sample_rate) :
  Sin(sample_rate, frequency_a4(), 0.0) {
  //
}

osc::Sin::Sin(double sample_rate, double frequency, double current_phase) :
  period_over_sr(grove::two_pi() / sample_rate),
  current_phase(current_phase),
  frequency(frequency) {
  //
}

/*
 * WaveTable
 */

osc::WaveTable::WaveTable() :
  WaveTable(44.1e3, frequency_a4()) {
  //
}

osc::WaveTable::WaveTable(double sample_rate, double frequency) :
  period_over_sr(double(size) / sample_rate),
  current_phase(0.0),
  frequency(frequency),
  table{} {
  //
}

double osc::WaveTable::get_frequency() const {
  return frequency;
}

void osc::WaveTable::fill_sin() {
  const double period_over_sz = grove::two_pi() / double(size);

  for (int i = 0; i < size; i++) {
    table[i] = Sample(std::sin(double(i) * period_over_sz));
  }

  table[size] = table[0];
}

void osc::WaveTable::fill_tri(int num_harms) {
  const double period_over_sz = grove::two_pi() / double(size);

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < num_harms; j++) {
      const double k = double(j) * 2.0 + 1.0;
      const double ak = 1.0 / (k * k);
      const double w = k * double(i) * period_over_sz;
      table[i] += Sample(ak * std::cos(w));
    }
  }

  table[size] = table[0];
}

void osc::WaveTable::fill_square(int num_harms) {
  const double period_over_sz = grove::two_pi() / double(size);

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < num_harms; j++) {
      const int k = j * 2 + 1;
      const double ak = 1.0 / double(k);
      const double w = double(k) * double(i) * period_over_sz;
      table[i] += Sample(ak * std::sin(w));
    }
  }

  table[size] = table[0];
}

void osc::WaveTable::fill_white_noise() {
  for (int i = 0; i < size+1; i++) {
    table[i] = urand_11f();
  }

  current_phase = 0.0;
  period_over_sr = 1.0;
  frequency = 1.0;
}

bool osc::WaveTable::fill_samples(const Sample* samples, int count) {
  if (count != size) {
    return false;
  }

  for (int i = 0; i < count; i++) {
    table[i] = samples[i];
  }

  table[size] = samples[0];
  return true;
}

void osc::WaveTable::normalize() {
  abs_max_normalize(table.data(), table.data() + table.size());
}

}