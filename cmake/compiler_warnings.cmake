# Warnings for this project's own targets only, and never as errors: a compiler or dependency upgrade must
# not break the build. Dependencies arrive as imported targets, so their headers are system headers and
# stay quiet.
function(id_vision_lab_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
