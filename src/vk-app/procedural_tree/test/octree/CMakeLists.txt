project(test_octree)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
        grove
        )

target_sources(${PROJECT_NAME} PRIVATE
        main.cpp
        ../../attraction_points.cpp
        )

configure_compiler_flags(${PROJECT_NAME})