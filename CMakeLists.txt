cmake_minimum_required(VERSION 3.13)

# create your C++ project
project(Pendulum VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent) 
FetchContent_Declare(mahi-daq GIT_REPOSITORY https://github.com/mahilab/mahi-daq.git) 
FetchContent_MakeAvailable(mahi-daq)

add_executable(pendulum src/main.cpp)