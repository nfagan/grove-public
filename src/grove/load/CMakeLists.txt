function(configure_load)

set(sources
  array.hpp
  array.cpp
  image.hpp
  image.cpp
  obj.hpp
  obj.cpp
  wav.hpp
  wav.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/load/${source})
endforeach()

endfunction(configure_load)

configure_load()