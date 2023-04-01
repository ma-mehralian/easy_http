cmake_minimum_required(VERSION 3.11)

add_executable(1_simple_server main.cpp)

target_include_directories(1_simple_server PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(1_simple_server PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(1_simple_server ${PROJECT_NAME})

#target_include_directories(1_simple_server PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
#target_link_directories(1_simple_server PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)
#target_link_libraries(1_simple_server ${PROJECT_NAME})
