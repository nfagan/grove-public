function(configure_gl)

set(sources
  GLBuffer.hpp
  GLBuffer.cpp
  context.hpp
  context.cpp
  debug.hpp
  debug.cpp
  debug/debug_draw.hpp
  debug/debug_draw.cpp
  DrawComponent.hpp
  DrawComponent.cpp
  GLKeyboard.hpp
  GLKeyboard.cpp
  GLMouse.hpp
  GLMouse.cpp
  GLFramebuffer.hpp
  GLFramebuffer.cpp
  GLProfiler.hpp
  GLProfiler.cpp
  GLRenderContext.hpp
  GLRenderContext.cpp
  GLResource.hpp
  GLBufferTexture.hpp
  GLBufferTexture.cpp
  GLTexture.hpp
  GLTexture.cpp
  GLTexture2.hpp
  GLTexture2.cpp
  GLTexture3.hpp
  GLTexture3.cpp
  GLTexture2Array.hpp
  GLTexture2Array.cpp
  GLTextureCube.hpp
  GLTextureCube.cpp
  GLRenderbuffer.hpp
  GLRenderbuffer.cpp
  GLWindow.hpp
  GLWindow.cpp
  Program.hpp
  Program.cpp
  ProgramComponent.hpp
  ProgramComponent.cpp
  Shader.hpp
  Shader.cpp
  TextureStack.hpp
  TextureStack.cpp
  types.hpp
  types.cpp
  UniformLocationMap.hpp
  UniformLocationMap.cpp
  Vao.hpp
  Vao.cpp
  VertexBufferArray.hpp
  VertexBufferArray.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/gl/${source})
endforeach()

endfunction(configure_gl)

configure_gl()