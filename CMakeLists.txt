cmake_minimum_required(VERSION 3.16)
project(bosectl-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)

# bmap library from bosectl submodule
add_library(bmap STATIC
    lib/bosectl/cpp/src/transport.cpp
    lib/bosectl/cpp/src/discovery.cpp
)
target_include_directories(bmap PUBLIC lib/bosectl/cpp/src)
set_target_properties(bmap PROPERTIES AUTOMOC OFF)

# Qt tray application
add_executable(bosectl-qt
    src/main.cpp
    src/TrayIcon.cpp
    src/BmapWorker.cpp
    src/EqWindow.cpp
    src/ModeWindow.cpp
    src/NcWindow.cpp
    src/Logging.cpp
    src/BluezBatteryProvider.cpp
    resources/resources.qrc
)
target_link_libraries(bosectl-qt PRIVATE Qt6::Widgets Qt6::DBus bmap bluetooth)
target_include_directories(bosectl-qt PRIVATE src)

# Install targets
include(GNUInstallDirs)
install(TARGETS bosectl-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/bosectl-qt.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES resources/bosectl-qt.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install(FILES resources/bosectl-qt-autostart.desktop
        DESTINATION /etc/xdg/autostart)
