# Radial menu component (T4a): pure slice math, the overlay window and its QML.
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LayerShellQt REQUIRED)

set(CMAKE_AUTOMOC ON)

add_library(vdwmenu STATIC radialmath.cpp radialmenu.cpp toast.cpp)
qt_add_resources(vdwmenu "vdwmenu_qml"
    PREFIX "/vdwmenu"
    FILES RadialMenu.qml Toast.qml)
target_include_directories(vdwmenu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vdwmenu PUBLIC Qt6::Gui Qt6::Quick LayerShellQt::Interface)

# For nested-session checks only, not installed.
add_executable(vdw-menu-demo demo.cpp)
target_link_libraries(vdw-menu-demo PRIVATE vdwmenu)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test)
    ecm_add_test(radialmathtest.cpp
        TEST_NAME radialmathtest
        LINK_LIBRARIES vdwmenu Qt6::Test)
endif()
