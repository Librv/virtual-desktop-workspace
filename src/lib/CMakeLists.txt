add_library(vdwcore STATIC config.cpp workspace.cpp layout.cpp matcher.cpp execargs.cpp shortcutkeys.cpp)
target_include_directories(vdwcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vdwcore PUBLIC Qt6::Core Qt6::Gui KF6::ConfigCore KF6::CoreAddons)
