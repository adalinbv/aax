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

#ifndef AEONWAVE
#define AEONWAVE 1

#include <map>
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

inline void free(void *ptr) {
    aaxFree(ptr);
}

inline unsigned no_bits(enum aaxFormat fmt) {
    return aaxGetBitsPerSample(fmt);
}
inline unsigned no_bytes(enum aaxFormat fmt) {
    return aaxGetBytesPerSample(fmt);
}

inline void clear_error() {
    aaxGetErrorNo();
}
inline enum aaxErrorType error_no() {
    return aaxGetErrorNo();
}
inline const char* error(enum aaxErrorType e=aaxGetErrorNo()) {
    return aaxGetErrorString(e);
}

inline bool valid(void* c, enum aaxHandleType t=AAX_CONFIG) {
    return aaxIsValid(c,t);
}

class Obj
{
public:
    typedef int close_fn(void*);

    Obj() : ptr(0), close(0) {}

    Obj(void *p, const close_fn* c) : ptr(p), close(c) {}

    Obj(const Obj& o) : close(o.close), ptr(o.ptr) {}

    ~Obj() {
        if (close) close(ptr);
    }

    virtual void swap(Obj& o) {
        std::swap(ptr, o.ptr);
        std::swap(close, o.close);
    }

    operator void*() const {
        return ptr;
    }

    explicit operator bool() {
        return !!ptr;
    }

protected:
    mutable close_fn* close;
    void* ptr;
};

class Buffer : public Obj
{
public:
    Buffer() : Obj() {}

    Buffer(aaxBuffer b, bool o=true) :
        Obj(b, o ? aaxBufferDestroy : 0) {}

    Buffer(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) :
        Obj(aaxBufferCreate(c,n,t,f), aaxBufferDestroy) {}

    ~Buffer() {}

    inline set(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) {
        ptr = aaxBufferCreate(c,n,t,f); close = aaxBufferDestroy;
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
        swap(o);
        return *this;
    }
};


class dsp : public Obj
{
public:
    dsp() : Obj(), filter(true) {}

    dsp(aaxFilter c, enum aaxFilterType f) :
        Obj(c,aaxFilterDestroy), filter(true) {
        if (!aaxIsValid(c, AAX_FILTER)) ptr = aaxFilterCreate(c,f);
    }

    dsp(aaxEffect c, enum aaxEffectType e) :
        Obj(c,aaxEffectDestroy), filter(false) {
        if (!aaxIsValid(c, AAX_EFFECT)) ptr = aaxEffectCreate(c,e);
    }

    dsp(const dsp& o) : Obj(o), filter(o.filter) {}

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
    void swap(dsp& o) {
        std::swap(filter, o.filter);
        std::swap(ptr, o.ptr);
        std::swap(close, o.close);
    }
    dsp& operator=(dsp o) {
        swap(o);
        return *this;
    }

    inline bool is_filter() {
        return filter;
    }

private:
    bool filter;
};


class Emitter : public Obj
{
public:
    Emitter() : Obj(aaxEmitterCreate(), aaxEmitterDestroy) {}

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
        return Buffer(aaxEmitterGetBufferByPos(ptr,p,c),!c);
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
        swap(o);
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

    bool destroy() {
        bool rv = close ? close(ptr) : true;
        ptr = 0; close = 0;
        return rv;
    }

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
        int res = dsp.is_filter() ? aaxMixerSetFilter(ptr,dsp)
                                  : aaxMixerSetEffect(ptr,dsp);
        if (!res) { clear_error();
            res = dsp.is_filter() ? aaxScenerySetFilter(ptr,dsp)
                                  : aaxScenerySetEffect(ptr,dsp); }
        return res;
    }
    dsp get(enum aaxFilterType t) {
        aaxFilter f = aaxMixerGetFilter(ptr,t);
        if (!f) { clear_error(); f = aaxSceneryGetFilter(ptr,t); }
        return dsp(f,t);
    }
    dsp get(enum aaxEffectType t) {
        aaxEffect e = aaxMixerGetEffect(ptr,t);
        if (!e) { clear_error(); e = aaxSceneryGetEffect(ptr,t); }
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

    void swap(Sensor& o) {
        std::swap(mode, o.mode);
        std::swap(ptr, o.ptr);
        std::swap(close, o.close);
    }
    Sensor& operator=(Sensor o) {
        swap(o);
        return *this;
    }

protected:
    enum aaxRenderMode mode;
};


class Frame : public Obj
{
public:
    Frame() {}

    Frame(aaxConfig c) : Obj(aaxAudioFrameCreate(c), aaxAudioFrameDestroy) {}

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
        swap(o);
        return *this;
    }
};
typedef Frame Mixer;


class AeonWave : public Sensor
{
public:
    AeonWave() : Sensor(), _ec(0) { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(n,m), _ec(0) { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m) :
        AeonWave(NULL,m) {}

    ~AeonWave() {
        for(_buffer_it it=_buffer_cache.begin(); it!=_buffer_cache.end(); it++){
             aaxBufferDestroy(it->second.second); _buffer_cache.erase(it);
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
    inline bool valid(enum aaxHandleType t) {
        return aaxIsValid(ptr,t);
    }

    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(ptr,t);
    }

    AeonWave& operator=(AeonWave o) {
        swap(o);
        return *this;
    }

    // ** mixing ******
    inline bool add(Frame& m) {
        return aaxMixerRegisterAudioFrame(ptr,m);
    }
    inline bool remove(Frame& m) {
        return aaxMixerDeregisterAudioFrame(ptr,m);
    }
    inline bool add(Sensor& s) {
        return aaxMixerRegisterSensor(ptr,s);
    }
    inline bool remove(Sensor& s) {
        return aaxMixerDeregisterSensor(ptr,s);
    }
    inline bool add(Emitter& e) {
        return aaxMixerRegisterEmitter(ptr,e);
    }
    inline bool remove(Emitter& e) {
        return aaxMixerDeregisterEmitter(ptr,e);
    }

    // ** buffer management ******
    // Get a shared buffer from the buffer cache if it's URL is already
    // in the cache. Otherwise create a new one and add it to the cache.
    Buffer& buffer(std::string name) {
        _buffer_it it = _buffer_cache.find(name);
        if (it == _buffer_cache.end()) {
            std::pair<_buffer_it,bool> ret = _buffer_cache.insert(std::make_pair(name,std::make_pair(static_cast<size_t>(0),Buffer(aaxBufferReadFromStream(ptr,name.c_str()),false))));
            it = ret.first;
        }
        it->second.first++;
        return it->second.second;
    }
    void destroy(Buffer& b) {
        for(_buffer_it it=_buffer_cache.begin(); it!=_buffer_cache.end(); it++)
        {
            if (it->second.second == b && !(--it->second.first)) {
                aaxBufferDestroy(it->second.second);
                _buffer_cache.erase(it);
            }
        }
    }

    // ** handles for a single background music stream ******
    bool playback(std::string f) {
        std::string devname = std::string("AeonWave on Audio Files: ")+f;
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
    std::map<std::string,std::pair<size_t,Buffer> > _buffer_cache;
    typedef std::map<std::string,std::pair<size_t,Buffer> >::iterator _buffer_it;

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

