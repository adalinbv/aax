find_package(pybind11 QUIET)

if(pybind11_FOUND)
  add_library(aeonwave MODULE bindings.cpp)
  target_link_libraries(aeonwave PRIVATE pybind11::module aax)
  set_target_properties(aeonwave PROPERTIES PREFIX "${PYTHON_MODULE_PREFIX}"
                                            SUFFIX "${PYTHON_MODULE_EXTENSION}")
endif()
