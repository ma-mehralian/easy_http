cmake_minimum_required(VERSION 3.11)

add_executable(server_middleware main.cpp)

target_include_directories(server_middleware PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(server_middleware PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(server_middleware ${PROJECT_NAME})
set_target_properties(server_middleware PROPERTIES FOLDER "examples" )