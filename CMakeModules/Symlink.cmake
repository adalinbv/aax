# Symbol link module for CMake
#
# Usage:
#   symlink(<DEST> <SOURCE>)
#   install_symlink(<DEST> <SOURCE>)

macro(InstallSymlink _filepath _sympath)
    get_filename_component(_symname ${_sympath} NAME)
    get_filename_component(_installdir ${_sympath} PATH)
    message("Installing link: ${_installdir}/${_symname} -> ${_filepath}")
 
    if (BINARY_PACKAGING_MODE OR
        "${CMAKE_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
         install(CODE "
              execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
                              ${_filepath}
                              ${_installdir}/${_symname})
          ")
    else ()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
                        ${_filepath}
                        ${_installdir}/${_symname})
        FILE(INSTALL ${_installdir}/${_symname}
                DESTINATION ${_installdir})
    endif ()
    list(APPEND CMAKE_INSTALL_MANIFEST_FILES ${_installdir}/${_symname})
endmacro(InstallSymlink)

