project(poly88)

cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# apt-get install libsdl2-dev
# apt-get install libsdl2-image-dev
#
# optional:
# apt-get install libsdl2-doc
#

if("${CMAKE_BUILD_TYPE}" MATCHES "")
	set(CMAKE_BUILD_TYPE "Debug")
	message("Configuring for '${CMAKE_BUILD_TYPE}'")
	message("Use cmake -DCMAKE_BUILD_TYPE=Release to build for release.")
endif()

add_executable(${PROJECT_NAME} emulator.cpp poly88.cpp memory.cpp i8080.cpp devices.cpp poly88_devices.cpp vdi_font.cpp sdl.cpp thirdparty/z80d.cpp)
add_executable(tape_byte tape_byte.cpp audio.cpp)
add_executable(tape_poly tape_poly.cpp audio.cpp)

# see http://stackoverflow.com/questions/23850472/how-to-use-sdl2-and-sdl-image-with-cmake
INCLUDE(FindPkgConfig)

PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
PKG_SEARCH_MODULE(SDL2IMAGE REQUIRED SDL2_image>=2.0.0)

INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIRS} ${SDL2IMAGE_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(thirdparty)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2IMAGE_LIBRARIES})

add_custom_target(
	cleanall
	COMMAND make clean
	COMMAND rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake
)
