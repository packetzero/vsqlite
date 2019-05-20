CMAKE_MINIMUM_REQUIRED(VERSION 2.7)

set (PROJECT_NAME vsqlite)
PROJECT(${PROJECT_NAME})

file(GLOB HDRS  "*.h*" "../thirdparty/sqlite/sqlite3.h")
file(GLOB SRCS  "*.cpp" "../thirdparty/sqlite/sqlite3.c")

include_directories(../thirdparty/sqlite/  )

add_library (${PROJECT_NAME} ${SRCS} ${HDRS})

install(TARGETS vsqlite ARCHIVE DESTINATION lib)
