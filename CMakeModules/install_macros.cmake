FIND_PROGRAM(SIGNTOOL_EXECUTABLE signtool 
  PATHS "$ENV{ProgramFiles}/Microsoft SDKs/Windows/v7.0A"
)
IF(NOT SIGNTOOL_EXECUTABLE)
  MESSAGE(FATAL_ERROR 
          "signtool is not found. Signing executables not possible")
ENDIF()

MACRO(SIGN_TARGET target)
 GET_TARGET_PROPERTY(target_type ${target} TYPE)
 IF(target_type AND NOT target_type MATCHES "STATIC")
   GET_TARGET_PROPERTY(target_location ${target}  LOCATION)
   IF(CMAKE_GENERATOR MATCHES "Visual Studio")
   STRING(REPLACE "${CMAKE_CFG_INTDIR}" "\${CMAKE_INSTALL_CONFIG_NAME}" 
     target_location ${target_location})
   ENDIF()
   INSTALL(CODE
   "EXECUTE_PROCESS(COMMAND 
     ${SIGNTOOL_EXECUTABLE} sign ${SIGNTOOL_PARAMETERS} ${target_location}
     RESULT_VARIABLE ERR)
    IF(NOT \${ERR} EQUAL 0)
      MESSAGE(FATAL_ERROR \"Error signing  ${target_location}\")
    ENDIF()
   ")
 ENDIF()
ENDMACRO()

FOREACH(target ${TARGETS})
  # If signing is required, sign executables before installing
   IF(SIGNCODE AND SIGNCODE_ENABLED)
    SIGN_TARGET(${target})
  ENDIF()
  # Install man pages on Unix
  IF(UNIX)
    GET_TARGET_PROPERTY(target_location ${target} LOCATION)
    INSTALL_MANPAGE(${target_location})
  ENDIF()
ENDFOREACH()
