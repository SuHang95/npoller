set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/io)
set(BUFFER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/iobuffer)
set(SPINLOCK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/spinlock)
set(LOGGER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/sulog)

add_executable(iotest
        iotest.cpp
        ${MODULE_SOURCE_DIR}/event_processor.cpp
        ${MODULE_SOURCE_DIR}/io.cpp
        ${MODULE_SOURCE_DIR}/io_factory.cpp
        ${MODULE_SOURCE_DIR}/tcp.cpp
        ${BUFFER_SOURCE_DIR}/io_buffer.cpp
        ${SPINLOCK_SOURCE_DIR}/spin_lock.cpp
        ${LOGGER_SOURCE_DIR}/simple_logger.cpp
        ${LOGGER_SOURCE_DIR}/logger.cpp)

add_executable(tcp_test
        tcp_test.cpp
        connection_pool.cpp
        ${MODULE_SOURCE_DIR}/event_processor.cpp
        ${MODULE_SOURCE_DIR}/io.cpp
        ${MODULE_SOURCE_DIR}/io_factory.cpp
        ${MODULE_SOURCE_DIR}/tcp.cpp
        ${MODULE_SOURCE_DIR}/tcp_acceptor.cpp
        ${BUFFER_SOURCE_DIR}/io_buffer.cpp
        ${SPINLOCK_SOURCE_DIR}/spin_lock.cpp
        ${LOGGER_SOURCE_DIR}/simple_logger.cpp
        ${LOGGER_SOURCE_DIR}/logger.cpp
        ${PROJECT_SOURCE_DIR}/test/timer.cpp)



target_link_libraries(iotest ${CMAKE_THREAD_LIBS_INIT}
        /usr/lib/libtbb.so)

target_link_libraries(tcp_test ${CMAKE_THREAD_LIBS_INIT}
        /usr/lib/libtbb.so)





