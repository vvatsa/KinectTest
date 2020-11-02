cmake_minimum_required(VERSION 3.17)
project(KinectTest)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package( OpenCV REQUIRED )
set(CMAKE_CXX_STANDARD 14)
FIND_LIBRARY(freenect2_LIBRARY freenect2
        PATHS /home/wish/freenect2/lib
        NO_DEFAULT_PATH
        )
SET(freenect2_LIBRARIES ${freenect2_LIBRARY} pthread)
FIND_PATH(freenect2_INCLUDE_DIR libfreenect2/libfreenect2.hpp
        PATHS /home/wish/freenect2/include
        NO_DEFAULT_PATH
        )
SET(freenect2_INCLUDE_DIRS ${freenect2_INCLUDE_DIR})

set(INCLUDE_DIRECTORIES "/home/wish/freenect2/include")
include_directories(${freenect2_INCLUDE_DIR})
include_directories( ${OpenCV_INCLUDE_DIRS} )
add_library(libfreenect2 SHARED IMPORTED)
set_property(TARGET libfreenect2 PROPERTY IMPORTED_LOCATION "/home/wish/freenect2/lib/libfreenect2.so")

add_executable(KinectTest main.cpp)
target_link_libraries(KinectTest ${freenect2_LIBRARIES} ${OpenCV_LIBS} )