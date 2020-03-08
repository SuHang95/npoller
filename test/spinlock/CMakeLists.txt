set(SPIN_LOCK_DIR ${PROJECT_SOURCE_DIR}/src/spinlock)

add_executable(spin_lock_test
        spin_lock_test.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp)
add_executable(performance_test
        performance_test.cpp
        ${SPIN_LOCK_DIR}/spin_lock.cpp)

target_link_libraries(spin_lock_test ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(performance_test ${CMAKE_THREAD_LIBS_INIT})