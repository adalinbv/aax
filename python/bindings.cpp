#include <pybind11/pybind11.h>
#include <aax/aeonwave>

namespace aax = aeonwave;
namespace py = pybind11;

PYBIND11_MODULE(aeonwave,m) {
    py::class_<aax::dsp>(m, "dsp")
        // constructors
        .def(py::init<>())

        // methods
        .def("set", py::overload_cast<uint64_t>(&aax::dsp::set))
        .def("set", py::overload_cast<unsigned,float,float,float,float,enum aaxType>(&aax::dsp::set))
        .def("set", py::overload_cast<enum aaxParameter,float,enum aaxType>(&aax::dsp::set))
        .def("state", &aax::dsp::state)
        ;

    py::class_<aax::Buffer>(m, "Buffer")
        // constructors
        .def(py::init<>())

        // methods
        .def("set", py::overload_cast<enum aaxSetupType,int64_t>(&aax::Buffer::set))
        .def("get", &aax::Buffer::get)
        .def("getf", &aax::Buffer::getf)
        ;

    py::class_<aax::Emitter>(m, "Emitter")
        // constructors
        .def(py::init<>()) 
        .def(py::init<enum aaxEmitterMode>())

        // methods
        .def("set", py::overload_cast<enum aaxModeType,int>(&aax::Emitter::set))
        .def("set", py::overload_cast<enum aaxSetupType,int64_t>(&aax::Emitter::set))
        .def("set", py::overload_cast<enum aaxState>(&aax::Emitter::set))
        .def("set", py::overload_cast<aax::dsp&>(&aax::Emitter::set))
        .def("get", py::overload_cast<enum aaxModeType>(&aax::Emitter::get))
        .def("get", py::overload_cast<enum aaxSetupType>(&aax::Emitter::get))
        .def("get", py::overload_cast<enum aaxFilterType>(&aax::Emitter::get))
        .def("get", py::overload_cast<enum aaxEffectType>(&aax::Emitter::get))
        .def("get", py::overload_cast<aax::Matrix64&>(&aax::Emitter::get))
        .def("get", py::overload_cast<aax::Vector&>(&aax::Emitter::get))
        .def("get", py::overload_cast<unsigned int,bool>(&aax::Emitter::get))
        .def("get", py::overload_cast<enum aaxState>(&aax::Emitter::get))
        .def("getf", &aax::Emitter::getf)
        .def("offset", py::overload_cast<unsigned long,enum aaxType>(&aax::Emitter::offset))
        .def("offset", py::overload_cast<float>(&aax::Emitter::offset))
        .def("offset", py::overload_cast<enum aaxType>(&aax::Emitter::offset))
        .def("offset", py::overload_cast<>(&aax::Emitter::offset))
        .def("matrix", &aax::Emitter::matrix)
        .def("velocity", &aax::Emitter::velocity)
        .def("add", &aax::Emitter::add)
        .def("remove_buffer", &aax::Emitter::remove_buffer)
        ;

    py::class_<aax::AeonWave>(m, "AeonWave")
        // constructors
        .def(py::init<>())
        .def(py::init<enum aaxRenderMode>())
        .def(py::init<const char*,enum aaxRenderMode>())

        // methods
        .def("sensor_matrix", &aax::AeonWave::sensor_matrix)
        .def("sensor_velocity", &aax::AeonWave::sensor_velocity)
        .def("add", py::overload_cast<aax::Frame&>(&aax::AeonWave::add))
        .def("add", py::overload_cast<aax::Emitter&>(&aax::AeonWave::add))
        .def("add", py::overload_cast<aax::Buffer&>(&aax::AeonWave::add))
        .def("remove", py::overload_cast<aax::Frame&>(&aax::AeonWave::remove))
        .def("remove", py::overload_cast<aax::Emitter&>(&aax::AeonWave::remove))       
        .def("buffer", &aax::AeonWave::buffer)
        .def("destroy", &aax::AeonWave::destroy)
        .def("buffer_avail", &aax::AeonWave::buffer_avail)
        .def("playback", py::overload_cast<const std::string>(&aax::AeonWave::playback))
        .def("stop", &aax::AeonWave::stop)
        .def("playing", &aax::AeonWave::playing)
        ;
}
