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

class Obj
{
public:
    typedef int _close_fn(void*);

    Obj() : _p(0), _close(0) {}

    Obj(void *p, const _close_fn* c) : _p(p), _close(c) {}

    ~Obj() {
        if (_close) _close(_p);
    }

    operator void*() const {
        return _p;
    }

    explicit operator bool() {
        return !!_p;
    }

protected:
    mutable _close_fn* _close;
    void* _p;
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
        _p = aaxBufferCreate(c,n,t,f); _close = aaxBufferDestroy;
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxBufferSetSetup(_p,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxBufferGetSetup(_p,t);
    }
    inline bool fill(const void* d) {
        return aaxBufferSetData(_p,d);
    }
    inline void** data() {
        return aaxBufferGetData(_p);
    }

    // ** buffer data mangling ******
    inline bool process(float f, enum aaxWaveformType t,
                        float r=0.5f, enum aaxProcessingType p=AAX_MIX) {
        return aaxBufferProcessWaveform(_p,f,t,r,p);
    }
    inline bool process(float f, enum aaxWaveformType t,
                        enum aaxProcessingType p, float r=0.5f) {
        return aaxBufferProcessWaveform(_p,f,t,r,p);
    }

    // ** support ******
    Buffer& operator=(const Buffer& o) {
        if (_close) _close(_p);
        _p = o._p; _close = o._close; o._close = 0;
        return *this;
    }
};


class dsp : public Obj
{
public:
    dsp() : Obj(), _f(true) {}

    dsp(aaxFilter c, enum aaxFilterType f) :
        Obj(c,aaxFilterDestroy), _f(true) {
        if (!aaxIsValid(c, AAX_FILTER)) _p = aaxFilterCreate(c,f);
    }

    dsp(aaxEffect c, enum aaxEffectType e) :
        Obj(c,aaxEffectDestroy), _f(false) {
        if (!aaxIsValid(c, AAX_EFFECT)) _p = aaxEffectCreate(c,e);
    }

    ~dsp() {}

    bool set(int s) {
        return (_f) ? !!aaxFilterSetState(_p,s)
                    : !!aaxEffectSetState(_p,s);
    }

    int state() {
        return (_f) ? aaxFilterGetState(_p) : aaxEffectGetState(_p);
    }

    bool set(unsigned s, float p1, float p2, float p3, float p4, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlot(_p,s,t,p1,p2,p3,p4)
                    : aaxEffectSetSlot(_p,s,t,p1,p2,p3,p4);
    }
    bool get(unsigned s, float* p1, float* p2, float* p3, float* p4, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetSlot(_p,s,t,p1,p2,p3,p4)
                    : aaxEffectGetSlot(_p,s,t,p1,p2,p3,p4);
    }
    bool set(unsigned s, Vector& v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlotParams(_p,s,t,v)
                    : aaxEffectSetSlotParams(_p,s,t,v);
    }
    bool set(unsigned s, const aaxVec4f v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlotParams(_p,s,t,v)
                    : aaxEffectSetSlotParams(_p,s,t,v);
    }
    bool get(unsigned s, aaxVec4f v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetSlotParams(_p,s,t,v)
                    : aaxEffectGetSlotParams(_p,s,t,v);
    }
    bool set(int p, float v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetParam(_p,p,t,v) : aaxEffectSetParam(_p,p,t,v);
    }
    float get(int p, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetParam(_p,p,t) : aaxEffectGetParam(_p,p,t);
    }

    // ** support ******
    dsp& operator=(const dsp& o) {
        if (_close) _close(_p);
        _p = o._p; _f = o._f; _close = o._close; o._close = 0;
        return *this;
    }

    inline bool is_filter() {
        return _f;
    }

private:
    bool _f;
};


class Emitter : public Obj
{
public:
    Emitter() : Obj(aaxEmitterCreate(), aaxEmitterDestroy) {}

    ~Emitter() {}

    inline bool set(enum aaxModeType t, int m) {
        return aaxEmitterSetMode(_p,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxEmitterGetMode(_p,t);
    }
    inline bool set(enum aaxState s) {
        return aaxEmitterSetState(_p,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxEmitterGetState(_p));
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        return dsp.is_filter() ? aaxEmitterSetFilter(_p,dsp)
                               : aaxEmitterSetEffect(_p,dsp);
    }
    inline dsp get(enum aaxFilterType f) {
        return dsp(aaxEmitterGetFilter(_p,f),f);
    }
    inline dsp get(enum aaxEffectType e) {
        return dsp(aaxEmitterGetEffect(_p,e),e);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix& m) {
        return aaxEmitterSetMatrix(_p,m);
    }
    inline bool get(Matrix& m) {
        return aaxEmitterGetMatrix(_p,m);
    }
    inline bool velocity(Vector& v) {
        return aaxEmitterSetVelocity(_p,v);
    }
    inline bool get(Vector& v) {
        return aaxEmitterGetVelocity(_p,v);
    }

    // ** buffer handling ******
    inline bool add(Buffer& b) {
        return aaxEmitterAddBuffer(_p,b);
    }
    inline bool remove_buffer() {
        return aaxEmitterRemoveBuffer(_p);
    }
    inline Buffer get(unsigned int p, int c=AAX_FALSE) {
        return Buffer(aaxEmitterGetBufferByPos(_p,p,c),!c);
    }
    inline unsigned int get(enum aaxState s) {
        return aaxEmitterGetNoBuffers(_p,s);
    }
    inline bool offset(unsigned long o, enum aaxType t) {
        return aaxEmitterSetOffset(_p,o,t);
    }
    inline bool offset(float o) {
        return aaxEmitterSetOffsetSec(_p,o);
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxEmitterGetOffset(_p,t);
    }
    inline float offset() {
        return aaxEmitterGetOffsetSec(_p);
    }

    // ** support ******
    Emitter& operator=(const Emitter& o) {
        if (_close) _close(_p);
        _p = o._p; _close = o._close; o._close = 0;
        return *this;
    }
};


class Sensor : public Obj
{
public:
    Sensor() : _m(AAX_MODE_READ) {}

    Sensor(aaxConfig c, enum aaxRenderMode m=AAX_MODE_READ) :
        Obj(c, aaxDriverDestroy), _m(m) {}

    Sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(aaxDriverOpenByName(n,m), m) {}

    Sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(s.empty() ? NULL : s.c_str(),m) {}

    ~Sensor() {}

    bool close() {
        bool rv = _close ? _close(_p) : true;
        _p = 0; _close = 0;
        return rv;
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxMixerSetSetup(_p,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxMixerGetSetup(_p,t);
    }

    inline bool set(enum aaxState s) {
        return aaxMixerSetState(_p,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxMixerGetState(_p));
    }

    // ** driver ******
    inline bool get(enum aaxRenderMode m) {
        return aaxDriverGetSupport(_p,m);
    }
    inline const char* info(enum aaxSetupType t) {
        return aaxDriverGetSetup(_p,t);
    }
    inline const char* info(enum aaxFilterType f) {
        return aaxFilterGetNameByType(_p,f);
    }
    inline const char* info(enum aaxEffectType e) {
        return aaxEffectGetNameByType(_p,e);
    }
    bool supports(enum aaxFilterType f) {
        return aaxIsFilterSupported(_p, aaxFilterGetNameByType(_p,f));
    }
    bool supports(enum aaxEffectType e) {
        return aaxIsEffectSupported(_p, aaxEffectGetNameByType(_p,e));
    }
    bool supports(const char* fe) {
        return aaxIsFilterSupported(_p,fe) ? true : aaxIsEffectSupported(_p,fe);
    }
    inline bool supports(std::string& s) {
        return supports(s.c_str());
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        int res = dsp.is_filter() ? aaxMixerSetFilter(_p,dsp)
                                  : aaxMixerSetEffect(_p,dsp);
        if (!res) { error_no();
            res = dsp.is_filter() ? aaxScenerySetFilter(_p,dsp)
                                  : aaxScenerySetEffect(_p,dsp); }
        return res;
    }
    dsp get(enum aaxFilterType t) {
        aaxFilter f = aaxMixerGetFilter(_p,t);
        if (!f) { error_no(); f = aaxSceneryGetFilter(_p,t); }
        return dsp(f,t);
    }
    dsp get(enum aaxEffectType t) {
        aaxEffect e = aaxMixerGetEffect(_p,t);
        if (!e) { error_no(); e = aaxSceneryGetEffect(_p,t); }
        return dsp(e,t);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix& m) {
        return aaxSensorSetMatrix(_p,m);
    }
    inline bool get(Matrix& m) {
        return aaxSensorGetMatrix(_p,m);
    }
    inline bool velocity(Vector& v) {
        return aaxSensorSetVelocity(_p,v);
    }
    inline bool get(Vector& v) {
        return aaxSensorGetVelocity(_p,v);
    }
    inline bool sensor(enum aaxState s) {
        return aaxSensorSetState(_p,s);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxSensorWaitForBuffer(_p,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxSensorGetBuffer(_p));
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(_p,t);
    }

    // ** support ******
    inline const char* version() {
        return aaxGetVersionString(_p);
    }
    static enum aaxErrorType error_no() {
        return aaxGetErrorNo();
    }
    static const char* error(enum aaxErrorType e=aaxGetErrorNo()) {
        return aaxGetErrorString(e);
    }

    Sensor& operator=(const Sensor& o) {
        aaxGetErrorNo();
        if (_close) _close(_p);
        _p = o._p; _m = o._m; _close = o._close; o._close = 0;
        return *this;
    }

protected:
    enum aaxRenderMode _m;
};


class Frame : public Obj
{
public:
    Frame() {}

    Frame(aaxConfig c) : Obj(aaxAudioFrameCreate(c), aaxAudioFrameDestroy) {}

    ~Frame() {}

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxAudioFrameSetSetup(_p,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxAudioFrameGetSetup(_p,t);
    }

    inline bool set(enum aaxState s) {
        return aaxAudioFrameSetState(_p,s);
    }
    inline enum aaxState state() {
        return aaxState(aaxAudioFrameGetState(_p));
    }
    inline bool set(enum aaxModeType t, int m) {
        return aaxAudioFrameSetMode(_p,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxAudioFrameGetMode(_p,t);
    }

    // ** filters and effects ******
    bool set(dsp& dsp) {
        return dsp.is_filter() ? aaxAudioFrameSetFilter(_p,dsp)
                               : aaxAudioFrameSetEffect(_p,dsp);
    }
    inline dsp get(enum aaxFilterType t) {
        return dsp(aaxAudioFrameGetFilter(_p,t),t);
    }
    inline dsp get(enum aaxEffectType t) {
        return dsp(aaxAudioFrameGetEffect(_p,t),t);
    }

    // ** sub-mixing ******
    bool add(Frame& m) {
        return aaxAudioFrameRegisterAudioFrame(_p,m);
    }
    bool remove(Frame& m) {
        return aaxAudioFrameDeregisterAudioFrame(_p,m);
    }
    bool add(Sensor& s) {
        return aaxAudioFrameRegisterSensor(_p,s);
    }
    bool remove(Sensor& s) {
        return aaxAudioFrameDeregisterSensor(_p,s);
    }
    bool add(Emitter& e) {
        return aaxAudioFrameRegisterEmitter(_p,e);
    }
    bool remove(Emitter& e) {
        return aaxAudioFrameDeregisterEmitter(_p,e);
    }

    // ** position and orientation ******
    inline bool matrix(Matrix& m) {
        return aaxAudioFrameSetMatrix(_p,m);
    }
    inline bool get(Matrix& m) {
        return aaxAudioFrameGetMatrix(_p,m);
    }
    inline bool velocity(Vector& v) {
        return aaxAudioFrameSetVelocity(_p,v);
    }
    inline bool get(Vector& v) {
        return aaxAudioFrameGetVelocity(_p,v);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxAudioFrameWaitForBuffer(_p,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxAudioFrameGetBuffer(_p));
    }

    // ** support ******
    Frame& operator=(const Frame& o) {
        if (_close) _close(_p);
        _p = o._p; _close = o._close; o._close = 0;
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
        return aaxDriverGetDeviceNameByPos(_p,d,_m);
    }
    const char* interface(int d, unsigned i) {
        const char* ed = device(d);
        return aaxDriverGetInterfaceNameByPos(_p,ed,i,_m);
    }
    inline const char* interface(const char* d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(_p,d,i,_m);
    }
    inline const char* interface(std::string& d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(_p,d.c_str(),i,_m);
    }

    // ** support ******
    static unsigned major_version() {
        return aaxGetMajorVersion();
    }
    static unsigned minor_version() {
        return aaxGetMinorVersion();
    }
    static unsigned int patch_level() {
        return aaxGetPatchLevel();
    }

    inline bool valid(aaxConfig c, enum aaxHandleType t=AAX_CONFIG) {
        return aaxIsValid(c,t);
    }
    inline bool valid(enum aaxHandleType t) {
        return aaxIsValid(_p,t);
    }

    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(_p,t);
    }

    AeonWave& operator=(const AeonWave& o) {
        aaxGetErrorNo();
        if (_close) _close(_p);
        _p = o._p; _m = o._m; _close = o._close; o._close = 0;
        return *this;
    }

    // ** mixing ******
    inline bool add(Frame& m) {
        return aaxMixerRegisterAudioFrame(_p,m);
    }
    inline bool remove(Frame& m) {
        return aaxMixerDeregisterAudioFrame(_p,m);
    }
    inline bool add(Sensor& s) {
        return aaxMixerRegisterSensor(_p,s);
    }
    inline bool remove(Sensor& s) {
        return aaxMixerDeregisterSensor(_p,s);
    }
    inline bool add(Emitter& e) {
        return aaxMixerRegisterEmitter(_p,e);
    }
    inline bool remove(Emitter& e) {
        return aaxMixerDeregisterEmitter(_p,e);
    }

    // ** buffer management ******
    // Get a shared buffer from the buffer cache if it's URL is already
    // in the cache. Otherwise create a new one and add it to the cache.
    Buffer& buffer(std::string name) {
        _buffer_it it = _buffer_cache.find(name);
        if (it == _buffer_cache.end()) {
            std::pair<_buffer_it,bool> ret = _buffer_cache.insert(std::make_pair(name,std::make_pair(static_cast<size_t>(0),Buffer(aaxBufferReadFromStream(_p,name.c_str()),false))));
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
    bool play(std::string f) {
        std::string devname = std::string("AeonWave on Audio Files: ")+f;
        _play = Sensor(devname, AAX_MODE_READ);
        return add(_play) ? (_play.set(AAX_INITIALIZED) ? _play.sensor(AAX_CAPTURING) : false) : false;
    }

    bool stop() {
        return _play.set(AAX_STOPPED);
    }

    bool playing() {
        return (_play.state() == AAX_PLAYING);
    }

    float offset() {
        return (float)_play.offset(AAX_SAMPLES)/(float)_play.get(AAX_FREQUENCY);
    }

private:
    std::map<std::string,std::pair<size_t,Buffer> > _buffer_cache;
    typedef std::map<std::string,std::pair<size_t,Buffer> >::iterator _buffer_it;

    // background music stream
    Sensor _play;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

} // namespace aax

#endif

