cmake_minimum_required(VERSION 3.8)
project(png2br)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_RELEASE -O2)
endif(CMAKE_COMPILER_IS_GNUCXX)

find_package(PkgConfig)

pkg_check_modules(PNG libpng)
pkg_check_modules(AVCODEC libavcodec)
pkg_check_modules(AVFORMAT libavformat)
pkg_check_modules(AVUTIL libavutil)
pkg_check_modules(SWSCALE libswscale)

link_directories(${PNG_LIBRARY_DIRS} ${AVCODEC_LIBRARY_DIRS} ${AVUTIL_LIBRARY_DIRS} ${SWSCALE_LIBRARY_DIRS})
include_directories(${PNG_INCLUDE_DIRS} ${AVCODEC_LIBRARY_DIRS} ${AVUTIL_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})

add_executable(png2br main.cpp image.cpp image.h util.h)
add_executable(avtest image.cpp image.h util.h avtest.cpp videodecoder.cpp videodecoder.h)

target_link_libraries(png2br stdc++ stdc++fs ${PNG_LIBRARIES})
target_link_libraries(avtest stdc++ stdc++fs pthread  ${PNG_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWSCALE_LIBRARIES})