cmake_minimum_required(VERSION 3.25)

qt_add_executable(quick-demo
    main.cpp
)

qt_add_qml_module(quick-demo
    URI QuickDemo
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(quick-demo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        QTerm::QTermLib
)

set_target_properties(quick-demo PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QTermDemo"
    AUTOMOC ON
)
