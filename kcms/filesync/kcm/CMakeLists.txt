cmake_minimum_required(VERSION 3.20)
project(kcm_filesync VERSION 1.0)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Must precede KDEInstallDirs: without it ECM falls back to the Qt5 layout and
# asks for a qmake that does not exist on this system.
set(QT_MAJOR_VERSION 6)
find_package(Qt6 REQUIRED COMPONENTS Core Quick)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(KF6 REQUIRED COMPONENTS KCMUtils CoreAddons I18n)

# KDECMakeSettings does not set these for a standalone build outside the KDE
# source tree, and kcoreaddons_add_plugin refuses to run without them.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

kcmutils_add_qml_kcm(kcm_filesync SOURCES src/filesynckcm.cpp)
target_link_libraries(kcm_filesync PRIVATE
    Qt6::Core Qt6::Quick KF6::KCMUtilsQuick KF6::CoreAddons KF6::I18n)
target_include_directories(kcm_filesync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
