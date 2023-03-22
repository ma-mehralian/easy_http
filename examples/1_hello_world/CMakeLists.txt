cmake_minimum_required(VERSION 3.11)

add_executable(hello_world main.cpp)
target_include_directories(hello_world PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_directories(hello_world PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)
target_link_libraries(hello_world ${PROJECT_NAME})
