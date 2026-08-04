qt_add_executable(qwidget_terminal
    main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/NewSessionDialog.cpp
    src/NewSessionDialog.h
    src/SessionConfig.h
    src/TerminalTab.cpp
    src/TerminalTab.h
    src/Theme.cpp
    src/Theme.h
)

set_target_properties(qwidget_terminal PROPERTIES
    OUTPUT_NAME "qwidget-terminal"
)

# SVG indicators referenced from the style sheet; the qsvg image plugin is
# loaded at run time.
qt_add_resources(qwidget_terminal "assets"
    PREFIX "/"
    FILES
        assets/icons/check.svg
        assets/icons/chevron-down-dark.svg
        assets/icons/chevron-down-light.svg
        assets/icons/chevron-down.svg
        assets/icons/close-dark.svg
        assets/icons/close-light.svg
        assets/icons/layers.svg
        assets/icons/moon.svg
        assets/icons/plus.svg
        assets/icons/refresh-cw.svg
        assets/icons/sun.svg
        assets/icons/terminal.svg
        assets/icons/x.svg
)

target_link_libraries(qwidget_terminal
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        QTerm::qterm
)
