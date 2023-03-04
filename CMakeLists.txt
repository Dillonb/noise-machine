cmake_minimum_required(VERSION 3.25)
project(noise_machine C)

set(CMAKE_C_STANDARD 23)

include_directories(contrib)

add_executable(noise_machine noise.c)
target_link_libraries(noise_machine portaudio)
