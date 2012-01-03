
macro(define_library name includePath sources headers)

  foreach(s ${sources})
    set_property(GLOBAL
            APPEND PROPERTY ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${s}")
  endforeach()

  foreach(h ${headers})
    set_property(GLOBAL
            APPEND PROPERTY PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${h}")
  endforeach()

  if (SHARED_LIBRARY)
    set(libName "${name}")
    add_library(${libName} STATIC ${sources} ${headers})

    install (TARGETS ${libName} ARCHIVE DESTINATION lib${LIB_SUFFIX})
    install (FILES ${headers}  DESTINATION include/${includePath})

  else ()
    set(libName "${name}")
    add_library(${libName} LIBTYPE SHARED ${sources} ${headers})
    set_target_properties(${libName} PROPERTIES DEFINE_SYMBOL ${libName}
                                     VERSION ${LIB_VERSION}.0
                                     SOVERSION ${LIB_MAJOR_VERSION})
    install (TARGETS ${libName} LIBRARY DESTINATION lib${LIB_SUFFIX})
    install (FILES ${headers}  DESTINATION include/${includePath})

  endif()

endmacro()

