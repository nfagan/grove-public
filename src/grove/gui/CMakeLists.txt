function(configure_gui)

set(sources
        font.hpp
        font.cpp
        gui_layout.hpp
        gui_layout.cpp
        gui_layout_private.hpp
        gui_cursor.hpp
        gui_cursor.cpp
        gui_elements.hpp
        gui_elements.cpp
)

foreach(source ${sources})
    target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/grove/gui/${source})
endforeach()

endfunction(configure_gui)

configure_gui()