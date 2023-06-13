project(test_growth_sys)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
        grove
        )

target_sources(${PROJECT_NAME} PRIVATE
        main.cpp
        ../../growth.cpp
        ../../growth_system.cpp

        ../../PointOctree.hpp
        ../../attraction_points.hpp
        ../../attraction_points.cpp
        ../../components.hpp
        ../../components.cpp
        ../../environment_input.hpp
        ../../environment_input.cpp
        ../../environment_sample.hpp
        ../../environment_sample.cpp
        ../../bud_fate.hpp
        ../../bud_fate.cpp
        ../../sync_growth.hpp
        ../../sync_growth.cpp
        ../../GrowthSystem.hpp
        ../../GrowthSystem.cpp
        ../../utility.hpp
        ../../utility.cpp
        ../../render.hpp
        ../../render.cpp
        ../../serialize.hpp
        ../../serialize.cpp
        )

target_compile_definitions(${PROJECT_NAME} PUBLIC
    GROVE_PLAYGROUND_RES_DIR="${PROJECT_SOURCE_DIR}/../../../../playground/res"
)

configure_compiler_flags(${PROJECT_NAME})