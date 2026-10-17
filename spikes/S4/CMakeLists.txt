# Throwaway S4 spike: launch apps through KIO::ApplicationLauncherJob.
# Build: cmake -S spikes/S4 -B spikes/S4/build -G Ninja && cmake --build spikes/S4/build
cmake_minimum_required(VERSION 3.22)
project(s4launch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(KF6KIO REQUIRED)
find_package(KF6Service REQUIRED)
find_package(KF6WindowSystem REQUIRED)

add_executable(s4launch s4launch.cpp)
target_link_libraries(s4launch PRIVATE Qt6::Gui KF6::KIOGui KF6::Service KF6::WindowSystem)
