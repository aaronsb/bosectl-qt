# Three test binaries:
#   bmap_tests        — the vendored bosectl C++ suite, built against the
#                       submodule commit this app pins. Catches a stale or
#                       broken pin before the tray ever links it.
#   bosectl-qt_tests  — QtTest suite for the app's own logic (BmapWorker
#                       slots over a mock transport, error mapping, state
#                       comparison).
#   autostart_tests   — start-on-login entry write/remove, under
#                       QStandardPaths test mode.

find_package(Qt6 REQUIRED COMPONENTS Test)

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
