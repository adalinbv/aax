/*
 * Copyright (C) 2015-2017 by Erik Hofman.
 * Copyright (C) 2015-2017 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of Adalin B.V. nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUTOF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Adalin B.V.
 */

#ifndef AAX_AEONWAVE_HPP
#define AAX_AEONWAVE_HPP 1

#include <map>
#include <vector>
#include <algorithm>

#include <aax/matrix.hpp>
#include <aax/aax.h>


namespace aax
{

inline unsigned major_version() {
    return aaxGetMajorVersion();
}
inline unsigned minor_version() {
    return aaxGetMinorVersion();
}
inline unsigned int patch_level() {
    return aaxGetPatchLevel();
}

inline unsigned bits_per_sample(enum aaxFormat fmt) {
    return aaxGetBitsPerSample(fmt);
}
inline unsigned bytes_per_sample(enum aaxFormat fmt) {
    return aaxGetBytesPerSample(fmt);
}

inline enum aaxErrorType error_no() {
    return aaxGetErrorNo();
}
inline const char* strerror(enum aaxErrorType e=error_no()) {
    return aaxGetErrorString(e);
}

inline bool is_valid(void* c, enum aaxHandleType t=AAX_CONFIG) {
    return aaxIsValid(c,t);
}

inline void free(void *ptr) {
    aaxFree(ptr);
}


class Obj
{
public:
    typedef int close_fn(void*);

    Obj() : ptr(0), closefn(0) {}

    Obj(void *p, const close_fn* c) : ptr(p), closefn(c) {}

    Obj(const Obj& o) : ptr(o.ptr), closefn(o.closefn) {
        o.closefn = 0;
    }

#if __cplusplus >= 201103L
    Obj(Obj&& o) : Obj() {
        swap(*this, o);
    }
#endif

    ~Obj() {
        if (!!closefn) closefn(ptr);
    }

    bool close() {
        bool rv = (!!closefn) ? closefn(ptr) : false;
        closefn = 0;
        return rv;
    }

    friend void swap(Obj& o1, Obj& o2) {
        std::swap(o1.ptr, o2.ptr);
        std::swap(o1.closefn, o2.closefn);
    }

    Obj& operator=(Obj o) {
        swap(*this, o);
        return *this;
    }

    operator void*() const {
        return ptr;
    }

    explicit operator bool() {
        return !!ptr;
    }

protected:
    void* ptr;
    mutable close_fn* closefn;
};

class Buffer : public Obj
{
public:
    Buffer() : Obj() {}

    Buffer(aaxBuffer b, bool o=true) :
        Obj(b, o ? aaxBufferDestroy : 0) {}

    Buffer(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) :
        Obj(aaxBufferCreate(c,n,t,f), aaxBufferDestroy) {}

    Buffer(const Buffer& o) : Obj(o) {}

    ~Buffer() {}

    inline void set(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) {
        ptr = aaxBufferCreate(c,n,t,f); closefn = aaxBufferDestroy;
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxBufferSetSetup(ptr,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxBufferGetSetup(ptr,t);
    }
    inline bool fill(const void* d) {
        return aaxBufferSetData(ptr,d);
    }
    inline void** data() {
        return aaxBufferGetData(ptr);
    }

    // ** buffer data mangling ******
    inline bool process(float f, enum aaxWaveformType t,
                        float r=0.5f, enum aaxProcessingType p=AAX_MIX) {
        return aaxBufferProcessWaveform(ptr,f,t,r,p);
    }
    inline bool process(float f, enum aaxWaveformType t,
                        enum aaxProcessingType p, float r=0.5f) {
        return aaxBufferProcessWaveform(ptr,f,t,r,p);
    }

    // ** support ******
    Buffer& operator=(Buffer o) {
        swap(*this, o);
        return *this;
    }
};


class dsp : public Obj
{
public:
    dsp() : Obj(), filter(true), fetype(0) {}

    dsp(aaxConfig c, enum aaxFilterType f) :
        Obj(c,aaxFilterDestroy), filter(true), fetype(f) {
        if (!aaxIsValid(c, AAX_FILTER)) ptr = aaxFilterCreate(c,f);
    }

    dsp(aaxConfig c, enum aaxEffectType e) :
        Obj(c,aaxEffectDestroy), filter(false), fetype(e) {
        if (!aaxIsValid(c, AAX_EFFECT)) ptr = aaxEffectCreate(c,e);
    }

    dsp(const dsp& o) : Obj(o), filter(o.filter), fetype(o.fetype) {}

    ~dsp() {}

    bool set(int s) {
        return (filter) ? !!aaxFilterSetState(ptr,s)
                        : !!aaxEffectSetState(ptr,s);
    }

    int state() {
        return (filter) ? aaxFilterGetState(ptr) : aaxEffectGetState(ptr);
    }

    bool set(unsigned s, float p1, float p2, float p3, float p4, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlot(ptr,s,t,p1,p2,p3,p4)
                        : aaxEffectSetSlot(ptr,s,t,p1,p2,p3,p4);
    }
    bool get(unsigned s, float* p1, float* p2, float* p3, float* p4, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterGetSlot(ptr,s,t,p1,p2,p3,p4)
                        : aaxEffectGetSlot(ptr,s,t,p1,p2,p3,p4);
    }
    bool set(unsigned s, Vector& v, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlotParams(ptr,s,t,v)
                    : aaxEffectSetSlotParams(ptr,s,t,v);
    }
    bool set(unsigned s, const aaxVec4f v, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlotParams(ptr,s,t,v)
                        : aaxEffectSetSlotParams(ptr,s,t,v);
    }
    bool get(unsigned s, aaxVec4f v, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterGetSlotParams(ptr,s,t,v)
                        : aaxEffectGetSlotParams(ptr,s,t,v);
    }
    bool set(enum aaxParameter p, float v, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetParam(ptr,p,t,v)
                        : aaxEffectSetParam(ptr,p,t,v);
    }
    float get(enum aaxParameter p, enum aaxType t=AAX_LINEAR) {
        return (filter) ? aaxFilterGetParam(ptr,p,t) : aaxEffectGetParam(ptr,p,t);
    }

    // ** support ******
    friend void swap(dsp& o1, dsp& o2) {
        swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        std::swap(o1.filter, o2.filter);
        std::swap(o1.fetype, o2.fetype);
    }
    dsp& operator=(dsp o) {
        swap(*this, o);
        return *this;
    }

    inline int type() {
        return fetype;
    }
    inline bool is_filter() {
        return filter;
    }

private:
    bool filter;
    int fetype;
};


class Emitter : public Obj
{
public:
    Emitter() : Obj() {}

    Emitter(enum aaxEmitterMode m) : Obj(aaxEmitterCreate(), aaxEmitterDestroy){
        aaxEmitterSetMode(ptr, AAX_POSITION, m);
    }

    Emitter(const Emitter& o) : Obj(o) {}

    ~Emitter() {}

    inline bool set(enum aaxModeType t, int m) {
        return aaxEmitterSetMode(ptr,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxEmitterGetMode(ptr,t);
    }
    inline bool set(enum aaxState s) {
        return aaxEmitterSetState(ptr,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxEmitterGetState(ptr));
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        return dsp.is_filter() ? aaxEmitterSetFilter(ptr,dsp)
                               : aaxEmitterSetEffect(ptr,dsp);
    }
    inline dsp get(enum aaxFilterType f) {
        return dsp(aaxEmitterGetFilter(ptr,f),f);
    }
    inline dsp get(enum aaxEffectType e) {
        return dsp(aaxEmitterGetEffect(ptr,e),e);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix64& m) {
        return aaxEmitterSetMatrix64(ptr,m);
    }
    inline bool get(Matrix64& m) {
        return aaxEmitterGetMatrix64(ptr,m);
    }
    inline bool velocity(Vector& v) {
        return aaxEmitterSetVelocity(ptr,v);
    }
    inline bool get(Vector& v) {
        return aaxEmitterGetVelocity(ptr,v);
    }

    // ** buffer handling ******
    inline bool add(Buffer& b) {
        return aaxEmitterAddBuffer(ptr,b);
    }
    inline bool remove_buffer() {
        return aaxEmitterRemoveBuffer(ptr);
    }
    inline Buffer get(unsigned int p, bool c=false) {
        return Buffer(aaxEmitterGetBufferByPos(ptr,p,c),~c);
    }
    inline unsigned int get(enum aaxState s) {
        return aaxEmitterGetNoBuffers(ptr,s);
    }
    inline bool offset(unsigned long o, enum aaxType t) {
        return aaxEmitterSetOffset(ptr,o,t);
    }
    inline bool offset(float o) {
        return aaxEmitterSetOffsetSec(ptr,o);
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxEmitterGetOffset(ptr,t);
    }
    inline float offset() {
        return aaxEmitterGetOffsetSec(ptr);
    }

    // ** support ******
    Emitter& operator=(Emitter o) {
        swap(*this, o);
        return *this;
    }
};


class Sensor : public Obj
{
public:
    Sensor() : mode(AAX_MODE_READ) {}

    Sensor(aaxConfig c, enum aaxRenderMode m=AAX_MODE_READ) :
        Obj(c, aaxDriverDestroy), mode(m) {}

    Sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(aaxDriverOpenByName(n,m), m) {}

    Sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(s.empty() ? NULL : s.c_str(),m) {}

    Sensor(const Sensor& o) : Obj(o), mode(o.mode) {}

    ~Sensor() {}

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxMixerSetSetup(ptr,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxMixerGetSetup(ptr,t);
    }

    inline bool set(enum aaxState s) {
        return aaxMixerSetState(ptr,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxMixerGetState(ptr));
    }

    // ** driver ******
    inline bool get(enum aaxRenderMode m) {
        return aaxDriverGetSupport(ptr,m);
    }
    inline const char* info(enum aaxSetupType t) {
        return aaxDriverGetSetup(ptr,t);
    }
    inline const char* info(enum aaxFilterType f) {
        return aaxFilterGetNameByType(ptr,f);
    }
    inline const char* info(enum aaxEffectType e) {
        return aaxEffectGetNameByType(ptr,e);
    }
    bool supports(enum aaxFilterType f) {
        return aaxIsFilterSupported(ptr, aaxFilterGetNameByType(ptr,f));
    }
    bool supports(enum aaxEffectType e) {
        return aaxIsEffectSupported(ptr, aaxEffectGetNameByType(ptr,e));
    }
    bool supports(const char* fe) {
        return aaxIsFilterSupported(ptr,fe) ? true : aaxIsEffectSupported(ptr,fe);
    }
    inline bool supports(std::string& s) {
        return supports(s.c_str());
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        return dsp.is_filter()
               ? (scenery_filter(dsp.type()) ? aaxScenerySetFilter(ptr,dsp)
                                             : aaxMixerSetFilter(ptr,dsp))
               : (scenery_effect(dsp.type()) ? aaxScenerySetEffect(ptr,dsp)
                                             : aaxMixerSetEffect(ptr,dsp));
    }
    dsp get(enum aaxFilterType t) {
        aaxFilter f = scenery_filter(t) ? aaxSceneryGetFilter(ptr,t)
                                        : aaxMixerGetFilter(ptr,t);
        return dsp(f,t);
    }
    dsp get(enum aaxEffectType t) {
        aaxEffect e = scenery_effect(t) ? aaxSceneryGetEffect(ptr,t)
                                        : aaxMixerGetEffect(ptr,t);
        return dsp(e,t);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix64& m) {
        return aaxSensorSetMatrix64(ptr,m);
    }
    inline bool get(Matrix64& m) {
        return aaxSensorGetMatrix64(ptr,m);
    }
    inline bool velocity(Vector& v) {
        return aaxSensorSetVelocity(ptr,v);
    }
    inline bool get(Vector& v) {
        return aaxSensorGetVelocity(ptr,v);
    }
    inline bool sensor(enum aaxState s) {
        return aaxSensorSetState(ptr,s);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxSensorWaitForBuffer(ptr,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxSensorGetBuffer(ptr));
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(ptr,t);
    }

    // ** support ******
    inline const char* version() {
        return aaxGetVersionString(ptr);
    }

    friend void swap(Sensor& o1, Sensor& o2) {
        swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        std::swap(o1.mode, o2.mode);
    }
    Sensor& operator=(Sensor o) {
        swap(*this, o);
        return *this;
    }

protected:
    enum aaxRenderMode mode;

private:
    inline bool scenery_filter(enum aaxFilterType type) {
        return (type == AAX_DISTANCE_FILTER || type == AAX_FREQUENCY_FILTER);
    }
    inline bool scenery_filter(int type) {
        return scenery_filter(aaxFilterType(type));
    }
    inline bool scenery_effect(enum aaxEffectType type) {
        return (type == AAX_VELOCITY_EFFECT);
    }
    inline bool scenery_effect(int type) {
        return scenery_effect(aaxEffectType(type));
    }
};


class Frame : public Obj
{
public:
    Frame() {}

    Frame(aaxConfig c) : Obj(aaxAudioFrameCreate(c), aaxAudioFrameDestroy) {}

    Frame(const Frame& o) : Obj(o),
        frames(o.frames), sensors(o.sensors), emitters(o.emitters) {}

    ~Frame() {
        for (size_t i=0; i<frames.size(); ++i) {
             aaxAudioFrameDeregisterAudioFrame(ptr,frames[i]);
        }
        frames.clear();
        for (size_t i=0; i<sensors.size(); ++i) {
             aaxAudioFrameDeregisterSensor(ptr,sensors[i]);
        }
        sensors.clear();
        for (size_t i=0; i<emitters.size(); ++i) {
             aaxAudioFrameDeregisterEmitter(ptr,emitters[i]);
        }
        emitters.clear();
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxAudioFrameSetSetup(ptr,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxAudioFrameGetSetup(ptr,t);
    }

    inline bool set(enum aaxState s) {
        return aaxAudioFrameSetState(ptr,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxAudioFrameGetState(ptr));
    }
    inline bool set(enum aaxModeType t, int m) {
        return aaxAudioFrameSetMode(ptr,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxAudioFrameGetMode(ptr,t);
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        return dsp.is_filter() ? aaxAudioFrameSetFilter(ptr,dsp)
                               : aaxAudioFrameSetEffect(ptr,dsp);
    }
    inline dsp get(enum aaxFilterType t) {
        return dsp(aaxAudioFrameGetFilter(ptr,t),t);
    }
    inline dsp get(enum aaxEffectType t) {
        return dsp(aaxAudioFrameGetEffect(ptr,t),t);
    }

    // ** sub-mixing ******
    bool add(Frame& m) {
        frames.push_back(m);
        return aaxAudioFrameRegisterAudioFrame(ptr,m);
    }
    bool remove(Frame& m) {
        frame_it fi = std::find(frames.begin(),frames.end(),m);
        if (fi != frames.end()) frames.erase(fi);
        return aaxAudioFrameDeregisterAudioFrame(ptr,m);
    }
    bool add(Sensor& s) {
        sensors.push_back(s);
        return aaxAudioFrameRegisterSensor(ptr,s);
    }
    bool remove(Sensor& s) {
        sensor_it si = std::find(sensors.begin(),sensors.end(),s);
        if (si != sensors.end()) sensors.erase(si);
        return aaxAudioFrameDeregisterSensor(ptr,s);
    }
    bool add(Emitter& e) {
        emitters.push_back(e);
        return aaxAudioFrameRegisterEmitter(ptr,e);
    }
    bool remove(Emitter& e) {
        emitter_it ei = std::find(emitters.begin(),emitters.end(),e);
        if (ei != emitters.end()) emitters.erase(ei);
        return aaxAudioFrameDeregisterEmitter(ptr,e);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix64& m) {
        return aaxAudioFrameSetMatrix64(ptr,m);
    }
    inline bool get(Matrix64& m) {
        return aaxAudioFrameGetMatrix64(ptr,m);
    }
    inline bool velocity(Vector& v) {
        return aaxAudioFrameSetVelocity(ptr,v);
    }
    inline bool get(Vector& v) {
        return aaxAudioFrameGetVelocity(ptr,v);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxAudioFrameWaitForBuffer(ptr,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxAudioFrameGetBuffer(ptr));
    }

    // ** support ******
    friend void swap(Frame& o1, Frame& o2) {
        swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        o1.frames.swap(o2.frames);
        o1.sensors.swap(o2.sensors);
        o1.emitters.swap(o2.emitters);
    }

    Frame& operator=(Frame o) {
        swap(*this, o);
        return *this;
    }

private:
    std::vector<aaxFrame> frames;
    typedef std::vector<aaxFrame>::iterator frame_it;

    std::vector<aaxConfig> sensors;
    typedef std::vector<aaxConfig>::iterator sensor_it;

    std::vector<aaxEmitter> emitters;
    typedef std::vector<aaxEmitter>::iterator emitter_it;
};
typedef Frame Mixer;


class AeonWave : public Sensor
{
public:
    AeonWave() : Sensor(), _ec(0) { std::fill(_e, _e+3, 0); }

    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(n,m), _ec(0) { std::fill(_e, _e+3, 0); }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m) :
        AeonWave(0, m) {}

    AeonWave(const AeonWave& o) : Sensor(o),
        frames(o.frames), sensors(o.sensors), emitters(o.emitters),
        buffers(o.buffers) {}

    ~AeonWave() {
        for (size_t i=0; i<frames.size(); ++i) {
             aaxMixerDeregisterAudioFrame(ptr,frames[i]);
        }
        frames.clear();
        for (size_t i=0; i<sensors.size(); ++i) {
             aaxMixerDeregisterSensor(ptr,sensors[i]);
        }
        sensors.clear();
        for (size_t i=0; i<emitters.size(); ++i) {
             aaxMixerDeregisterEmitter(ptr,emitters[i]);
        }
        emitters.clear();
        for(buffer_it it=buffers.begin(); it!=buffers.end(); it++){
             aaxBufferDestroy(it->second.second); buffers.erase(it);
        }
    }

    // ** position and orientation ******
    inline bool sensor_matrix(Matrix64& m) {
        return matrix(m);
    }
    inline bool sensor_velocity(Vector& v) {
        return velocity(v);
    }

    // ** enumeration ******
    const char* drivers(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        aaxDriverDestroy(_ec);
        _em = m; _ec = 0; _e[1] = 0; _e[2] = 0;
        if (_e[0] < aaxDriverGetCount(_em)) {
            _ec = aaxDriverGetByPos(_e[0]++,_em);
        }  else _e[0] = 0;
        return aaxDriverGetSetup(_ec,AAX_DRIVER_STRING);
    }
    const char* devices(bool c_str=false) {
        _e[2] = 0; _ed = c_str ? 0 : (_e[1] ? 0 : "");
        if (_e[1]++ < aaxDriverGetDeviceCount(_ec,_em)) {
            _ed = aaxDriverGetDeviceNameByPos(_ec,_e[1]-1,_em);
        }
        return _ed;
    }
    const char* interfaces(bool c_str=false) {
        const char *ifs = c_str ? 0 : (_e[2] ? 0 : "");
        if (_e[2]++ < aaxDriverGetInterfaceCount(_ec,_ed,_em)) {
            ifs = aaxDriverGetInterfaceNameByPos(_ec,_ed,_e[2]-1,_em);
        }
        return ifs;
    }

    inline const char* device(unsigned d) {
        return aaxDriverGetDeviceNameByPos(ptr,d,mode);
    }
    const char* interface_name(int d, unsigned i) {
        const char* ed = device(d);
        return aaxDriverGetInterfaceNameByPos(ptr,ed,i,mode);
    }
    inline const char* interface_name(const char* d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(ptr,d,i,mode);
    }
    inline const char* interface_name(std::string& d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(ptr,d.c_str(),i,mode);
    }

    // ** support ******
    inline enum aaxErrorType error_no() {
        return aaxDriverGetErrorNo(ptr);
    }
    inline  const char* strerror() {
        return aaxGetErrorString(error_no());
    }

    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(ptr,t);
    }

    friend void swap(AeonWave& o1, AeonWave& o2) {
        swap(static_cast<Sensor&>(o1), static_cast<Sensor&>(o2));
        o1.frames.swap(o2.frames);
        o1.sensors.swap(o2.sensors);
        o1.emitters.swap(o2.emitters);
        o1.buffers.swap(o2.buffers);
        std::swap(o1.play, o2.play);
    }

    AeonWave& operator=(AeonWave o) {
        swap(*this, o);
        return *this;
    }

    // ** mixing ******
    bool add(Frame& m) {
        frames.push_back(m);
        return aaxMixerRegisterAudioFrame(ptr,m);
    }
    bool remove(Frame& m) {
        frame_it fi = std::find(frames.begin(),frames.end(),m);
        if (fi != frames.end()) frames.erase(fi);
        return aaxMixerDeregisterAudioFrame(ptr,m);
    }
    bool add(Sensor& s) {
        sensors.push_back(s);
        return aaxMixerRegisterSensor(ptr,s);
    }
    bool remove(Sensor& s) {
        sensor_it si = std::find(sensors.begin(),sensors.end(),s);
        if (si != sensors.end()) sensors.erase(si);
        return aaxMixerDeregisterSensor(ptr,s);
    }
    bool add(Emitter& e) {
        emitters.push_back(e);
        return aaxMixerRegisterEmitter(ptr,e);
    }
    bool remove(Emitter& e) {
        emitter_it ei = std::find(emitters.begin(),emitters.end(),e);
        if (ei != emitters.end()) emitters.erase(ei);
        return aaxMixerDeregisterEmitter(ptr,e);
    }

    // ** buffer management ******
    // Get a shared buffer from the buffer cache if it's name is already
    // in the cache. Otherwise create a new one and add it to the cache.
    // The name can be an URL or a path to a file or just a reference-id.
    // In the case of an RUL of a path the data is read automatically,
    // otherwise the application should add the audio-data itself.
    Buffer& buffer(std::string name) {
        buffer_it it = buffers.find(name);
        if (it == buffers.end()) {
            std::pair<buffer_it,bool> ret = buffers.insert(std::make_pair(name,std::make_pair(static_cast<size_t>(0),Buffer(aaxBufferReadFromStream(ptr,name.c_str()),false))));
            it = ret.first;
        }
        it->second.first++;
        return it->second.second;
    }
    void destroy(Buffer& b) {
        for(buffer_it it=buffers.begin(); it!=buffers.end(); it++)
        {
            if (it->second.second == b && !(--it->second.first)) {
                aaxBufferDestroy(it->second.second);
                buffers.erase(it);
            }
        }
    }

    // ** handles for a single background music stream ******
    // The name can be an URL or a path to a file.
    bool playback(std::string name) {
        std::string devname = std::string("AeonWave on Audio Files: ")+name;
        play = Sensor(devname, AAX_MODE_READ);
        return add(play) ? (play.set(AAX_INITIALIZED) ? play.sensor(AAX_CAPTURING) : false) : false;
    }

    bool stop() {
        return play.set(AAX_STOPPED);
    }

    bool playing() {
        return (play.state() == AAX_PLAYING);
    }

    float offset() {
        return (float)play.offset(AAX_SAMPLES)/(float)play.get(AAX_FREQUENCY);
    }

private:
    std::vector<aaxFrame> frames;
    typedef std::vector<aaxFrame>::iterator frame_it;

    std::vector<aaxConfig> sensors;
    typedef std::vector<aaxConfig>::iterator sensor_it;

    std::vector<aaxEmitter> emitters;
    typedef std::vector<aaxEmitter>::iterator emitter_it;

    std::map<std::string,std::pair<size_t,Buffer> > buffers;
    typedef std::map<std::string,std::pair<size_t,Buffer> >::iterator buffer_it;

    // background music stream
    Sensor play;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

} // namespace aax

#endif

