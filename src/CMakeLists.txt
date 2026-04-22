qt_add_library(QTermLib STATIC
    backend/qtermsessionbackend.cpp
    backend/qtermlocalbackend.cpp
    vterm/qtermvtermprocessor.cpp
    session/qtermsession.cpp
    quick/qtermquickitem.cpp
)

add_library(QTerm::QTermLib ALIAS QTermLib)

target_include_directories(QTermLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(QTermLib
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        QTerm::libvterm
)

set_target_properties(QTermLib PROPERTIES
    AUTOMOC ON
)
