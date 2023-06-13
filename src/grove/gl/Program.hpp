#pragma once

#include "GLResource.hpp"
#include "grove/common/common.hpp"
#include "grove/math/matrix.hpp"
#include "grove/math/vector.hpp"

#include <vector>
#include <string>

namespace grove {

class Shader;
class Program;
class GLTexture;
class GLTexture2;
class GLTexture3;
class GLTexture2Array;

Program make_program(const char* vertex_source, const char* fragment_source, bool* success);
Program make_compute_program(const char* source, bool* success);

namespace gl {
  struct ProgramLifecycle {
    static void create(int num, unsigned int* id);
    static void dispose(int num, unsigned int* id);
  };
}

namespace detail {
  template <typename T>
  struct UniformSetter;
}

}

class grove::Program {
  friend class GLRenderContext;
public:
  Program() = default;
  ~Program() = default;

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(Program)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(Program)

  unsigned int get_instance_handle() const;
  
  int uniform_location(const char* name) const;
  std::vector<std::string> active_uniform_names() const;
  
  void set_int(int location, int value) const;
  void set_float(int location, float value) const;
  void set_float2(int location, float x, float y) const;
  void set_float3(int location, float x, float y, float z) const;
  void set_float4(int location, float x, float y, float z, float w) const;
  void set_texture(int location, const GLTexture& texture) const;

  void set_vec2(int location, const Vec2f& vec) const;
  void set_vec3(int location, const Vec3f& vec) const;
  void set_vec4(int location, const Vec4f& vec) const;
  void set_ivec2(int location, const Vec2<int>& vec) const;
  void set_ivec3(int location, const Vec3<int>& vec) const;
  void set_ivec4(int location, const Vec4<int>& vec) const;
  void set_mat4(int location, const Mat4f& mat) const;

  template <typename T>
  void set(int location, const T& value) const {
    detail::UniformSetter<T>::set(*this, location, value);
  }
  
  void create();
  void dispose();
  bool attach_link_dispose_shaders(Shader* shaders, int num_shaders, bool dispose_on_error = true) const;
  
  bool is_valid() const;
  
private:
  void bind() const;
  void unbind() const;
  bool attach(const Shader& shader) const;
  bool link() const;
  bool check_created() const;
  
private:
  GLResource<gl::ProgramLifecycle> instance;
};

/*
 * UniformSetter impl
 */

namespace grove {
  namespace detail {
    template <>
    struct UniformSetter<int> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_int(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<bool> {
      static void set(const Program& prog, int loc, bool value) {
        prog.set_int(loc, int(value));
      }
    };

    template <>
    struct UniformSetter<float> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_float(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<Vec2f> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_vec2(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<Vec3f> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_vec3(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<Vec4f> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_vec4(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<Mat4f> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_mat4(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<GLTexture> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_texture(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<GLTexture2> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_texture(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<GLTexture3> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_texture(std::forward<Args>(args)...);
      }
    };

    template <>
    struct UniformSetter<GLTexture2Array> {
      template <typename... Args>
      static void set(const Program& prog, Args&&... args) {
        prog.set_texture(std::forward<Args>(args)...);
      }
    };
  }
}