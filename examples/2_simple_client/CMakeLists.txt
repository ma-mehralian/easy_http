cmake_minimum_required(VERSION 3.11)

add_executable(2_simple_client main.cpp)

target_include_directories(2_simple_client PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(2_simple_client PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(2_simple_client ${PROJECT_NAME})