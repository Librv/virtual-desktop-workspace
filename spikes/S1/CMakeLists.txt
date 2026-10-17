# S1 spike: KGlobalAccel tap/hold/confirm probe. Throwaway code.
#
#   cmake -S spikes/S1 -B spikes/S1/build -G Ninja
#   cmake --build spikes/S1/build
#
# s1probe:  registers Ctrl+Meta+D through KGlobalAccel and logs every
#           globalShortcutActiveChanged / triggered signal plus Ctrl/Meta/
#           Shift/Alt state from KModifierKeyInfo, with millisecond times.
# s1inject: (optional) sends key presses through KWin's org_kde_kwin_fake_input
#           protocol. Only for the NESTED session; needs KWayland.
cmake_minimum_required(VERSION 3.20)
project(vdw-s1-spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6GuiAddons REQUIRED)
find_package(KWayland QUIET)

add_executable(s1probe s1probe.cpp)
target_link_libraries(s1probe PRIVATE Qt6::Gui KF6::GlobalAccel KF6::GuiAddons)

if(KWayland_FOUND)
    add_executable(s1inject s1inject.cpp)
    target_link_libraries(s1inject PRIVATE Qt6::Gui Plasma::KWaylandClient)
else()
    message(STATUS "KWayland not found: s1inject (nested-session key injector) not built")
endif()
