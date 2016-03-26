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
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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

#include <stdio.h>

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include <aax/Matrix.hpp>

namespace AAX
{


class Buffer
{
public:
    Buffer(aaxBuffer b, bool o) : _b(b), _owner(o) {}

    Buffer(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f) :
        Buffer(aaxBufferCreate(c,n,t,f),true) {}

    ~Buffer() {
        if (_owner) aaxBufferDestroy(_b);
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxBufferSetSetup(_b,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxBufferGetSetup(_b,t);
    }
    inline bool set(const void* d) {
        return aaxBufferSetData(_b,d);
    }
    inline void** get() {
        return aaxBufferGetData(_b);
    }

    // ** buffer data mangling ******
    inline bool process(float f, enum aaxWaveformType t,
                        float r=0.5f, enum aaxProcessingType p=AAX_MIX) {
        return aaxBufferProcessWaveform(_b,f,t,r,p);
    }
    inline bool process(float f, enum aaxWaveformType t,
                        enum aaxProcessingType p, float r=0.5f) {
        return aaxBufferProcessWaveform(_b,f,t,r,p);
    }

    // ** support ******
    bool operator=(bool o) {
       bool r = _owner; _owner = o;
       return r;
    }

    Buffer& operator=(aaxBuffer& b) {
        _owner = b = false; _b = b;
        return *this;
    }

    operator void*() const {
        return _b;
    }

    explicit operator bool() {
        return _b ? true : false;
    }

private:
    aaxBuffer _b;
    bool _owner;
};


class DSP
{
public:
    DSP() : _f(0), _e(0) {}

    DSP(aaxConfig c, enum aaxFilterType f) : _e(0) {
        if (aaxIsValid(c, AAX_FILTER)) _f = c;
        else _f = aaxFilterCreate(c,f);
    }
    DSP(aaxConfig c, enum aaxEffectType e) : _f(0) {
        if (aaxIsValid(c, AAX_EFFECT)) _e = c;
        else _e = aaxEffectCreate(c,e);
    }
    ~DSP() {
        if (_f) aaxFilterDestroy(_f);
        else if(_e) aaxEffectDestroy(_e);
    }

    bool set(int s) {
        return (_f) ? (aaxFilterSetState(_f,s) ? true : false)
                    : aaxEffectSetState(_e,s) ? true : false;
    }
    int get() {
        return (_f) ? aaxFilterGetState(_f) : aaxEffectGetState(_e);
    }
    bool set(unsigned s, float p1, float p2, float p3, float p4, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlot(_f,s,t,p1,p2,p3,p4)
                  : aaxEffectSetSlot(_e,s,t,p1,p2,p3,p4);
    }

    bool get(unsigned s, float* p1, float* p2, float* p3, float* p4, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetSlot(_f,s,t,p1,p2,p3,p4)
                    : aaxEffectGetSlot(_e,s,t,p1,p2,p3,p4);
    }
    bool set(unsigned s, Vector& v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlotParams(_f,s,t,v)
                    : aaxEffectSetSlotParams(_e,s,t,v);
    }
    bool set(unsigned s, aaxVec4f v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetSlotParams(_f,s,t,v)
                    : aaxEffectSetSlotParams(_e,s,t,v);
    }
    bool get(unsigned s, aaxVec4f v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetSlotParams(_f,s,t,v)
                    : aaxEffectGetSlotParams(_e,s,t,v);
    }
    bool set(int p, float v, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterSetParam(_f,p,t,v) : aaxEffectSetParam(_e,p,t,v);
    }
    float get(int p, int t=AAX_LINEAR) {
        return (_f) ? aaxFilterGetParam(_f,p,t) : aaxEffectGetParam(_e,p,t);
    }

    // ** support ******
    DSP& operator=(DSP dsp) {
        aaxConfig c = dsp;
        dsp = 0; *this = c;
        return *this;
    }

    aaxConfig operator=(aaxConfig c) {
        aaxConfig rv = _f ? _f : _e; _e = _f = 0;
        if (aaxIsValid(c, AAX_FILTER)) _f = c;
        else if (aaxIsValid(c, AAX_EFFECT)) _e = c;
        return rv;
    }

    operator void*() const {
        return _f ? _f : _e;
    }

    explicit operator bool() {
        return (_f || _e) ? true : false;
    }

    bool is_filter() {
        return _f ? true : false;
    }

private:
    aaxFilter _f;
    aaxEffect _e;
};


class Emitter
{
public:
    Emitter() : _e(aaxEmitterCreate()), _owner(true) {}

    Emitter(aaxEmitter e, bool o) : _e(e), _owner(o) {}

    ~Emitter() {
        if (_owner) aaxEmitterDestroy(_e);
    }

    inline bool set(enum aaxModeType t, int m) {
        return aaxEmitterSetMode(_e,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxEmitterGetMode(_e,t);
    }
    inline bool set(enum aaxState s) {
        return aaxEmitterSetState(_e,s);
    }
    inline enum aaxState get() {
        return aaxState(aaxEmitterGetState(_e));
    }

    // ** filters and effects ******
    bool set(DSP dsp) {
        return dsp.is_filter() ? aaxEmitterSetFilter(_e,dsp)
                               : aaxEmitterSetEffect(_e,dsp);
    }
    inline DSP get(enum aaxFilterType f) {
        return DSP(aaxEmitterGetFilter(_e,f),f);
    }
    inline DSP get(enum aaxEffectType e) {
        return DSP(aaxEmitterGetEffect(_e,e),e);
    }

    // ** position and orientation ******
    inline bool set(Matrix& m) {
        return aaxEmitterSetMatrix(_e,m.ptr());
    }
    inline bool set(aaxMtx4f m) {
        return aaxEmitterSetMatrix(_e,m);
    }
    inline bool get(Matrix& m) {
        return aaxEmitterGetMatrix(_e,m.ptr());
    }
    inline bool get(aaxMtx4f m) {
        return aaxEmitterGetMatrix(_e,m);
    }
    inline bool set(Vector& v) {
        return aaxEmitterSetVelocity(_e,v);
    }
    inline bool set(const aaxVec3f v) {
        return aaxEmitterSetVelocity(_e,v);
    }
    inline bool get(aaxVec3f v) {
        return aaxEmitterGetVelocity(_e,v);
    }

    // ** buffer handling ******
    inline bool add(Buffer& b) {
        return aaxEmitterAddBuffer(_e,b);
    }
    inline bool remove() {
        return aaxEmitterRemoveBuffer(_e);
    }
    inline Buffer get(unsigned int p, int c=AAX_FALSE) {
        return Buffer(aaxEmitterGetBufferByPos(_e,p,c),c ? false : true);
    }
    inline unsigned int get(enum aaxState s) {
        return aaxEmitterGetNoBuffers(_e,s);
    }
    inline bool offset(unsigned long o, enum aaxType t) {
        return aaxEmitterSetOffset(_e,o,t);
    }
    inline bool offset(float o) {
        return aaxEmitterSetOffsetSec(_e,o);
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxEmitterGetOffset(_e,t);
    }
    inline float offset() {
        return aaxEmitterGetOffsetSec(_e);
    }

    // ** support ******
    operator void*() const {
        return _e;
    }

    explicit operator bool() {
        return _e ? true : false;
    }

private:
    aaxEmitter _e;
    bool _owner;
};


class Sensor
{
public:
    Sensor() : _m(AAX_MODE_READ), _c(0), _owner(false) {}

    Sensor(aaxConfig c, bool o) :
        _m(AAX_MODE_READ), _c(c), _owner(o) {}

    Sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        _m(m), _c(aaxDriverOpenByName(n,m)), _owner(true) {}

    Sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(s.empty() ? "default" : s.c_str(),m) {}

    ~Sensor() {
        if (_owner) aaxDriverDestroy(_c);
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxMixerSetSetup(_c,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxMixerGetSetup(_c,t);
    }

    inline bool set(enum aaxState s) {
        return aaxMixerSetState(_c,s);
    }
    inline enum aaxState get() {
        return aaxState(aaxMixerGetState(_c));
    }

    // ** driver ******
    bool close() {
        return aaxDriverClose(_c) ? (((_c=0) == 0) ? false : true) : false;
    }
    inline bool get(enum aaxRenderMode m) {
        return aaxDriverGetSupport(_c,m);
    }
    inline const char* info(enum aaxSetupType t) {
        return aaxDriverGetSetup(_c,t);
    }

    inline const char* info(enum aaxFilterType f) {
        return aaxFilterGetNameByType(_c,f);
    }
    inline const char* info(enum aaxEffectType e) {
        return aaxEffectGetNameByType(_c,e);
    }
    bool supports(const char* fe) {
        return aaxIsFilterSupported(_c,fe) ? true : aaxIsEffectSupported(_c,fe);
    }
    inline bool supports(std::string& s) {
        return supports(s.c_str());
    }

    // ** filters and effects ******
    bool set(DSP dsp) {
        int res = dsp.is_filter() ? aaxScenerySetFilter(_c,dsp)
                                  : aaxScenerySetEffect(_c,dsp);
        if (!res) res = dsp.is_filter() ? aaxMixerSetFilter(_c,dsp)
                                        : aaxMixerSetEffect(_c,dsp);
        return res;
    }
    DSP get(enum aaxFilterType t) {
        aaxFilter f = aaxSceneryGetFilter(_c,t);
        if (!f) f = aaxMixerGetFilter(_c,t);
        return DSP(f,t);
    }
    DSP get(enum aaxEffectType t) {
        aaxEffect e = aaxSceneryGetEffect(_c,t);
        if (!e) e = aaxMixerGetEffect(_c,t);
        return DSP(e,t);
    }

    // ** position and orientation ******
    inline bool set(Matrix& m) {
        return aaxSensorSetMatrix(_c,m.ptr());
    }
    inline bool set(aaxMtx4f m) {
        return aaxSensorSetMatrix(_c,m);
    }
    inline bool get(Matrix& m) {
        return aaxSensorGetMatrix(_c,m.ptr());
    }
    inline bool get(aaxMtx4f m) {
        return aaxSensorGetMatrix(_c,m);
    }
    inline bool set(Vector& v) {
        return aaxSensorSetVelocity(_c,v);
    }
    inline bool set(const aaxVec3f v) {
        return aaxSensorSetVelocity(_c,v);
    }
    inline bool get(aaxVec3f v) {
        return aaxSensorGetVelocity(_c,v);
    }
    inline bool sensor(enum aaxState s) {
        return aaxSensorSetState(_c,s);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxSensorWaitForBuffer(_c,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxSensorGetBuffer(_c),true);
    }
    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(_c,t);
    }

    // ** support ******
    inline const char* version() {
        return aaxGetVersionString(_c);
    }

    bool operator=(bool o) {
        bool r = _owner; _owner = o;
        return r;
    }

    Sensor& operator=(Sensor s) {
        _owner = s = false; _c = s;
        return *this;
    }

    operator void*() const {
        return _c;
    }

    explicit operator bool() {
        return _c ? true : false;
    }

protected:
    enum aaxRenderMode _m;
    aaxConfig _c;
    bool _owner;
};


class Mixer
{
public:
    Mixer() : _f(0), _owner(false) {}

    Mixer(aaxConfig c) : _f(aaxAudioFrameCreate(c)), _owner(true) {}

    Mixer(aaxFrame f, bool o) : _f(f), _owner(o) {}

    ~Mixer() {
        if (_owner) aaxAudioFrameDestroy(_f);
    }

    inline bool set(enum aaxSetupType t, unsigned int s) {
        return aaxAudioFrameSetSetup(_f,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return aaxAudioFrameGetSetup(_f,t);
    }

    inline bool set(enum aaxState s) {
        return aaxAudioFrameSetState(_f,s);
    }
    inline enum aaxState get() {
        return aaxState(aaxAudioFrameGetState(_f));
    }
    inline bool set(enum aaxModeType t, int m) {
        return aaxAudioFrameSetMode(_f,t,m);
    }
    inline int get(enum aaxModeType t) {
        return aaxAudioFrameGetMode(_f,t);
    }

    // ** filters and effects ******
    bool set(DSP dsp) {
        return dsp.is_filter() ? aaxAudioFrameSetFilter(_f,dsp)
                               : aaxAudioFrameSetEffect(_f,dsp);
    }
    inline DSP get(enum aaxFilterType t) {
        return DSP(aaxAudioFrameGetFilter(_f,t),t);
    }
    inline DSP get(enum aaxEffectType t) {
        return DSP(aaxAudioFrameGetEffect(_f,t),t);
    }

    // ** sub-mixing ******
    bool add(Mixer& m) {
        return aaxAudioFrameRegisterAudioFrame(_f,m);
    }
    bool remove(Mixer& m) {
        return aaxAudioFrameDeregisterAudioFrame(_f,m);
    }
    bool add(Sensor& s) {
        return aaxAudioFrameRegisterSensor(_f,s);
    }
    bool remove(Sensor& s) {
        return aaxAudioFrameDeregisterSensor(_f,s);
    }
    bool add(Emitter& e) {
        return aaxAudioFrameRegisterEmitter(_f,e);
    }
    bool remove(Emitter& e) {
        return aaxAudioFrameDeregisterEmitter(_f,e);
    }

    // ** position and orientation ******
    inline bool set(Matrix& m) {
        return aaxAudioFrameSetMatrix(_f,m.ptr());
    }
    inline bool set(aaxMtx4f m) {
        return aaxAudioFrameSetMatrix(_f,m);
    }
    inline bool get(Matrix& m) {
        return aaxAudioFrameGetMatrix(_f,m.ptr());
    }
    inline bool get(aaxMtx4f m) {
        return aaxAudioFrameGetMatrix(_f,m);
    }
    inline bool set(Vector& v) {
        return aaxAudioFrameSetVelocity(_f,v);
    }
    inline bool set(const aaxVec3f v) {
        return aaxAudioFrameSetVelocity(_f,v);
    }
    inline bool get(aaxVec3f v) {
        return aaxAudioFrameGetVelocity(_f,v);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxAudioFrameWaitForBuffer(_f,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxAudioFrameGetBuffer(_f),true);
    }

    // ** support ******
    operator void*() const {
        return _f;
    }

    explicit operator bool() {
        return _f ? true : false;
    }

private:
    aaxFrame _f;
    bool _owner;
};
typedef Mixer Frame;


class AeonWave : public Sensor
{
public:
    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(n,m), _ec(0) { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave("default",m) {}

    ~AeonWave() {
        while (_mixer.size()) {
            aaxAudioFrameDestroy(_mixer.back()); _mixer.pop_back();
        }
        while (_sensor.size()) {
            aaxDriverDestroy(_sensor.back()); _sensor.pop_back();
        }
        for(_buffer_it it=_buffer_cache.begin(); it!=_buffer_cache.end(); it++){
             aaxBufferDestroy(it->second.second); _buffer_cache.erase(it);
        }
    }

    bool scenery(DSP dsp) {
        return dsp.is_filter() ? aaxScenerySetFilter(_c,dsp)
                               : aaxScenerySetEffect(_c,dsp);
    }
    inline DSP scenery(enum aaxFilterType t) {
        return DSP(aaxSceneryGetFilter(_c,t),t);
    }
    inline DSP scenery(enum aaxEffectType t) {
        return DSP(aaxSceneryGetEffect(_c,t),t);
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
    const char* devices() {
        _ed = _e[1] ? 0 : ""; _e[2] = 0;
        if (_e[1]++ < aaxDriverGetDeviceCount(_ec,_em)) {
            _ed = aaxDriverGetDeviceNameByPos(_ec,_e[1]-1,_em);
        }
        return _ed;
    }
    const char* interfaces() {
        const char *ifs = _e[2] ? 0 : "";
        if (_e[2]++ < aaxDriverGetInterfaceCount(_ec,_ed,_em)) {
            ifs = aaxDriverGetInterfaceNameByPos(_ec,_ed,_e[2]-1,_em);
        }
        return ifs;
    }

    inline const char* device(unsigned d) {
        return aaxDriverGetDeviceNameByPos(_c,d,_m);
    }
    const char* interface(int d, unsigned i) {
        const char* ed = device(d);
        return aaxDriverGetInterfaceNameByPos(_c,ed,i,_m);
    }
    inline const char* interface(const char* d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(_c,d,i,_m);
    }
    inline const char* interface(std::string& d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(_c,d.c_str(),i,_m);
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
    static enum aaxErrorType error_no() {
        return aaxGetErrorNo();
    }
    static const char* error(enum aaxErrorType e=aaxGetErrorNo()) {
        return aaxGetErrorString(e);
    }

    inline bool valid(aaxConfig c, enum aaxHandleType t=AAX_CONFIG) {
        return aaxIsValid(c,t);
    }
    inline bool valid(enum aaxHandleType t) {
        return aaxIsValid(_c,t);
    }

    // ** mixing ******
    inline bool add(Mixer& m) {
        return aaxMixerRegisterAudioFrame(_c,m);
    }
    inline bool remove(Mixer& m) {
        return aaxMixerDeregisterAudioFrame(_c,m);
    }
    inline bool add(Sensor& s) {
        return aaxMixerRegisterSensor(_c,s);
    }
    inline bool remove(Sensor& s) {
        return aaxMixerDeregisterSensor(_c,s);
    }
    inline bool add(Emitter& e) {
        return aaxMixerRegisterEmitter(_c,e);
    }
    inline bool remove(Emitter& e) {
        return aaxMixerDeregisterEmitter(_c,e);
    }

    // ** module management ******
    Mixer mixer() {
        aaxFrame m = aaxAudioFrameCreate(_c); _mixer.push_back(m);
        return Mixer(m,false);
    }
    inline Frame frame() { return mixer(); }
    void destroy(Mixer& m) {
        _void_it it = std::remove(_mixer.begin(),_mixer.end(),m);
        _mixer.erase(it,_mixer.end()); aaxAudioFrameDestroy(*it);
    }

    Sensor sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        aaxConfig c = aaxDriverOpenByName(n,m); _sensor.push_back(c);
        return Sensor(c,false);
    }
    Sensor sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        return sensor(s.empty() ? 0 : s.c_str(),m);
    }
    Sensor sensor(enum aaxRenderMode m) {
        return sensor(0,m);
    }
    void destroy(Sensor& s) {
        _void_it it = std::remove(_sensor.begin(),_sensor.end(),s);
        _sensor.erase(it,_sensor.end()); aaxDriverDestroy(*it);
    }

    Emitter emitter() {
        aaxEmitter e = aaxEmitterCreate(); _emitter.push_back(e);
        return Emitter(e,false);
    }

    void destroy(Emitter& e) {
        _void_it it = std::remove(_emitter.begin(),_emitter.end(),e);
        _emitter.erase(it,_sensor.end()); aaxEmitterDestroy(*it);
    }

    // Get a shared buffer from the buffer cache if it's full path is already
    // in the cache. Otherwise create a new one and add it to the cache.
    Buffer buffer(std::string path) {
        _buffer_it it = _buffer_cache.find(path);
        if (it == _buffer_cache.end()) {
            aaxBuffer b = aaxBufferReadFromStream(_c,path.c_str());
            std::pair<_buffer_it,bool> ret = _buffer_cache.insert(std::make_pair(path,std::make_pair(static_cast<size_t>(0),b)));
            it = ret.first;
        }
       it->second.first++;
       return Buffer(it->second.second,false);
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

    // ** backgrpund music ******
    bool play(std::string f) {
        f = "AeonWave on Audio Files: "+f; _play = Sensor(f, AAX_MODE_READ);
        return add(_play) ? (_play.set(AAX_INITIALIZED) ? _play.sensor(AAX_CAPTURING) : false) : false;
    }

    bool stop() {
        return _play.set(AAX_STOPPED);
    }

    bool playing() {
        return _play.get()== AAX_PLAYING;
    }

    float offset() {
        return (float)_play.offset(AAX_SAMPLES)/(float)_play.get(AAX_FREQUENCY);
    }

private:
    std::map<std::string,std::pair<size_t,aaxBuffer> > _buffer_cache;
    typedef std::map<std::string,std::pair<size_t,aaxBuffer> >::iterator _buffer_it;

    std::vector<aaxFrame> _mixer;
    std::vector<aaxConfig> _sensor;
    std::vector<aaxEmitter> _emitter;
    typedef std::vector<void*>::iterator _void_it;

    // background music stream
    Sensor _play;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

}

#endif

