cmake_minimum_required(VERSION 3.21)

project(SuperpowerDesktop VERSION 1.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

qt_add_executable(superpower-desktop
  src/main.cpp
  src/mainwindow.cpp
  src/mainwindow.h
  src/mainwindowactionrouter.cpp
  src/mainwindowworkflow.cpp
  src/mainwindowstate.cpp
  src/mcpbridgeprocess.cpp
  src/mcpbridgeprocess.h
  src/conversationwindow.cpp
  src/conversationwindow.h
  src/onboardingdialog.cpp
  src/onboardingdialog.h
  src/workspacedashboard.cpp
  src/workspacedashboard.h
  src/workflowreviewdialog.cpp
  src/workflowreviewdialog.h
)

target_link_libraries(superpower-desktop PRIVATE Qt6::Widgets)

target_compile_definitions(superpower-desktop PRIVATE
  SUPERPOWER_DESKTOP_VERSION="${PROJECT_VERSION}"
)

set_target_properties(superpower-desktop PROPERTIES
  OUTPUT_NAME "Superpower Desktop"
)

if(WIN32)
  set_target_properties(superpower-desktop PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
  set_target_properties(superpower-desktop PROPERTIES MACOSX_BUNDLE TRUE)
endif()
