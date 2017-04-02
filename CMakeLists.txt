cmake_minimum_required(VERSION 3.3)
project(new)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -std=c++11 -fopenmp")

set(SOURCE_FILES
    draw.cpp
    draw.h
    main.cpp
	fftwpp/fftw++.cc)

include_directories(.
                    fftwpp)

add_executable(new ${SOURCE_FILES})

LINK_DIRECTORIES(/usr/lib/x86_64-linux-gnu/)
TARGET_LINK_LIBRARIES(main fftw3_omp fftw3 glfw GLEW GLU GL pthread)

