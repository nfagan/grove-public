#pragma once

namespace grove {
  class Window;
}

class grove::Window {
public:
  struct Dimensions {
    int width;
    int height;
    
    float aspect_ratio() const {
      return float(width) / float(height);
    }
  };
  
public:
  virtual ~Window() = default;
  
  virtual void swap_buffers() const = 0;
  virtual void poll_events() const = 0;
  virtual bool should_close() const = 0;
  virtual Dimensions dimensions() const = 0;
  virtual Dimensions framebuffer_dimensions() const = 0;
  virtual void set_vsync(bool to) const = 0;
  virtual void set_swap_interval(int to) const = 0;
  virtual void close_if_escape_pressed() = 0;
  virtual void close() = 0;

  float framebuffer_aspect_ratio() const;
};
