cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network PrintSupport)

add_executable(fortxxi-receiver
    main.cpp
    receiverwindow.cpp
    receiverwindow.h
    qcustomplot.cpp
    qcustomplot.h
)

target_link_libraries(fortxxi-receiver Qt5::Core Qt5::Widgets Qt5::Network Qt5::PrintSupport)

set_target_properties(fortxxi-sender PROPERTIES
    WIN32_EXECUTABLE True
)
