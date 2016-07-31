/*
 * Copyright (C) 2015-2016 by Erik Hofman.
 * Copyright (C) 2015-2016 by Adalin B.V.
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

    inline set(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) {
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

    dsp(aaxFilter c, enum aaxFilterType f) :
        Obj(c,aaxFilterDestroy), filter(true), fetype(f) {
        if (!aaxIsValid(c, AAX_FILTER)) ptr = aaxFilterCreate(c,f);
    }

    dsp(aaxEffect c, enum aaxEffectType e) :
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

    bool set(unsigned s, float p1, float p2, float p3, float p4, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlot(ptr,s,t,p1,p2,p3,p4)
                        : aaxEffectSetSlot(ptr,s,t,p1,p2,p3,p4);
    }
    bool get(unsigned s, float* p1, float* p2, float* p3, float* p4, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterGetSlot(ptr,s,t,p1,p2,p3,p4)
                        : aaxEffectGetSlot(ptr,s,t,p1,p2,p3,p4);
    }
    bool set(unsigned s, Vector& v, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlotParams(ptr,s,t,v)
                    : aaxEffectSetSlotParams(ptr,s,t,v);
    }
    bool set(unsigned s, const aaxVec4f v, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetSlotParams(ptr,s,t,v)
                        : aaxEffectSetSlotParams(ptr,s,t,v);
    }
    bool get(unsigned s, aaxVec4f v, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterGetSlotParams(ptr,s,t,v)
                        : aaxEffectGetSlotParams(ptr,s,t,v);
    }
    bool set(int p, float v, int t=AAX_LINEAR) {
        return (filter) ? aaxFilterSetParam(ptr,p,t,v)
                        : aaxEffectSetParam(ptr,p,t,v);
    }
    float get(int p, int t=AAX_LINEAR) {
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
    inline bool matrix(Matrix& m) {
        return aaxEmitterSetMatrix(ptr,m);
    }
    inline bool get(Matrix& m) {
        return aaxEmitterGetMatrix(ptr,m);
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
    inline Buffer get(unsigned int p, int c=AAX_FALSE) {
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
    inline bool matrix(Matrix& m) {
        return aaxSensorSetMatrix(ptr,m);
    }
    inline bool get(Matrix& m) {
        return aaxSensorGetMatrix(ptr,m);
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

    Frame(const Frame& o) : Obj(o) {}

    ~Frame() {}

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
        return aaxAudioFrameRegisterAudioFrame(ptr,m);
    }
    bool remove(Frame& m) {
        return aaxAudioFrameDeregisterAudioFrame(ptr,m);
    }
    bool add(Sensor& s) {
        return aaxAudioFrameRegisterSensor(ptr,s);
    }
    bool remove(Sensor& s) {
        return aaxAudioFrameDeregisterSensor(ptr,s);
    }
    bool add(Emitter& e) {
        return aaxAudioFrameRegisterEmitter(ptr,e);
    }
    bool remove(Emitter& e) {
        return aaxAudioFrameDeregisterEmitter(ptr,e);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix& m) {
        return aaxAudioFrameSetMatrix(ptr,m);
    }
    inline bool get(Matrix& m) {
        return aaxAudioFrameGetMatrix(ptr,m);
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
    Frame& operator=(Frame o) {
        swap(*this, o);
        return *this;
    }
};
typedef Frame Mixer;


class AeonWave : public Sensor
{
public:
    AeonWave() : Sensor(), _ec(0) { std::fill(_e, _e+3, 0); }

    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(n,m), _ec(0) { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m) :
        AeonWave(NULL,m) {}

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
    inline bool sensor_matrix(Matrix& m) {
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
    const char* interface(int d, unsigned i) {
        const char* ed = device(d);
        return aaxDriverGetInterfaceNameByPos(ptr,ed,i,mode);
    }
    inline const char* interface(const char* d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(ptr,d,i,mode);
    }
    inline const char* interface(std::string& d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(ptr,d.c_str(),i,mode);
    }

    // ** support ******
    inline enum aaxErrorType error_no() {
        return aaxDriverGetErrorNo(ptr);
    }
    inline const char* strerror() {
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

inline std::string to_string(enum aaxHandleType handle)
{
    switch(handle)
    {
    case AAX_CONFIG: return "audio driver";
    case AAX_BUFFER: return "audio buffer";
    case AAX_EMITTER: return "sound emitter";
    case AAX_AUDIOFRAME: return "audio frame";
    case AAX_FILTER: return "sound filter";
    case AAX_EFFECT: return "sound effect";
    case AAX_CONFIG_HD: return "HD audio driver";
    }
}

inline std::string to_string(enum aaxFormat fmt)
{
    switch(fmt)
    {
    case AAX_PCM8S: return "signed, 8-bits per sample";
    case AAX_PCM16S: return "signed, 16-bits per sample";
    case AAX_PCM24S: return "signed, 24-bits per sample 32-bit encoded";
    case AAX_PCM32S: return "signed, 32-bits per sample";
    case AAX_FLOAT: return "32-bit floating point, -1.0 to 1.0";
    case AAX_DOUBLE: return "64-bit floating point, -1.0 to 1.0";
    case AAX_MULAW: return "16-bit compressed to 8-bit µ-law";
    case AAX_ALAW: return "16-bit compresed to -bit A-law";
    case AAX_IMA4_ADPCM: return "16-bit compressed to 4-bit addaptive differential";
    case AAX_PCM8U: return "unsigned, 8-bits per sample";
    case AAX_PCM16U: return "unsigned, 16-bits per sample";
    case AAX_PCM24U: return "unsigned, 24-bits per sample 32-bit encoded";
    case AAX_PCM32U: return "unsigned, 32-bits per sample";
    case AAX_PCM16S_LE: return "signed, 16-bits per sample little-endian";
    case AAX_PCM24S_LE: return "signed, 24-bits per sample little-endian 32-bit encoded";
    case AAX_PCM32S_LE: return "signed, 32-bits per sample little-endian";
    case AAX_FLOAT_LE: return "32-bit floating point, -1.0 to 1.0 little-endian";
    case AAX_DOUBLE_LE: return "64-bit floating point, -1.0 to 1.0 little-endian";
    case AAX_PCM16U_LE: return "unsigned, 16-bits per sample little-endian";
    case AAX_PCM24U_LE: return "unsigned, 24-bits per sample little-endian 32-bit encoded";
    case AAX_PCM32U_LE: return "unsigned, 32-bits per sample little-endian";
    case AAX_PCM16S_BE: return "signed, 16-bits per sample big-endian";
    case AAX_PCM24S_BE: return "signed, 24-bits per sample big-endian 32-bit encoded";
    case AAX_PCM32S_BE: return "signed, 32-bits per sample big-endian";
    case AAX_FLOAT_BE: return "32-bit floating point, -1.0 to 1.0 big-endian";
    case AAX_DOUBLE_BE: return "64-bit floating point, -1.0 to 1.0 big-endian";
    case AAX_PCM16U_BE: return "unsigned, 16-bits per sample big-endian";
    case AAX_PCM24U_BE: return "unsigned, 24-bits per sample big-endian 32-bit encoded";
    case AAX_PCM32U_BE: return "unsigned, 32-bits per sample big-endian";
    case AAX_AAXS16S: return "16-bit XML encoded waveform synthesizer";
    case AAX_AAXS24S: return "24-bit XML encoded waveform synthesizer";
    }
}

inline std::string to_string(enum aaxType type)
{
    switch(type)
    {
    case AAX_LINEAR: return "linear";
    case AAX_LOGARITHMIC:
    case AAX_DECIBEL: return "Decibels";
    case AAX_RADIANS: return "radians";
    case AAX_DEGREES: return "degrees";
    case AAX_BYTES: return "bytes";
    case AAX_FRAMES: return "frames";
    case AAX_SAMPLES: return "samples";
    case AAX_MICROSECONDS: return "µ-seconds";
    }
}

inline std::string to_string(enum aaxModeType mode)
{
    switch(mode)
    {
    case AAX_POSITION: return "position";
    case AAX_LOOPING: return "looping";
    case AAX_BUFFER_TRACK: return "buffer track";
    }
}

inline std::string to_string(enum aaxTrackType type)
{
    switch(type)
    {
    case AAX_TRACK_MIX: return "mix tracks";
    case AAX_TRACK_ALL: return "all tracks";
    case AAX_TRACK0:
    case AAX_TRACK_LEFT:
    case AAX_TRACK_FRONT_LEFT: return "front-left";
    case AAX_TRACK1:
    case AAX_TRACK_RIGHT:
    case AAX_TRACK_FRONT_RIGHT: return "front-right";
    case AAX_TRACK2:
    case AAX_TRACK_REAR_LEFT: return "rear-left";
    case AAX_TRACK3:
    case AAX_TRACK_REAR_RIGHT: return "rear-right";
    case AAX_TRACK4:
    case AAX_TRACK_CENTER:
    case AAX_TRACK_CENTER_FRONT: reutn "center-front";
    case AAX_TRACK5:
    case AAX_TRACK_LFE:
    case AAX_TRACK_SUBWOOFER: "sub-woofer (low-frequency emitter)";
    case AAX_TRACK6:
    case AAX_TRACK_SIDE_LEFT: return "side-left";
    case AAX_TRACK7:
    case AAX_TRACK_SIDE_RIGHT: return "side-right";
    }
}

inline std::string to_string(enum aaxSetupType type)
{
    switch(type)
    {
    case AAX_NAME_STRING:
    case AAX_DRIVER_STRING: return "driver name string";
    case AAX_VERSION_STRING; return "version string";
    case AAX_RENDERER_STRING: return "renderer string";
    case AAX_VENDOR_STRING: return "vendor string";
    case AAX_FREQUENCY: return "frequency";
    case AAX_TRACKS: return "number of stracks";
    case AAX_FORMAT: return "audio format";
    case AAX_REFRESH_RATE:
    case AAX_REFRESHRATE: return "audio refresh rate";
    case AAX_TRACK_SIZE:
    case AAX_TRACKSIZE: return "track size";
    case AAX_NO_SAMPLES: return "number of samples";
    case AAX_LOOP_START: return "loop-start point";
    case AAX_LOOP_END: return "loop-end point";
    case AAX_MONO_EMITTERS:
    case AAX_MONO_SOURCES: return "number of mono emitters";
    case AAX_STEREO_EMITTERS:
    case AAX_STEREO_SOURCES: return "number of multi-track emitters";
    case AAX_BLOCK_ALIGNMENT: return "block-alignment";
    case AAX_AUDIO_FRAMES: return "number of audio frames";
    case AAX_UPDATE_RATE:
    case AAX_UPDATERATE: return "4D parameter update rate";
    case AAX_LATENCY: return "audio latency";
    case AAX_TRACK_LAYOUT: return "track layout";
    case AAX_BITRATE: return" bit-rate";
    case AAX_FRAME_TIMING: return "frame-timing";
    case AAX_PEAK_VALUE: return "track peak-value";
    case AAX_AVERAGE_VALUE: return "track average value";
    case AAX_COMPRESSION_VALUE: return "track compression-value";
    case AAX_GATE_ENABLED: return "noise-gate enabled";
    case AAX_SHARED_MODE: return "driver sared-mode";
    case AAX_TIMER_MODE: return "driver timer-mode";
    case AAX_BATCHED_MODE: return "driver non-realtime batched-mode";
    case AAX_SEEKABLE_SUPPORT: return "driver seekable support";
    case AAX_TRACKS_MIN: return "minimum number of supported playback channels";
    case AAX_TRACKS_MAX: return "maximum number of supported playback channels";
    case AAX_PERIODS_MIN: return "minimum number of supported driver periods";
    case AAX_PERIODS_MAX: return "maximum number of supported driver periods";
    case AAX_FREQUENCY_MIN: return "minimum supported playback frequency";
    case AAX_FREQUENCY_MAX: return "maximum supported playback frequency";
    case AAX_SAMPLES_MAX: return "maximum number of capture samples";
    case AAX_COVER_IMAGE_DATA: return "album cover art";
    case AAX_MUSIC_PERFORMER_STRING: return "music performer string";
    case AAX_MUSIC_GENRE_STRING: return "music genre string";
    case AAX_TRACK_TITLE_STRING: return "song title string";
    case AAX_TRACK_NUMBER_STRING: return "song track number string";
    case AAX_ALBUM_NAME_STRING: return "album name string";
    case AAX_RELEASE_DATE_STRING: return "song release date string";
    case AAX_SONG_COPYRIGHT_STRING: return "song copyright string";
    case AAX_SONG_COMPOSER_STRING: return "song composer string";
    case AAX_SONG_COMMENT_STRING: return "song comment string";
    case AAX_ORIGINAL_PERFORMER_STRING: return "song original performer string";
    case AAX_WEBSITE_STRING: return "artist website string";
    case AAX_MUSIC_PERFORMER_UPDATE: return "updated music performer string";
    case AAX_TRACK_TITLE_UPDATE: return "updated song title string";
    case AAX_RELEASE_MODE: return "release mode";
    }
}

} // namespace aax

#endif

