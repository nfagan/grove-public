project(test_worley)

add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} grove)
target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
    ../worley.cpp
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    GROVE_PLAYGROUND_OUT_DIR="${PROJECT_SOURCE_DIR}/../../../../playground/res/test/noise"
)

configure_compiler_flags(${PROJECT_NAME})