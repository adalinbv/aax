find_package(pybind11 CONFIG QUIET)

if(pybind11_FOUND)
  find_package(Python3 REQUIRED)
  pybind11_add_module(aeonwave bindings.cpp)
  target_link_libraries(aeonwave PRIVATE pybind11::module aax)
  install(TARGETS aeonwave
          LIBRARY DESTINATION ${Python3_SITELIB} COMPONENT Libraries)
else()
  message(AUTHOR_WARNING "pybind11 not found: the python bindings will not be build.")
endif()
