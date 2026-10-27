# AddressSanitizer and UndefinedBehaviorSanitizer, for the asan preset.

if(IMAGE_PROCESSING_SANITIZERS AND NOT MSVC)
    # Global rather than per target, so every object in an executable uses the same instrumented runtime.
    # No recovery: undefined behaviour aborts like an ASan error, so it fails the test instead of printing a
    # report into a passing run.
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Compiles the sanitizer defaults into an executable, so they apply however it is started: by ctest, from
# Qt Creator, or by GoogleTest's test discovery during the build.
function(image_processing_add_sanitizer_defaults target)
    if(IMAGE_PROCESSING_SANITIZERS AND NOT MSVC)
        target_sources(${target} PRIVATE ${PROJECT_SOURCE_DIR}/cmake/sanitizer_defaults.cpp)
    endif()
endfunction()
