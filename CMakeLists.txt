cmake_minimum_required(VERSION 3.3)
project(main)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++17 -D_REENTRANT -DLINUX")

set(SOURCE_FILES
    src/main.cpp
    src/Window.cpp
    src/ShaderConfig.cpp
    src/ShaderPrograms.cpp
    src/Renderer.cpp
    src/AudioStreams/LinuxAudioStream.cpp
    src/AudioStreams/WavAudioStream.cpp
)

include_directories(
    .
    src
    libs/ffts/include/
    libs/rapidjson/include/
    libs/SimpleFileWatcher/include/
)

add_subdirectory(libs/ffts)
add_subdirectory(libs/SimpleFileWatcher)

add_executable(main ${SOURCE_FILES})
add_dependencies(main ffts)
add_dependencies(main SimpleFileWatcher)

TARGET_LINK_LIBRARIES(main glfw GLEW GLU GL pulse-simple pulse pthread ${CMAKE_SOURCE_DIR}/build/libs/ffts/libffts.a ${CMAKE_SOURCE_DIR}/build/libs/SimpleFileWatcher/libSimpleFileWatcher.a stdc++fs)

