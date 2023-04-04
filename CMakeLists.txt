# the whole CMakeLists.txt for project ttiger
PROJECT(npoller)

CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

# include directories
INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}/src

  /usr/local/include
  /usr/include
)

# lib directories
LINK_DIRECTORIES(
  ${PROJECT_BINARY_DIR}/lib

  /usr/local/lib
  /use/lib
)


if(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_COMPILER /usr/bin/g++)
    add_compile_options(-g)
    add_compile_options(-D _DEBUG)
endif(CMAKE_COMPILER_IS_GNUCXX)
find_package(Threads)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

ADD_SUBDIRECTORY(test)
#ADD_SUBDIRECTORY(example)

