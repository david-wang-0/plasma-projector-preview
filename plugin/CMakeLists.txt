# SPDX-FileCopyrightText: 2026 David Wang
# SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

cmake_minimum_required(VERSION 3.21)
project(outputstream LANGUAGES C CXX)  # C: the generated wayland-*-protocol.c

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml WaylandClient)
qt_standard_project_setup(REQUIRES 6.7)

# The protocol XML ships with plasma-wayland-protocols.
find_package(PlasmaWaylandProtocols QUIET)
if(NOT PLASMA_WAYLAND_PROTOCOLS_DIR)
    set(PLASMA_WAYLAND_PROTOCOLS_DIR /usr/share/plasma-wayland-protocols)
endif()
set(SCREENCAST_XML ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml)
if(NOT EXISTS ${SCREENCAST_XML})
    message(FATAL_ERROR "${SCREENCAST_XML} not found; install plasma-wayland-protocols")
endif()

# Where the module is installed, relative to CMAKE_INSTALL_PREFIX (build.sh sets the prefix).
set(QML_INSTALL_DIR lib/qt6/qml CACHE STRING "QML import dir, relative to the install prefix")

set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

qt_add_qml_module(outputstream
    URI local.outputstream
    VERSION 1.0
    PLUGIN_TARGET outputstream
    SOURCES
        screencasting.cpp screencasting.h
        outputstream.cpp outputstream.h
)

qt6_generate_wayland_protocol_client_sources(outputstream FILES ${SCREENCAST_XML})

target_link_libraries(outputstream PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::WaylandClient)
# A QML plugin is dlopen()ed, so a missing symbol would only show up at load time; fail at link time.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(outputstream PRIVATE "LINKER:--no-undefined")
endif()

set(module_dir ${QML_INSTALL_DIR}/local/outputstream)
install(TARGETS outputstream LIBRARY DESTINATION ${module_dir})
install(FILES
    ${QT_QML_OUTPUT_DIRECTORY}/local/outputstream/qmldir
    ${QT_QML_OUTPUT_DIRECTORY}/local/outputstream/outputstream.qmltypes
    DESTINATION ${module_dir})
