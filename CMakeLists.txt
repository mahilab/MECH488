# declare minimum CMake version
cmake_minimum_required(VERSION 3.13)
# declare CMake project
project(MECH488 VERSION 0.1.0 LANGUAGES CXX)

# fetch mahi:com from GitHub (needed by both Windows and myRIO applications)
include(FetchContent) 
FetchContent_Declare(mahi-com GIT_REPOSITORY https://github.com/mahilab/mahi-com.git) 
FetchContent_MakeAvailable(mahi-com)

if (WIN32)

    # fetch mahi::gui from GitHub
    include(FetchContent) 
    FetchContent_Declare(mahi-gui GIT_REPOSITORY https://github.com/mahilab/mahi-gui.git) 
    FetchContent_MakeAvailable(mahi-gui)

    # Pendulum GUI application
    add_executable(pendulum-gui src/windows/pendulum-gui.cpp src/windows/PendulumGui.hpp src/windows/PendulumGui.cpp)
    target_link_libraries(pendulum-gui mahi::com mahi::gui)
    target_include_directories(pendulum-gui PUBLIC src/common)

elseif(NI_LRT)

    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    # fetch mahi::daq from GitHub
    include(FetchContent) 
    FetchContent_Declare(mahi-daq GIT_REPOSITORY https://github.com/mahilab/mahi-daq.git) 
    FetchContent_MakeAvailable(mahi-daq)
    # fetch mahi::robo from GitHub
    include(FetchContent) 
    FetchContent_Declare(mahi-robo GIT_REPOSITORY https://github.com/mahilab/mahi-robo.git) 
    FetchContent_MakeAvailable(mahi-robo)

    # basic myRIO application
    add_executable(basic src/myrio/basic.cpp)
    target_link_libraries(basic mahi::daq mahi::robo mahi::com)
    target_include_directories(basic PUBLIC src/common)

    # Pendulum application
    add_executable(pendulum src/myrio/pendulum.cpp)
    target_link_libraries(pendulum mahi::daq mahi::robo mahi::com)
    target_include_directories(pendulum PUBLIC src/common)

endif()