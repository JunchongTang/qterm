qt_add_executable(qterm_quick_demo
    main.cpp
)

qt_add_qml_module(qterm_quick_demo
    URI QTerm.Demo
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(qterm_quick_demo
    PRIVATE
        QTerm::QTerm
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
)

set_target_properties(qterm_quick_demo PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)