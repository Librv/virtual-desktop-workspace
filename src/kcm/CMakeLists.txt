find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config KCMUtils Kirigami I18n Service)

# Pure part: the editing models (the vdwrc writer lives in vdwcore). No D-Bus,
# no KCM classes.
add_library(vdwkcmcore STATIC workspacemodel.cpp)
target_include_directories(vdwkcmcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vdwkcmcore PUBLIC vdwcore Qt6::Core KF6::ConfigCore)
set_target_properties(vdwkcmcore PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(vdwkcmcore PROPERTIES AUTOMOC ON)

kcmutils_add_qml_kcm(kcm_vdw SOURCES
    vdwkcm.cpp
    vdwkcm.h
    appmodel.cpp
    appmodel.h
)
target_link_libraries(kcm_vdw PRIVATE
    vdwkcmcore
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
    KF6::KCMUtilsQuick KF6::I18n KF6::Service KF6::ConfigCore
)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test)
    ecm_add_test(workspacemodeltest.cpp
        TEST_NAME workspacemodeltest
        LINK_LIBRARIES Qt6::Core Qt6::Test vdwkcmcore
    )
endif()

if(BUILD_TESTING)
    # The tests use plain string literals for brevity.
    foreach(t workspacemodeltest)
        target_compile_options(${t} PRIVATE -UQT_NO_CAST_FROM_ASCII -UQT_NO_CAST_TO_ASCII)
    endforeach()
endif()
