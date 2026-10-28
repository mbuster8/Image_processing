# 6.4 is what Ubuntu 24.04 LTS ships; nothing here needs newer.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# Here rather than at the top level, so automatic moc, uic and rcc never touch the Qt-free libraries.
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

include(${PROJECT_SOURCE_DIR}/cmake/bundle_icon.cmake)

add_subdirectory(qt_common)
add_subdirectory(viewer)
add_subdirectory(example_sender)
