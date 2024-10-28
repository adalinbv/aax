#include <pybind11/pybind11.h>
#include <aax/simple_aeonwave>

namespace aax = aeonwave;
namespace py = pybind11;

PYBIND11_MODULE(aeonwave,m) {
    py::enum_<aaxRenderMode>(m, "aaxRenderMode")
        .value("AAX_MODE_READ", AAX_MODE_READ)
        .value("AAX_MODE_WRITE_STEREO", AAX_MODE_WRITE_STEREO)
        .value("AAX_MODE_WRITE_SPATIAL", AAX_MODE_WRITE_SPATIAL)
        .value("AAX_MODE_WRITE_SPATIAL_SURROUND", AAX_MODE_WRITE_SPATIAL_SURROUND)
        .value("AAX_MODE_WRITE_HRTF", AAX_MODE_WRITE_HRTF)
        .value("AAX_MODE_WRITE_SURROUND", AAX_MODE_WRITE_SURROUND)
        .export_values();

    py::class_<aax::SoundSource>(m, "SoundSource")
        // constructors
        .def(py::init<aax::AeonWave&,std::string&>())

        // methods
        .def("play", &aax::SoundSource::play,
               py::arg("p")=0.0f)
        .def("pause", &aax::SoundSource::pause)
        .def("stop", &aax::SoundSource::stop)
        .def("set_pitch", &aax::SoundSource::set_pitch)
        .def("set_volume", &aax::SoundSource::set_volume)
        .def("set_looping", &aax::SoundSource::set_looping,
              py::arg("l")=true)
        .def("set_balance", &aax::SoundSource::set_balance)
        ;

    py::class_<aax::SimpleMixer>(m, "SimpleMixer")
        // constructors
        .def(py::init<const std::string,enum aaxRenderMode>(),
               py::arg("d")="", py::arg("m")=AAX_MODE_WRITE_STEREO)

        // methods
        .def("source", &aax::SimpleMixer::source,
               py::return_value_policy::reference)
        .def("set_balance", &aax::SimpleMixer::set_balance)
        ;
}
