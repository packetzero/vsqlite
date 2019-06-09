CMAKE_MINIMUM_REQUIRED(VERSION 2.7)

set (PROJECT_NAME table_example)
PROJECT(${PROJECT_NAME})

file(GLOB HDRS "../*.h*" )
file(GLOB SRCS "*.cpp")

include_directories(../include )

add_executable (${PROJECT_NAME} ${SRCS} ${HDRS})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} vsqlite )
TARGET_LINK_LIBRARIES(${PROJECT_NAME} pthread dl )
