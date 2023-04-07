
add_executable(mpsc_queue_test
        mpsc_queue_test.cpp)


target_link_libraries(mpsc_queue_test ${CMAKE_THREAD_LIBS_INIT}
        /usr/lib/libtbb.so)