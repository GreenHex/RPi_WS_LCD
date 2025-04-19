# CMakeLists.txt
# 13-Apr-2025
# 2025 (c) MVK
#
# > cmake -H. -B_builds 
# > cmake --build _builds
#
# > readelf -d EXE_PATH | grep 'NEEDED'
# > objdump -p LIB_PATH | grep 'SONAME'
# > ldd EXE_PATH
# > patchelf --set-soname NEW_SONAME LIB_PATH
# > patchelf --replace-needed OLD_NAME NEW_NAME EXE_PATH
# > LD_DEBUG=libs EXE_PATH


cmake_minimum_required(VERSION 3.10)
project(LCD
		VERSION 1.0
		DESCRIPTION "SPI Display"
    	LANGUAGES C)

add_compile_definitions(PROJECT_VERSION=${PROJECT_VERSION})
add_compile_definitions(CMAKE_PROJECT_VERSION=${CMAKE_PROJECT_VERSION})

add_executable(LCD
	./lib/LCD/LCD_1in3.c
	./lib/Config/DEV_Config.c
	./lib/Fonts/font8.c
	./lib/Fonts/font12.c
	./lib/Fonts/font16.c
	./lib/Fonts/font20.c
	./lib/Fonts/font24.c
	./lib/Fonts/font48.c
	./lib/Fonts/font50.c
	./lib/GUI/GUI_BMP.c
	./lib/GUI/GUI_Paint.c
	./lib/LCD/LCD_1in3.c
	./src/image.c
	./src/init.c
	./src/utils.c
	./src/i2c_utils.c
	./src/main.c
	)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

add_compile_definitions(USE_DEV_LIB)
add_compile_definitions(LG_SET_INPUT)
# add_compile_definitions(USE_DEBUG)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})

target_include_directories(LCD PRIVATE ./examples)
target_include_directories(LCD PRIVATE ./lib)
target_include_directories(LCD PRIVATE ./lib/Config)
target_include_directories(LCD PRIVATE ./lib/Fonts)
target_include_directories(LCD PRIVATE ./lib/GUI)
target_include_directories(LCD PRIVATE ./lib/LCD)
target_include_directories(LCD PRIVATE ./src)

find_library(LIB_GPIO lgpio)
message(STATUS "LIB_GPIO: " ${LIB_GPIO})
target_link_libraries(LCD PUBLIC ${LIB_GPIO})

find_library(LIB_SD systemd)
message(STATUS "LIB_SD: " ${LIB_SD})
target_link_libraries(LCD PUBLIC ${LIB_SD})
