# Minimum version required
cmake_minimum_required(VERSION 3.10)

# Set the build version
set(BUILD_VERSION 1.2.32)

# Add definition of Jiminy version for C++ headers
add_definitions("-DJIMINY_VERSION=\"${BUILD_VERSION}\"")

# Project and library name
project(jiminy VERSION ${BUILD_VERSION})
set(LIBRARY_NAME ${PROJECT_NAME})

# Set build environment and standard dependencies
include(${CMAKE_SOURCE_DIR}/build_tools/cmake/base.cmake)
include(${CMAKE_SOURCE_DIR}/build_tools/cmake/doxygen.cmake)
include(${CMAKE_SOURCE_DIR}/build_tools/cmake/boostPythonDocstring.cmake)
include(${CMAKE_SOURCE_DIR}/build_tools/cmake/exportCmakeConfigFiles.cmake)
include(${CMAKE_SOURCE_DIR}/build_tools/cmake/buildPythonWheel.cmake)

# Set the compilation flags
if(NOT WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -ftemplate-backtrace-limit=0 ${WARN_FULL}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DNDEBUG -O3 ${WARN_FULL} -Wno-non-virtual-dtor -Wno-deprecated-declarations -Wno-unused-parameter -Wfatal-errors -Werror")
else(NOT WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /EHsc /bigobj -g /Wall")
    # It would be great to have the same quality standard for Windows but
    # system inlcude of dependencies is not working properly so far.
    # - Disabling warning 4820 generated by std time.h, which is included by std chrono
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /EHsc /bigobj -DNDEBUG /O2 /W2 /wd4820 /wd4244 /wd4005 /WX")
endif(NOT WIN32)

# Sub-projects
add_subdirectory(soup)
add_subdirectory(core)
if(BUILD_PYTHON_INTERFACE)
    add_subdirectory(python)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_TESTING)
    add_subdirectory(unit)
endif()

# Build documentation from Doxygen.
build_DOC()

# Install the source cmake files
file(GLOB_RECURSE SOURCE_CMAKE_FILES "${CMAKE_SOURCE_DIR}/build_tools/cmake/Find*.cmake")
install(FILES ${SOURCE_CMAKE_FILES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${LIBRARY_NAME}/cmake"
)
install(FILES ${CMAKE_SOURCE_DIR}/build_tools/cmake/base.cmake
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${LIBRARY_NAME}/cmake"
        RENAME ${LIBRARY_NAME}_base.cmake
)
install(FILES ${CMAKE_SOURCE_DIR}/build_tools/cmake/boostPythonDocstring.cmake
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${LIBRARY_NAME}/cmake"
)
