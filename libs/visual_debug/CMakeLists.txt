add_library(visual_debug STATIC
    src/client.cpp
    src/endpoint.cpp
    src/image_sender.cpp
    src/send_image.cpp
    src/server.cpp
    src/wire_format.cpp)
add_library(visual_debug::visual_debug ALIAS visual_debug)

target_include_directories(visual_debug PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(visual_debug PUBLIC cxx_std_20)

# Host applications see only OpenCV in the public headers; ZeroMQ and JSON stay implementation details.
target_link_libraries(visual_debug
    PUBLIC
        opencv_core
    PRIVATE
        cppzmq
        nlohmann_json::nlohmann_json)

id_vision_lab_set_warnings(visual_debug)
