find_package(pybind11 REQUIRED)

add_library(aeonwave MODULE bindings.cpp)
target_link_libraries(aeonwave PRIVATE pybind11::module aax)
set_target_properties(aeonwave PROPERTIES PREFIX "${PYTHON_MODULE_PREFIX}"
                                          SUFFIX "${PYTHON_MODULE_EXTENSION}")
