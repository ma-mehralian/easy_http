cmake_minimum_required(VERSION 3.11)

add_executable(server_hello_world main.cpp)

target_include_directories(server_hello_world PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(server_hello_world PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(server_hello_world ${PROJECT_NAME})
set_target_properties(server_hello_world PROPERTIES FOLDER "examples" )