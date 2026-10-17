add_executable(vdwd main.cpp manager.cpp)
target_link_libraries(vdwd PRIVATE vdwcore Qt6::Gui Qt6::DBus KF6::DBusAddons)
install(TARGETS vdwd DESTINATION ${KDE_INSTALL_BINDIR})

configure_file(io.github.librv.vdw.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.librv.vdw.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
