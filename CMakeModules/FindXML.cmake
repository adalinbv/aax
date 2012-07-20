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

FIND_PATH(RMALLOC_INCLUDE_DIR rmalloc.h
  HINTS
  $ENV{XMLDIR}
  $ENV{ProgramFiles}/ZeroXml
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt
)

