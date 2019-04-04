cmake_minimum_required(VERSION 3.13)

# create your C++ project
project(MyProject VERSION 0.1.0 LANGUAGES CXX)

# find MEL::MEL and all available MEL::xxx modules
find_package(MEL REQUIRED)

# add your include directories
include_directories("include")

# create an app from your source files
add_executable(app
    "src/main.cpp"
)

# link your app to MEL libaries
target_link_libraries(app 
    MEL::MEL   # common MEL classes available on all systems
    MEL::myrio # needed if using MEL NI myRIO classes
)
