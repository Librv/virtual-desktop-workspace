add_executable(vdwctl main.cpp)
target_link_libraries(vdwctl PRIVATE Qt6::DBus)
target_include_directories(vdwctl PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
install(TARGETS vdwctl DESTINATION ${KDE_INSTALL_BINDIR})
