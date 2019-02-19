# Copyright (c) 2014 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# EBUR128_FOUND
# EBUR128_INCLUDE_DIR
# EBUR128_LIBRARY

set(ProgramFilesx86 "ProgramFiles(x86)")

find_path(EBUR128_INCLUDE_DIR
  NAMES ebur128.h
  HINTS
  "$ENV{${ProgramFilesx86}}/libebur128"
  PATH_SUFFIXES include
)

find_library(EBUR128_LIBRARY
  NAMES ebur128
  HINTS
  "$ENV{${ProgramFilesx86}}/libebur128"
  PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EBUR128 DEFAULT_MSG EBUR128_LIBRARY EBUR128_INCLUDE_DIR)

mark_as_advanced(EBUR128_INCLUDE_DIR EBUR128_LIBRARY)
