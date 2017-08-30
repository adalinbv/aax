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

set(ProgramFilesx86 "ProgramFiles(x86)")

FIND_PATH(XML_INCLUDE_DIR xml.h
  HINTS
  $ENV{XMLDIR}
  $ENV{ProgramFiles}/ZeroXML
  $ENV{${ProgramFilesx86}}/ZeroXML
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)

IF(RMALLOC)
  FIND_LIBRARY(XML_LIBRARY 
    NAMES XML-rmalloc zeroxml-rmalloc ZeroXML32-rmalloc libZeroXML32-rmalloc
    HINTS
    $ENV{XMLDIR}
    $ENV{ProgramFiles}/ZeroXML
    $ENV{${ProgramFilesx86}}/ZeroXML
    PATH_SUFFIXES lib lib/${CMAKE_LIBRARY_ARCHITECTURE} lib64 libs64 libs libs/Win32 libs/Win64
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /opt
  )

  CHECK_INCLUDE_FILE(${XML_INCLUDE_DIR}/rmalloc.h HAVE_RMALLOC_H)
  SET(USE_RMALLOC 1)
  SET(MALLOC_DEBUG ${USE_RMALLOC})


ELSE(RMALLOC)
  FIND_LIBRARY(XML_LIBRARY 
    NAMES XML zeroxml ZeroXML32 libZeroXML32
    HINTS
    $ENV{XMLDIR}
    $ENV{ProgramFiles}/ZeroXML
    $ENV{${ProgramFilesx86}}/ZeroXML
    PATH_SUFFIXES lib lib/${CMAKE_LIBRARY_ARCHITECTURE} lib64 libs64 libs libs/Win32 libs/Win64
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /opt
  )
ENDIF(RMALLOC)

SET(XML_FOUND "NO")
IF(XML_LIBRARY AND XML_INCLUDE_DIR)
  SET(XML_FOUND "YES")
ENDIF(XML_LIBRARY AND XML_INCLUDE_DIR)
