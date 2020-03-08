set(MODULE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/io)
set(BUFFER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/iobuffer)
set(SPINLOCK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/spinlock)
set(LOGGER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/sulog)

add_executable(iotest
	iotest.cpp
	${MODULE_SOURCE_DIR}/event_processor.cpp
	${MODULE_SOURCE_DIR}/io.cpp
	${MODULE_SOURCE_DIR}/io_factory.cpp
	${BUFFER_SOURCE_DIR}/io_buffer.cpp
	${SPINLOCK_SOURCE_DIR}/spin_lock.cpp
	${LOGGER_SOURCE_DIR}/simple_logger.cpp)



target_link_libraries(iotest ${CMAKE_THREAD_LIBS_INIT}
		/usr/lib/libtbb.so)






