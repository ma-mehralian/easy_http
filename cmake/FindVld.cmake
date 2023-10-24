# ===================================================================================
#  The vld CMake configuration file
#
#  Usage from an external project:
#    In your CMakeLists.txt, add these lines:
#
#    FIND_PACKAGE(vld REQUIRED)
#    TARGET_LINK_LIBRARIES(MY_TARGET_NAME vld)
#
#    This file will define the following variables:
#      - VLD_FOUND                   : if the system found vld in ${VLD_DIR}.
#      - VLD_LIBRARIES               : The list of all imported targets for vld.
#      - VLD_INCLUDE_DIR             : The vld include directory
#
# ===================================================================================

if(NOT TARGET vld)
    find_path(VLD_INCLUDE_DIR
        NAMES vld.h
        PATHS ${VLD_DIR}/include 
        )

    find_library(VLD_LIBRARIES
        NAMES vld
        PATHS ${VLD_DIR}/lib/Win64
        )

    if (VLD_INCLUDE_DIR AND VLD_LIBRARIES)
        set(VLD_FOUND TRUE)
        message(STATUS "Found VLD: ${VLD_LIBRARIES}, ${VLD_INCLUDE_DIR}")
	    
        add_library(vld STATIC IMPORTED)
	    set_target_properties(vld PROPERTIES
	      INTERFACE_INCLUDE_DIRECTORIES ${VLD_INCLUDE_DIR}
	      IMPORTED_LOCATION ${VLD_LIBRARIES}
	    )
    else ()
        message(FATAL_ERROR "Could not find VLD in ${VLD_DIR}")
    endif ()

endif ()