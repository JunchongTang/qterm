qt_add_executable(qterm_core_tests
    core/QTermCoreTest.cpp
)

qt_add_executable(qterm_session_tests
    session/QTermSessionTest.cpp
)

qt_add_executable(qterm_local_pty_tests
    session/QTermLocalPtyBackendTest.cpp
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

add_test(NAME qterm_core_tests COMMAND qterm_core_tests)
add_test(NAME qterm_session_tests COMMAND qterm_session_tests)
add_test(NAME qterm_local_pty_tests COMMAND qterm_local_pty_tests)