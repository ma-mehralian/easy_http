cmake_minimum_required(VERSION 3.11)

add_executable(3_server_controller main.cpp)

target_include_directories(3_server_controller PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(3_server_controller PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(3_server_controller ${PROJECT_NAME})