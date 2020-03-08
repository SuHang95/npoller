set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/io)
set(BUFFER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/iobuffer)
set(SPINLOCK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/spinlock)
set(LOGGER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/sulog)

INCLUDE_DIRECTORIES(
        .
)

add_executable(image_recv
        recv.cpp
        recvtask.cpp
        ${MODULE_SOURCE_DIR}/event_processor.cpp
        ${MODULE_SOURCE_DIR}/tcp.cpp
        ${BUFFER_SOURCE_DIR}/io_buffer.cpp
        ${SPINLOCK_SOURCE_DIR}/spin_lock.cpp
        ${LOGGER_SOURCE_DIR}/simple_logger.cpp)

target_link_libraries(image_recv ${CMAKE_THREAD_LIBS_INIT})
