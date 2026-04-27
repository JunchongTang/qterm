qt_add_library(qterm STATIC
    ../include/QTerm/QTermLocalPtyBackend.h
    ../include/QTerm/QTermSerialBackend.h
    ../include/QTerm/QTermSerialPortInfo.h
    ../include/QTerm/QTermSerialPortScanner.h
    ../include/QTerm/QTermTelnetBackend.h
    ../include/QTerm/QTermQuickPaintedItem.h
    ../include/QTerm/QTermQuickItem.h
    ../include/QTerm/QTermSurfaceModel.h
    ../include/QTerm/QTermSession.h
    ../include/QTerm/QTermSessionBackend.h
    ../include/QTerm/QTermTerminal.h
    ../include/QTerm/QTermTheme.h
    ../include/QTerm/QTermThemePack.h
    ../include/QTerm/QTermThemeLoader.h
    ../include/QTerm/QTermWidget.h
    QTermTheme.cpp
    QTermThemePack.cpp
    QTermThemeLoader.cpp
    core/QTermBuffer.h
    core/QTermBuffer.cpp
    core/QTermCell.h
    core/QTermCore.h
    core/QTermInputEncoder.h
    core/QTermInputEncoder.cpp
    core/QTermInputExecutor.h
    core/QTermInputExecutor.cpp
    core/QTermLine.h
    core/QTermLine.cpp
    core/QTermModeState.h
    core/QTermSelectionModel.h
    core/QTermSelectionModel.cpp
    core/QTermCursorState.h
    core/QTermCore.cpp
    core/QTermSavedCursorState.h
    core/QTermScreenState.h
    core/QTermTextParser.h
    core/QTermTextParser.cpp
    core/QTermSurfaceModel.cpp
    core/QTermTerminal.cpp
    quick/QTermViewController.h
    quick/QTermViewController.cpp
    quick/QTermQmlForeignTypes.cpp
    quick/QTermQuickPaintedItem.cpp
    quick/QTermQuickItem.cpp
    widget/QTermWidget.cpp
    session/local/QTermLocalPtyBackend.cpp
    session/serial/QTermSerialBackend.cpp
    session/serial/QTermSerialPortInfo.cpp
    session/serial/QTermSerialPortScanner.cpp
    session/telnet/QTermTelnetBackend.cpp
    session/QTermSession.cpp
    session/QTermSessionBackend.cpp
)

qt_add_qml_module(qterm
    URI QTerm
    VERSION 1.0
    # All QML types are declared via QML_ELEMENT / QML_UNCREATABLE / QML_VALUE_TYPE
    # macros in their respective public headers — no QML_FILES needed here.
)

if(APPLE)
    target_sources(qterm PRIVATE quick/QTermMacOSUtils.mm)
    target_link_libraries(qterm PRIVATE "-framework Foundation")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(qterm PRIVATE util)
endif()

add_library(QTerm::qterm ALIAS qterm)

target_include_directories(qterm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/quick
)

target_link_libraries(qterm
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Widgets
        Qt6::Network
        Qt6::SerialPort
)

target_compile_features(qterm PUBLIC cxx_std_17)