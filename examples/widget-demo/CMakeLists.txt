qt_add_executable(qterm_widget_demo
    main.cpp
)

target_link_libraries(qterm_widget_demo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        QTerm::qterm
)
