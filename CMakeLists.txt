# declare minimum CMake version
cmake_minimum_required(VERSION 3.13)
# declare CMake project
project(MECH488 VERSION 0.1.0 LANGUAGES CXX)
# include FetchContent so we can automagically get code from GitHub
include(FetchContent) 

# fetch mahi:com from GitHub (needed by both Windows and myRIO applications)
FetchContent_Declare(mahi-com GIT_REPOSITORY https://github.com/mahilab/mahi-com.git) 
FetchContent_MakeAvailable(mahi-com)

if (WIN32)

    # fetch mahi::gui from GitHub
    FetchContent_Declare(mahi-gui GIT_REPOSITORY https://github.com/mahilab/mahi-gui.git) 
    FetchContent_MakeAvailable(mahi-gui)

    # Pendulum GUI application
    add_executable(pendulum-gui src/windows/pendulum-gui.cpp src/windows/PendulumGui.hpp src/windows/PendulumGui.cpp src/windows/icons/pendulum-gui.rc)
    target_link_libraries(pendulum-gui mahi::com mahi::gui)
    target_include_directories(pendulum-gui PUBLIC src/common)

elseif(NI_LRT)

    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    # fetch mahi::daq from GitHub
    FetchContent_Declare(mahi-daq GIT_REPOSITORY https://github.com/mahilab/mahi-daq.git) 
    FetchContent_MakeAvailable(mahi-daq)
    # fetch mahi::robo from GitHub
    FetchContent_Declare(mahi-robo GIT_REPOSITORY https://github.com/mahilab/mahi-robo.git) 
    FetchContent_MakeAvailable(mahi-robo)
    # fetch IIR filters from GitHub and exclude their demos from build
    FetchContent_Declare(iir GIT_REPOSITORY https://github.com/berndporr/iir1) 
    FetchContent_MakeAvailable(iir)
    set_target_properties(iirdemo ecg50hzfilt PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

    # basic myRIO application
    add_executable(myrio src/myrio/myrio.cpp)
    target_link_libraries(myrio mahi::daq mahi::robo mahi::com)
    target_include_directories(myrio PUBLIC src)

    # Pendulum application
    add_executable(pendulum src/myrio/pendulum.cpp src/myrio/IPendulum.hpp src/myrio/IPendulum.cpp)
    target_link_libraries(pendulum mahi::daq mahi::robo mahi::com iir::iir_static)
    target_include_directories(pendulum PUBLIC src/common)

endif()