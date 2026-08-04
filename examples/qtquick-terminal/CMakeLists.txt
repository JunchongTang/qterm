qt_add_executable(qtquick_terminal
    main.cpp
)

set_target_properties(qtquick_terminal PROPERTIES
    OUTPUT_NAME "qtquick-terminal"
)

set_source_files_properties(
    src/qml/components/Theme.qml
    src/qml/components/AppState.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(qtquick_terminal
    URI QtQuickTerminal
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/TabWorkspace.qml
        src/qml/TerminalTab.qml
        src/qml/TerminalPane.qml
        src/qml/NewSessionDialog.qml
        src/qml/PtySessionForm.qml
        src/qml/SerialSessionForm.qml
        src/qml/TelnetSessionForm.qml
        src/qml/components/Theme.qml
        src/qml/components/AppState.qml
        src/qml/components/Icon.qml
        src/qml/components/FocusRing.qml
        src/qml/components/Label.qml
        src/qml/components/Button.qml
        src/qml/components/IconButton.qml
        src/qml/components/Input.qml
        src/qml/components/Select.qml
        src/qml/components/Tabs.qml
        src/qml/components/TabButton.qml
        src/qml/components/Dialog.qml
    RESOURCES
        src/qml/assets/icons/x.svg
        src/qml/assets/icons/plus.svg
        src/qml/assets/icons/check.svg
        src/qml/assets/icons/chevron-down.svg
        src/qml/assets/icons/terminal.svg
        src/qml/assets/icons/refresh-cw.svg
        src/qml/assets/icons/sun.svg
        src/qml/assets/icons/moon.svg
        src/qml/assets/icons/layers.svg
)

target_link_libraries(qtquick_terminal
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
qt_import_qml_plugins(qtquick_terminal)
