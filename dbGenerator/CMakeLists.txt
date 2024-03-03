cmake_minimum_required(VERSION 3.22)
project(${COMPONENT_DB_GENERATOR})

set(SRC
    src/main.cpp
    src/Schema.cpp
)

add_executable(${PROJECT_NAME}
    ${SRC}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)