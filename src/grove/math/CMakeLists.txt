function(configure_math)

set(sources
  bezier.hpp
  bezier.cpp
  bounds.hpp
  cdt.hpp
  cdt.cpp
  debug/cdt.hpp
  debug/cdt.cpp
  constants.hpp
  constants.cpp
  ease.hpp
  frame.hpp
  Frustum.hpp
  intersect.hpp
  intersect.cpp
  GridIterator3.hpp
  random.hpp
  random.cpp
  triangle.hpp
  triangle.cpp
  triangle_search.hpp
  triangle_search.cpp
  Ray.hpp
  util.hpp
  util.cpp
  Bounds3.hpp
  Bounds2.hpp
  OBB3.hpp
  Octree.hpp
  triangulation.hpp
  triangulation.cpp
  Vec2.hpp
  Vec3.hpp
  Vec4.hpp
  Mat2.hpp
  Mat3.hpp
  Mat3x4.hpp
  Mat4.hpp
  vector.hpp
  matrix.hpp
  matrix_transform.hpp
  string_cast.hpp
  window.hpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/math/${source})
endforeach()

endfunction(configure_math)

configure_math()