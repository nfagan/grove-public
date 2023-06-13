#include "TextureStack.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <algorithm>

GROVE_NAMESPACE_BEGIN

TextureStack::TextureStack() :
  current_frame(-1) {
  //
  std::fill(num_active_per_frame.begin(), num_active_per_frame.end(), 0);
}

void TextureStack::push_texture_frame() {
  if (current_frame == max_num_frames-1) {
    GROVE_LOG_ERROR_CAPTURE_META("Attempted to push beyond the stack frame limit.", "TextureStack");
    return;
  }
  
  current_frame++;
}

int TextureStack::next_free_index(uint64_t id) {
  if (current_frame < 0) {
    GROVE_LOG_ERROR_CAPTURE_META("Called get_active_index before pushing stack frame.", "TextureStack");
    return -1;
  }
  
  auto& textures_this_frame = active_textures[current_frame];
  const int num_this_frame = num_active_per_frame[current_frame];
  
  for (int i = 0; i < num_this_frame; i++) {
    if (textures_this_frame[i] == id) {
      return i;
    }
  }
  
  if (num_this_frame == max_num_active_textures) {
    GROVE_LOG_ERROR_CAPTURE_META("Exceeded stack frame capacity.", "TextureStack");
    return -1;
  } else {
    textures_this_frame[num_this_frame] = id;
    return num_active_per_frame[current_frame]++;
  }
}

void TextureStack::pop_texture_frame() {
  if (current_frame == -1) {
    GROVE_LOG_ERROR_CAPTURE_META("Attempted to pop below -1.", "TextureStack");
    return;
  }
  
  num_active_per_frame[current_frame] = 0;
  std::fill(active_textures[current_frame].begin(), active_textures[current_frame].end(), 0);
  current_frame--;
}

GROVE_NAMESPACE_END
