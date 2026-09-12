qt_add_executable(qterm_core_tests
    core/QTermCoreTest.cpp
)

qt_add_executable(qterm_ref_tests
    core/QTermRefTest.cpp
)

target_include_directories(qterm_ref_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(qterm_ref_tests
    PRIVATE
        QTERM_REFDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/refdata"
)

target_link_libraries(qterm_ref_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

qt_add_executable(qterm_session_tests
    session/QTermSessionTest.cpp
)

qt_add_executable(qterm_local_shell_tests
    session/QTermLocalShellBackendTest.cpp
)

qt_add_executable(qterm_serial_tests
    session/QTermSerialBackendTest.cpp
)

qt_add_executable(qterm_quick_item_tests
    quick/QTermQuickPaintedItemTest.cpp
)

qt_add_executable(qterm_cursor_visibility_tests
    core/QTermCursorVisibilityTest.cpp
)

target_link_libraries(qterm_cursor_visibility_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

add_test(NAME qterm_cursor_visibility_tests COMMAND qterm_cursor_visibility_tests)

qt_add_executable(qterm_capability_tests
    core/QTermCapabilityTest.cpp
)

target_include_directories(qterm_capability_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qterm_capability_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

add_test(NAME qterm_capability_tests COMMAND qterm_capability_tests)

# Regenerates docs/protocol-support.md from a real run of the capability tests.
# The table is never hand-edited: the previous hand-maintained one had drifted
# into claiming support the code did not have.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(qterm_protocol_support
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/generate_protocol_support.py
                $<TARGET_FILE:qterm_capability_tests>
        DEPENDS qterm_capability_tests
        COMMENT "Generating docs/protocol-support.md from the capability tests"
        VERBATIM)
endif()

qt_add_executable(qterm_widget_repaint_tests
    widget/QTermWidgetRepaintTest.cpp
)

target_link_libraries(qterm_widget_repaint_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        QTerm::qterm
)

add_test(NAME qterm_widget_repaint_tests COMMAND qterm_widget_repaint_tests)

qt_add_executable(qterm_inverse_text_tests
    quick/QTermInverseTextTest.cpp
)

target_link_libraries(qterm_inverse_text_tests
    PRIVATE
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
        QTerm::qterm
)

add_test(NAME qterm_inverse_text_tests COMMAND qterm_inverse_text_tests)

qt_add_executable(qterm_pointer_shape_tests
    quick/QTermPointerShapeTest.cpp
)

target_link_libraries(qterm_pointer_shape_tests
    PRIVATE
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
        QTerm::qterm
)

add_test(NAME qterm_pointer_shape_tests COMMAND qterm_pointer_shape_tests)

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

target_link_libraries(qterm_local_shell_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        QTerm::qterm
)

target_link_libraries(qterm_serial_tests
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
add_test(NAME qterm_ref_tests COMMAND qterm_ref_tests)
add_test(NAME qterm_session_tests COMMAND qterm_session_tests)
add_test(NAME qterm_local_shell_tests COMMAND qterm_local_shell_tests)
add_test(NAME qterm_serial_tests COMMAND qterm_serial_tests)
add_test(NAME qterm_quick_item_tests COMMAND qterm_quick_item_tests)
if(QTERM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
