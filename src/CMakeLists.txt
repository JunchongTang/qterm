qt_add_library(qterm STATIC)

add_library(QTerm::QTerm ALIAS qterm)

target_sources(qterm
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermcell.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermlibvtermparser.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermparser.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermplaintextsurfaceadapter.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermtextparser.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermlocalbackend.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermsession.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermsessionbackend.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermsurfacemodel.h
            ${CMAKE_CURRENT_SOURCE_DIR}/../include/QTerm/qtermquickitem.h
)

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
        QTerm::libvterm
)

target_compile_features(qterm PUBLIC cxx_std_17)

add_subdirectory(core)
add_subdirectory(session)
add_subdirectory(quick)
add_subdirectory(platform)