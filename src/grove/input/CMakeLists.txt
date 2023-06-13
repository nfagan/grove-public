function(configure_input)

set(sources
  Controller.hpp
  Controller.cpp
  controllers/KeyboardMouse.hpp
  controllers/KeyboardMouse.cpp
  Directional.hpp
  Directional.cpp
  Keyboard.hpp
  Keyboard.cpp
  KeyTrigger.hpp
  KeyTrigger.cpp
  Mouse.hpp
  MouseButtonTrigger.hpp
  MouseButtonTrigger.cpp
)

foreach(source ${sources})
  target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/input/${source})
endforeach()

endfunction(configure_input)

configure_input()