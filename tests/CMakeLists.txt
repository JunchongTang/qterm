qt_add_executable(qterm_core_tests
    core/QTermCoreTest.cpp
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

add_test(NAME qterm_core_tests COMMAND qterm_core_tests)