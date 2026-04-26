qt_add_library(qterm STATIC
    ../include/QTerm/QTermLocalPtyBackend.h
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
    quick/QTermQuickPaintedItem.cpp
    quick/QTermQuickItem.cpp
    widget/QTermWidget.cpp
    session/local/QTermLocalPtyBackend.cpp
    session/QTermSession.cpp
    session/QTermSessionBackend.cpp
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
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(qterm
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Widgets
)

target_compile_features(qterm PUBLIC cxx_std_17)