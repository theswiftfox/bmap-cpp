cmake_minimum_required(VERSION 3.20)

project(bmapcpp-cmd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(tinyxml2 REQUIRED)

set(SRC
    src/main.cpp
)

add_executable(${PROJECT_NAME}
    ${SRC}
)

target_link_libraries(${PROJECT_NAME}
    tinyxml2
)