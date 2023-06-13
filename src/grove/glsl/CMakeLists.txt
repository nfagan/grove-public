function(configure_glsl)

set(sources
  preprocess.hpp
  preprocess.cpp
  parse/parse.hpp
  parse/parse.cpp
  parse/ast.hpp
  parse/ast.cpp
  parse/visitor.hpp
  scan/keyword.hpp
  scan/keyword.cpp
  scan/scan.hpp
  scan/scan.cpp
  scan/token.hpp
  scan/token.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/glsl/${source})
endforeach()

endfunction(configure_glsl)

configure_glsl()