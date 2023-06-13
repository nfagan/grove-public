function(configure_serialize)

set(sources
        ast.hpp
        ast.cpp
        token.hpp
        token.cpp
        scan.hpp
        scan.cpp
        error.hpp
        error.cpp
        parse.hpp
        parse.cpp
        parse_ast.hpp
        parse_ast.cpp
        StringRegistry.hpp
        common.hpp
        text.hpp
        text.cpp
        declare.hpp
        declare.cpp
        resolve.hpp
        resolve.cpp
        visitor.hpp
        collect.hpp
        collect.cpp
        pipeline.hpp
        pipeline.cpp
        transform.hpp
        transform.cpp
        utility.hpp
        utility.cpp
)

foreach(source ${sources})
    target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/serialize/${source})
endforeach()

endfunction(configure_serialize)

configure_serialize()