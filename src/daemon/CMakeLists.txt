find_package(KF6Service ${KF_MIN_VERSION} REQUIRED)
find_package(KF6GlobalAccel ${KF_MIN_VERSION} REQUIRED)

add_executable(vdwd main.cpp manager.cpp kwinscripts.cpp launcher.cpp uiclient.cpp shortcuts.cpp gesturecontroller.cpp)
qt_add_resources(vdwd kwin PREFIX /kwin BASE kwin FILES kwin/placer.js kwin/empty.js kwin/area.js kwin/cursor.js kwin/list.js)
target_link_libraries(vdwd PRIVATE vdwcore vdwgesture Qt6::Gui Qt6::DBus KF6::DBusAddons KF6::CoreAddons KF6::Service KF6::GlobalAccel)
install(TARGETS vdwd DESTINATION ${KDE_INSTALL_BINDIR})

configure_file(io.github.librv.vdw.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

install(FILES io.github.librv.vdw.autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR} RENAME io.github.librv.vdw.desktop)
