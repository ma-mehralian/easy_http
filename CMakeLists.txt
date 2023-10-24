cmake_minimum_required(VERSION 3.11)
project(easy_http CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property( GLOBAL PROPERTY USE_FOLDERS ON)

option(USE_SPDLOG   "set ON to use spdlog" OFF)
option(USE_JSON     "set ON to use nlohman json for json responses" OFF)
option(USE_MLD      "set ON to use memory leak detector" OFF)

set(PROJECT_MAJOR_VERSION 1)
set(PROJECT_MINOR_VERSION 0)
set(PROJECT_PATCH_VERSION 0)
set(PROJECT_VERSION
    ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION})

find_package(Libevent REQUIRED)
if (MSVC AND USE_MLD)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
endif()

set(PROJECT_HEADERS 
    include/${PROJECT_NAME}/server.h
    include/${PROJECT_NAME}/client.h
    include/${PROJECT_NAME}/route.h
    include/${PROJECT_NAME}/ev_request.h
    include/${PROJECT_NAME}/request_base.h
    include/${PROJECT_NAME}/request.h
    include/${PROJECT_NAME}/response.h
    include/${PROJECT_NAME}/controller.h
    include/${PROJECT_NAME}/middleware.h
)
add_library(${PROJECT_NAME} STATIC
    src/server.cpp
    src/client.cpp
    src/route.cpp
    src/ev_request.cpp
    src/request_base.cpp
    src/request.cpp
    src/response.cpp
    src/controller.cpp
    src/middleware.cpp
    src/utils.h
    src/utils.cpp
    ${PROJECT_HEADERS}
)
set_target_properties(${PROJECT_NAME} PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_HEADERS}")
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PRIVATE libevent::extra)
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# --- use spdlog
if(USE_SPDLOG)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SPDLOG)
    add_subdirectory(third_party/spdlog)
    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
endif(USE_SPDLOG)

# --- use nlohmann_json
if(USE_JSON)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_JSON)
    include(FetchContent)
    FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
    FetchContent_MakeAvailable(json)
    target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
endif(USE_JSON)

# --- installation
install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_BUILD_TYPE}/lib"
    ARCHIVE DESTINATION "${CMAKE_BUILD_TYPE}/lib"
    PUBLIC_HEADER DESTINATION "${CMAKE_BUILD_TYPE}/include/${PROJECT_NAME}"
)

# --- examples
add_subdirectory("examples/client_hello_world")
add_subdirectory("examples/server_hello_world")
add_subdirectory("examples/server_middleware")
add_subdirectory("examples/server_controller")
