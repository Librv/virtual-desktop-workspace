# S2 spike: radial menu overlay on LayerShellQt. Throwaway code.
#
#   cmake -S spikes/S2 -B spikes/S2/build -G Ninja
#   cmake --build spikes/S2/build
#   ctest --test-dir spikes/S2/build --output-on-failure
cmake_minimum_required(VERSION 3.25)
project(vdw-s2-spike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick DBus Test)
find_package(LayerShellQt REQUIRED)
find_package(KF6GlobalAccel REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

enable_testing()

# Pure slice selection math and its test.
add_library(radialmath STATIC radialmath.cpp)
target_link_libraries(radialmath PUBLIC Qt6::Core)
target_include_directories(radialmath PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(radialmathtest radialmathtest.cpp)
target_link_libraries(radialmathtest radialmath Qt6::Test)
add_test(NAME radialmathtest COMMAND radialmathtest)

# The prototype overlay.
add_executable(s2-radial main.cpp)
qt_add_resources(s2-radial "qml" PREFIX "/" FILES Overlay.qml)
target_link_libraries(s2-radial radialmath Qt6::Gui Qt6::Quick Qt6::DBus LayerShellQt::Interface KF6::GlobalAccel)

# Test helpers for the nested session only.
# s2-fakeinput: org_kde_kwin_fake_input client (pointer motion, keys).
set(FAKE_XML ${CMAKE_CURRENT_SOURCE_DIR}/tools/fake-input.xml)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fake-input-client.h ${CMAKE_CURRENT_BINARY_DIR}/fake-input-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${FAKE_XML} ${CMAKE_CURRENT_BINARY_DIR}/fake-input-client.h
    COMMAND ${WAYLAND_SCANNER} private-code ${FAKE_XML} ${CMAKE_CURRENT_BINARY_DIR}/fake-input-protocol.c
    DEPENDS ${FAKE_XML})
add_executable(s2-fakeinput tools/fakeinput.c ${CMAKE_CURRENT_BINARY_DIR}/fake-input-protocol.c)
target_include_directories(s2-fakeinput PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(s2-fakeinput PkgConfig::WAYLAND_CLIENT)

# s2-shot: grabs the screen through org.kde.KWin.ScreenShot2.
add_executable(s2-shot tools/shot.cpp)
target_link_libraries(s2-shot Qt6::Gui Qt6::DBus)
