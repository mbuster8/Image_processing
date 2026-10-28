# cmake -DPNG=<square png> -DICNS=<output .icns> -P make_icns.cmake
# Scales the PNG to every size an .icns holds with sips, then packs them with iconutil; both come with macOS.
string(REGEX REPLACE "\\.icns$" ".iconset" iconset "${ICNS}")
file(REMOVE_RECURSE "${iconset}")
file(MAKE_DIRECTORY "${iconset}")

function(scale pixels name)
    execute_process(COMMAND sips -z ${pixels} ${pixels} "${PNG}" --out "${iconset}/${name}"
                    OUTPUT_QUIET RESULT_VARIABLE result)
    if(result)
        message(FATAL_ERROR "sips could not scale ${PNG} to ${pixels} pixels")
    endif()
endfunction()

foreach(size IN ITEMS 16 32 128 256 512)
    math(EXPR double "${size} * 2")
    scale(${size} "icon_${size}x${size}.png")
    scale(${double} "icon_${size}x${size}@2x.png")
endforeach()

execute_process(COMMAND iconutil -c icns "${iconset}" -o "${ICNS}" RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "iconutil could not make ${ICNS}")
endif()
