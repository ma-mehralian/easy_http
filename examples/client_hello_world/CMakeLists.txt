set(EXAMPLE_NAME client_hello_world)
add_executable(${EXAMPLE_NAME} main.cpp)
target_include_directories(	
	${EXAMPLE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(	
	${EXAMPLE_NAME} PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(	
	${EXAMPLE_NAME} ${PROJECT_NAME})

if (MSVC AND USE_MLD)
    target_compile_definitions(${EXAMPLE_NAME} PUBLIC USE_MLD)
	find_package(vld REQUIRED)
	target_link_libraries(${EXAMPLE_NAME} vld)
endif()

set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER "examples" )