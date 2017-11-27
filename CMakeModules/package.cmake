
macro(aax_sources objs)
  set(SOURCES "")
  foreach(src ${objs})
    set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
  endforeach()
endmacro()

macro(aax_subdirectory dir)
  add_subdirectory(${dir})
  get_directory_property(OBJECTS DIRECTORY ${dir} DEFINITION SOURCES)
  set(SOURCES "${OBJECTS};${SOURCES}")
endmacro()
