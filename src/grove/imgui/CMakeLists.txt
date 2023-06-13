function(configure_imgui)

set(sources
  IMGUIWrapper.hpp
  IMGUIWrapper.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/imgui/${source})
endforeach()

endfunction(configure_imgui)

configure_imgui()