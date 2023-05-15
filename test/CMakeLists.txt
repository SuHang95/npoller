include_directories(${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(iobuffer)
add_subdirectory(io)
add_subdirectory(spinlock)
add_subdirectory(logger)
add_subdirectory(coroutine)
##add_subdirectory(lockfree)


if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-D _DEBUG)
endif(CMAKE_COMPILER_IS_GNUCXX)