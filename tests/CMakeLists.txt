# Five test binaries:
#   bmap_tests        — the vendored bosectl C++ suite, built against the
#                       submodule commit this app pins. Catches a stale or
#                       broken pin before the tray ever links it.
#   bosectl-qt_tests  — QtTest suite for the app's own logic (BmapWorker
#                       slots over a mock transport, error mapping, state
#                       comparison).
#   autostart_tests   — start-on-login entry write/remove, under
#                       QStandardPaths test mode.
#   help_tests        — help.md parsing, and help.md and the Help window
#                       agreeing on the same set of ids. Runs offscreen.
#   tray_tests        — the tray menu and its windows against the simulated
#                       headset: menu state per device state, and what each
#                       control writes to the device. Runs offscreen.

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

set(BMAP_TESTS_DIR ${CMAKE_SOURCE_DIR}/lib/bosectl/cpp/tests)
add_executable(bmap_tests
    ${BMAP_TESTS_DIR}/test_main.cpp
    ${BMAP_TESTS_DIR}/test_protocol.cpp
    ${BMAP_TESTS_DIR}/test_parsers.cpp
    ${BMAP_TESTS_DIR}/test_connection.cpp
    ${BMAP_TESTS_DIR}/test_catalog.cpp
    ${BMAP_TESTS_DIR}/test_transport.cpp
)
target_link_libraries(bmap_tests PRIVATE bmap bluetooth)
set_target_properties(bmap_tests PROPERTIES AUTOMOC OFF)
add_test(NAME bmap_tests COMMAND bmap_tests)

add_executable(bosectl-qt_tests
    test_bmap_worker.cpp
    ${CMAKE_SOURCE_DIR}/src/BmapWorker.h
    ${CMAKE_SOURCE_DIR}/src/Logging.cpp
)
target_include_directories(bosectl-qt_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bosectl-qt_tests PRIVATE Qt6::Core Qt6::Test bmap bluetooth)
add_test(NAME bosectl-qt_tests COMMAND bosectl-qt_tests)

add_executable(autostart_tests test_autostart.cpp)
target_include_directories(autostart_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autostart_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME autostart_tests COMMAND autostart_tests)

# Builds in the real help.md through resources.qrc, so the drift checks run
# against the file the app ships.
add_executable(help_tests
    test_help.cpp
    ${CMAKE_SOURCE_DIR}/src/HelpWindow.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_include_directories(help_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(help_tests PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME help_tests COMMAND help_tests)
set_tests_properties(help_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# resources.qrc again: help.md feeds the menu tooltips, and the tray icon.
add_executable(tray_tests
    test_tray.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
target_link_libraries(tray_tests PRIVATE bosectl-qt-core Qt6::Test)
add_test(NAME tray_tests COMMAND tray_tests)
set_tests_properties(tray_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
