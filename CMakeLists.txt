cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIGURATION>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIGURATION>)

if("${CMAKE_BUILD_TYPE}" MATCHES "")
	set(CMAKE_BUILD_TYPE "Debug")
	message("Configuring for '${CMAKE_BUILD_TYPE}'")
	message("Use cmake -DCMAKE_BUILD_TYPE=Release to build for release.")
endif()


project(poly88)

# Qt Stuff

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
	main.cpp
	poly88.cpp
	memory.cpp
	devices.cpp
	i8080.cpp
	z80d.cpp
	poly88_devices.cpp
	MediaPicker.cpp
	Keyboard.cpp
	TieredMemory.cpp
)

# Link it all up
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

add_custom_target(
	cleanall
	COMMAND make clean
	COMMAND rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake
)
