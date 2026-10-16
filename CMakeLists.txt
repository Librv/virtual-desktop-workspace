cmake_minimum_required(VERSION 3.25)

project(virtual-desktop-workspace VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.10.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(KDE_COMPILERSETTINGS_LEVEL "6.0")

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMAddTests)
include(FeatureSummary)

# Only what is needed so far. Later tasks add their own components here
# (e.g. KF6 GlobalAccel, KIO, Config, DBusAddons; LayerShellQt).
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Test)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

# `cmake --build --preset dev --target clang-format` formats all C++ sources.
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/autotests/*.cpp ${CMAKE_SOURCE_DIR}/autotests/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
