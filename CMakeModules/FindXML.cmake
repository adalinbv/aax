# Locate ZeroXML
# This module defines
# XML_LIBRARIES
# XML_FOUND, if false, do not try to link to XML 
# XML_INCLUDE_DIR, where to find the headers
#
# $XMLDIR is an environment variable that would
# correspond to the ./configure --prefix=$XMLDIR
# used in building XML.
#
# Created by Erik Hofman.

FIND_PATH(XML_INCLUDE_DIR xml.h
  HINTS
  $ENV{XMLDIR}
  $ENV{ProgramFiles}/ZeroXML
  $ENV{ProgramFiles}/Adalin/ZeroXML
  ${CMAKE_SOURCE_DIR}/zeroxml
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)

FIND_LIBRARY(XML_LIBRARY 
  NAMES XML xml zeroxml libzeroxml
  HINTS
  $ENV{XMLDIR}
  $ENV{ProgramFiles}/ZeroXML
  $ENV{ProgramFiles}/Adalin/ZeroXML
  ${CMAKE_BUILD_DIR}/xml
  PATH_SUFFIXES lib64 lib lib/${CMAKE_LIBRARY_ARCHITECTURE} libs64 libs libs/Win32 libs/Win64 bin
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)

IF(XML_LIBRARY AND XML_INCLUDE_DIR)
  SET(XML_FOUND "YES")
ELSE(XML_LIBRARY AND XML_INCLUDE_DIR)
  IF(NOT XML_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Unable to find the XML library development files.")
    SET(XML_FOUND "NO")
  ENDIF(NOT XML_INCLUDE_DIR)
  IF(NOT XML_LIBRARY)
    IF(SINGLE_PACKAGE)
      SET(XML_LIBRARY "${xml_BUILD_DIR}/xml/ZeroXML.lib")
      SET(XML_FOUND "YES")
    ELSE(SINGLE_PACKAGE)
    ENDIF(SINGLE_PACKAGE)
  ENDIF(NOT XML_LIBRARY)
ENDIF(XML_LIBRARY AND XML_INCLUDE_DIR)

