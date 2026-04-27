qt_add_executable(qterm_quick_demo
    main.cpp
)

qt_add_qml_module(qterm_quick_demo
    URI QTermDemo
    VERSION 1.0
    QML_FILES
        Main.qml
        TabManager.qml
        TerminalTab.qml
        TerminalPane.qml
        NewSessionDialog.qml
        PtySessionForm.qml
        SerialSessionForm.qml
        TelnetSessionForm.qml
        TermStatusBar.qml
        TermMenuBar.qml
)

target_link_libraries(qterm_quick_demo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        QTerm::qterm
        qtermplugin
)

# For static builds, ensure the QTerm QML plugin is linked into the executable.
qt_import_qml_plugins(qterm_quick_demo)