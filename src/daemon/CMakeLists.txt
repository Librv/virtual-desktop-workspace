find_package(KF6KIO ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Service ${KF_MIN_VERSION} REQUIRED)

add_executable(vdwd main.cpp manager.cpp kwinscripts.cpp launcher.cpp)
qt_add_resources(vdwd kwin PREFIX /kwin BASE kwin FILES kwin/placer.js kwin/empty.js kwin/area.js)
target_link_libraries(vdwd PRIVATE vdwcore Qt6::Gui Qt6::DBus KF6::DBusAddons KF6::CoreAddons KF6::KIOGui KF6::Service)
install(TARGETS vdwd DESTINATION ${KDE_INSTALL_BINDIR})

configure_file(io.github.librv.vdw.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
