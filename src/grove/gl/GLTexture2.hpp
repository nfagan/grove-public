#pragma once

#include "GLTexture.hpp"
#include "types.hpp"
#include "grove/visual/types.hpp"
#include "grove/visual/Texture.hpp"

namespace grove {

class GLTexture2 : public GLTexture {
public:
  GLTexture2();
  explicit GLTexture2(int width_height);
  GLTexture2(int width, int height);

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLTexture2)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLTexture2)

  void set_border_color(const float* rgba) const;

  int get_width() const;
  int get_height() const;

  void set_dimensions(int width_height);
  void set_dimensions(int w, int h);
  
  void fill(int level, TextureFormat internal_format, TextureFormat source_format,
            IntegralType type, const void* data) const;

  //  internal_format: internal representation, and number of components (e.g., GL_R32F)
  //  source_format: abstract representation (e.g., GL_RED)
  void fill(int level, int internal_format, unsigned int source_format,
            unsigned int type, const void* data) const;
  
  void fill8(int level, int num_components, const void* data) const;
  void fill8_srgb(int level, int num_components, const void* data) const; //  3 or 4 components
  void fill_rgba8(int level, const void* data, bool reverse_upload = true) const;
  
  //  Refill subset
  void refill(int level, int x_offset, int y_offset, int width_subset, int height_subset,
              TextureFormat source_format, IntegralType data_type, const void* data) const;
  void refill(int level, int x_offset, int y_offset, int width_subset, int height_subset,
              unsigned int source_format, unsigned int data_type, const void* data) const;
  //  Refill all
  void refill(int level, TextureFormat source_format, IntegralType data_type, const void* data) const;
  void refill(int level, unsigned int source_format, unsigned int type, const void* data) const;
  
private:
  int width;
  int height;
};

}