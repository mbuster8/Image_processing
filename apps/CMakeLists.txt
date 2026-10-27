# 6.4 is what Ubuntu 24.04 LTS ships; nothing here needs newer.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# Here rather than at the top level, so automatic moc, uic and rcc never touch the Qt-free libraries.
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

add_subdirectory(qt_common)
add_subdirectory(image_processing)
add_subdirectory(client)
