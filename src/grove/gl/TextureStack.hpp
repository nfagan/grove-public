#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>

namespace grove {
  class TextureStack;
}

class grove::TextureStack {
private:
  using IdArray = std::array<uint64_t, limits::max_num_active_textures>;
  
public:
  TextureStack();
  
  void push_texture_frame();
  void pop_texture_frame();
  int next_free_index(uint64_t id);
  
private:
  std::array<IdArray, limits::max_num_texture_stack_frames> active_textures;
  std::array<int, limits::max_num_texture_stack_frames> num_active_per_frame;
  int current_frame;

  static constexpr int max_num_frames = limits::max_num_texture_stack_frames;
  static constexpr int max_num_active_textures = limits::max_num_active_textures;
};
