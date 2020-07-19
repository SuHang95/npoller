
add_executable(locally_queue_test
        locally_queue_test.cpp)


target_link_libraries(locally_queue_test ${CMAKE_THREAD_LIBS_INIT}
        /usr/lib/libtbb.so)