cmake_minimum_required(VERSION 3.11)

add_executable(server_controller main.cpp)

target_include_directories(server_controller PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(server_controller PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(server_controller ${PROJECT_NAME})
set_target_properties(server_controller PROPERTIES FOLDER "examples" )