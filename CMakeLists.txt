cmake_minimum_required(VERSION 3.25)

project(QTerm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml Test)

qt_standard_project_setup(REQUIRES 6.8)

option(QTERM_BUILD_EXAMPLES "Build QTerm examples" ON)
option(QTERM_BUILD_TESTS "Build QTerm tests" ON)

enable_testing()

add_subdirectory(third_party)
add_subdirectory(src)

if(QTERM_BUILD_EXAMPLES)
    add_subdirectory(examples/quick-demo)
endif()

if(QTERM_BUILD_TESTS)
    add_subdirectory(tests)
endif()