set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/sulog)

#add_executable(log_test
#        log_test.cpp
#        ${MODULE_SOURCE_DIR}/base_logger.cpp)
add_executable(log_test_concurrency
        log_test_concurrency.cpp
        ${MODULE_SOURCE_DIR}/simple_logger.cpp
        ${MODULE_SOURCE_DIR}/logger.cpp
        ${PROJECT_SOURCE_DIR}/src/spinlock/spin_lock.cpp)
add_executable(baselogger_test
        ${MODULE_SOURCE_DIR}/simple_logger.cpp
        ${MODULE_SOURCE_DIR}/logger.cpp
        baselogger_test.cpp)

target_link_libraries(log_test_concurrency ${CMAKE_THREAD_LIBS_INIT})