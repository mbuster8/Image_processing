# Gives a macOS app bundle its Finder and Dock icon, made at build time from one square PNG (1024 pixels is
# best), so the repository holds only the PNG. Elsewhere it does nothing: Windows icons come with the Windows
# port.
function(image_processing_add_bundle_icon target png)
    if(NOT APPLE)
        return()
    endif()
    set(script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/make_icns.cmake")
    set(icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
    add_custom_command(OUTPUT "${icns}"
        COMMAND "${CMAKE_COMMAND}" "-DPNG=${png}" "-DICNS=${icns}" -P "${script}"
        DEPENDS "${png}" "${script}"
        COMMENT "Making ${target}.icns"
        VERBATIM)
    target_sources(${target} PRIVATE "${icns}")
    set_source_files_properties("${icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${target}.icns")
endfunction()
