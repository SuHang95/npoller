set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/coroutine_pool
        set (LOGGER_DIR ${PROJECT_SOURCE_DIR}/src/sulog))

add_executable(coroutine_test
        coroutine_test.cpp
        )

target_link_libraries(coroutine_test ${CMAKE_THREAD_LIBS_INIT}
        /usr/lib/libtbb.so)