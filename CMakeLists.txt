CMAKE_MINIMUM_REQUIRED(VERSION 2.7)

set (PROJECT_NAME vsqlite-project)
PROJECT(${PROJECT_NAME})

set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF) # want c++11 not gnu++11

set(CMAKE_CXX_RELEASE_FLAGS "-DNDEBUG=1")

# on MacOS with brew:
include_directories(/usr/local/include)
include_directories(${CMAKE_SOURCE_DIR}/deps/dyno/include)

add_subdirectory(src)
add_subdirectory(example)

if($ENV{MAKE_TESTS})
  set(GTEST_DIR $ENV{GTEST_DIR})
  add_subdirectory(tests)
endif()

install(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h" )
