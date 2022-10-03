cmake_minimum_required(VERSION 3.0.2)
project(dynamic_limits)

find_package(Pylon QUIET)

if (NOT ${Pylon_FOUND})
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindPylon.cmake")
endif()

include_directories(${Pylon_INCLUDE_DIRS})

add_executable(
    ${PROJECT_NAME}
     src/dynamic_limits.cpp
)

target_link_libraries(
	${PROJECT_NAME}
	${Pylon_LIBRARIES}
	)

### tests
enable_testing()
add_test(NAME TestCamEmuDumpFeatures
	 COMMAND ${PROJECT_NAME}
	 )
