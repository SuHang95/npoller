set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/iobuffer)
set(SPIN_LOCK_DIR ${PROJECT_SOURCE_DIR}/src/spinlock)
set(LOGGER_DIR ${PROJECT_SOURCE_DIR}/src/sulog)

find_package(Boost REQUIRED COMPONENTS system thread atomic)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(buffer_test
        buffer_test.cpp
        ${MODULE_SOURCE_DIR}/io_buffer.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp
        ${LOGGER_DIR}/simple_logger.cpp)

add_executable(iobuff_selftest
        ${MODULE_SOURCE_DIR}/io_buffer.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp
        iobuff_selftest.cpp
        ${LOGGER_DIR}/simple_logger.cpp)
add_executable(iobuff_selftest1
        ${MODULE_SOURCE_DIR}/io_buffer.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp
        iobuff_selftest1.cpp
        ${LOGGER_DIR}/simple_logger.cpp)
add_executable(iobuff_othertest
        ${MODULE_SOURCE_DIR}/io_buffer.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp
        iobuff_othertest.cpp
        ${LOGGER_DIR}/simple_logger.cpp)

#add_executable(concurrent_buffer_test
#        ${MODULE_SOURCE_DIR}/concurrent_buffer.cpp
#        concurrent_buffer_test.cpp)



#target_link_libraries(concurrent_buffer_test ${CMAKE_THREAD_LIBS_INIT}
#        ${Boost_LIBRARIES})
target_link_libraries(buffer_test ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES})
target_link_libraries(iobuff_selftest ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES})
target_link_libraries(iobuff_selftest1 ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES})
target_link_libraries(iobuff_othertest ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES})






