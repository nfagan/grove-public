function(configure_ls)

set(sources
    common.hpp
    common.cpp
    compile.hpp
    compile.cpp
    derive.hpp
    derive.cpp
    derive_branched.hpp
    derive_branched.cpp
    dump.hpp
    dump.cpp
    interpret.hpp
    interpret.cpp
    ls.hpp
    match.hpp
    match.cpp
    parse.hpp
    parse.cpp
    resolve.hpp
    resolve.cpp
    scan.hpp
    scan.cpp
    StringRegistry.hpp
)

foreach(source ${sources})
    target_sources(${PROJECT_NAME} PRIVATE ${GROVE_MODULE_SOURCE_DIR}/ls/${source})
endforeach()

endfunction(configure_ls)

configure_ls()