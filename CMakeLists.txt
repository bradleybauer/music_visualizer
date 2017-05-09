cmake_minimum_required(VERSION 3.3)
project(main)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -D_REENTRANT -fopenmp")

set(SOURCE_FILES
	draw.cpp
	draw.h
	main.cpp
	pulse.cpp
	fftwpp/fftw++.cc
)

include_directories(
	.
	fftwpp
)

add_executable(main ${SOURCE_FILES})

LINK_DIRECTORIES(/usr/lib/x86_64-linux-gnu/)
TARGET_LINK_LIBRARIES(main fftw3_omp fftw3 glfw GLEW GLU GL pulse-simple pulse pthread)

