# Locate XML
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
  $ENV{ProgramFiles}/zeroxml
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)

FIND_LIBRARY(XML_LIBRARY 
  NAMES XML zeroxml ZeroXML32
  HINTS
  $ENV{XMLDIR}
  $ENV{ProgramFiles}/zeroxml
  PATH_SUFFIXES lib lib/${CMAKE_LIBRARY_ARCHITECTURE} lib64 libs64 libs libs/Win32 libs/Win64
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)


SET(XML_FOUND "NO")
IF(XML_LIBRARY AND XML_INCLUDE_DIR)
  SET(XML_FOUND "YES")
ENDIF(XML_LIBRARY AND XML_INCLUDE_DIR)
