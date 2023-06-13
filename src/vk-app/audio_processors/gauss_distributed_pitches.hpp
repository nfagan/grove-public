#pragma once

namespace grove::gdp {

struct Config {
  static constexpr int oct_off = -3;
  static constexpr int oct_span = 7;
  static constexpr int st_div = 25; //  divisions of semitone
  static constexpr int st_buffer_size = 12 * oct_span * st_div;
  static constexpr int st_gauss_half_width = 5;
  static constexpr int ref_st = 60;
  static constexpr int root_st = ref_st + Config::oct_off * 12;
};

struct Distribution {
  float z[Config::st_buffer_size];
  float sorted_z[Config::st_buffer_size];
  double p[Config::st_buffer_size];
  double sorted_p[Config::st_buffer_size];
  int tmp_i[Config::st_buffer_size];
};

}