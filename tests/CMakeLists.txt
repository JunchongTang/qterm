qt_add_executable(qterm_core_tests
    core/QTermCoreTest.cpp
)

qt_add_executable(qterm_session_tests
    session/QTermSessionTest.cpp
)

qt_add_executable(qterm_local_pty_tests
    session/QTermLocalPtyBackendTest.cpp
)

qt_add_executable(qterm_quick_item_tests
    quick/QTermQuickPaintedItemTest.cpp
)

target_include_directories(qterm_core_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qterm_core_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

target_link_libraries(qterm_session_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

target_link_libraries(qterm_local_pty_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

target_link_libraries(qterm_quick_item_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
        QTerm::qterm
)

# Manual visual verification tool — not part of ctest.
qt_add_executable(qterm_resize_visual_test
    quick/QTermResizeVisualTest.cpp
)

target_link_libraries(qterm_resize_visual_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        QTerm::qterm
)

add_test(NAME qterm_core_tests COMMAND qterm_core_tests)
add_test(NAME qterm_session_tests COMMAND qterm_session_tests)
add_test(NAME qterm_local_pty_tests COMMAND qterm_local_pty_tests)
add_test(NAME qterm_quick_item_tests COMMAND qterm_quick_item_tests)