include_directories(${PROJECT_SOURCE_DIR}/src)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(iobuffer)
add_subdirectory(io)
add_subdirectory(spinlock)
add_subdirectory(logger)
add_subdirectory(locally_queue)

