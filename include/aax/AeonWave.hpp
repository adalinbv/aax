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

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include <aax/aax.h>

namespace AAX
{

class DSP
{
public:
    DSP(aaxConfig c, enum aaxFilterType f) :
        _e(0) {
        if (aaxIsValid(c, AAX_FILTER)) _f = c;
        else _f = aaxFilterCreate(c,f);
    }
    DSP(aaxConfig c, enum aaxEffectType e) :
        _f(0) {
        if (aaxIsValid(c, AAX_EFFECT)) _f = c;
        else _f = aaxEffectCreate(c,e);
    }
    ~DSP() {
        if (_f) aaxFilterDestroy(_f);
        else aaxEffectDestroy(_e);
    }

    bool set(int s) {
        if (_f) return aaxFilterSetState(_f,s);
        else return aaxEffectSetState(_e,s);
    }
    int get() {
        if (_f) return aaxFilterGetState(_f);
        else aaxEffectGetState(_e);
    }
    bool set(unsigned s, int t, float p1, float p2, float p3, float p4) {
        if (_f) return aaxFilterSetSlot(_f,s,t,p1,p2,p3,p4);
        else return aaxEffectSetSlot(_e,s,t,p1,p2,p3,p4);
    }
    bool get(unsigned s, int t, float* p1, float* p2, float* p3, float* p4) {
        if (_f) aaxFilterGetSlot(_f,s,t,p1,p2,p3,p4);
        else aaxEffectGetSlot(_e,s,t,p1,p2,p3,p4);
    }
    bool set(unsigned s, int t, aaxVec4f v) {
        if (_f) return aaxFilterSetSlotParams(_f,s,t,v);
        else return aaxEffectSetSlotParams(_e,s,t,v);
    }
    bool get(unsigned s, int t, aaxVec4f v) {
        if (_f) return aaxFilterGetSlotParams(_f,s,t,v);
        else return aaxEffectGetSlotParams(_e,s,t,v);
    }
    bool set(int p, int t, float v) {
        if (_f) return aaxFilterSetParam(_f,p,t,v);
        else return  aaxEffectSetParam(_e,p,t,v);
    }
    float get(int p, int t) {
        if (_f) return aaxFilterGetParam(_f,p,t);
        else return  aaxEffectGetParam(_e,p,t);
    }

    void* config() const {
        return _f ? _f : _e;
    }
    bool is_filter() {
        return _f ? true : false;
    }

private:
    aaxFilter _f;
    aaxEffect _e;
};


class Sensor
{
public:
    Sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        _m(m), _c(aaxDriverOpenByName(n,m)) {}

    Sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(s.empty() ? 0 : s.c_str(),m) {}

    Sensor(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) : Sensor(0,m) {}

    ~Sensor() {
        aaxDriverClose(_c); aaxDriverDestroy(_c); _c = 0;
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
        return dsp.is_filter() ? aaxMixerSetFilter(_c,dsp.config())
                               : aaxMixerSetEffect(_c,dsp.config());
    }
    inline DSP get(enum aaxFilterType t) {
        return DSP(aaxMixerGetFilter(_c,t),t);
    }
    inline DSP get(enum aaxEffectType t) {
        return DSP(aaxMixerGetEffect(_c,t),t);
    }

    // ** position and orientation ******
    inline bool set(aaxMtx4f m) {
        return aaxSensorSetMatrix(_c,m);
    }
    inline bool get(aaxMtx4f m) {
        return aaxSensorGetMatrix(_c,m);
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
    inline aaxBuffer buffer() {
        return aaxSensorGetBuffer(_c);
    }
    inline float offset(enum aaxType t) {
        return aaxSensorGetOffset(_c,t);
    }

    // ** support ******
    inline const char* version() {
        return aaxGetVersionString(_c);
    }

    aaxConfig config() const {
        return _c;
    }

    bool operator==(const Sensor* s) {
        return (s->config() == _c);
    }

protected:
    enum aaxRenderMode _m;
    aaxConfig _c;
};
static Sensor EMPTY;


class Mixer
{
public:
    Mixer(aaxConfig c) : _f(aaxAudioFrameCreate(c)) {}

    ~Mixer() {
        aaxAudioFrameDestroy(_f); _f = 0;
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

    // ** filters and effects ******
    bool set(DSP dsp) {
        return dsp.is_filter() ? aaxAudioFrameSetFilter(_f,dsp.config())
                               : aaxAudioFrameSetEffect(_f,dsp.config());
    }
    inline DSP get(enum aaxFilterType t) {
        return DSP(aaxAudioFrameGetFilter(_f,t),t);
    }
    inline DSP get(enum aaxEffectType t) {
        return DSP(aaxAudioFrameGetEffect(_f,t),t);
    }

    // ** sub-mixing ******
    bool add(Mixer* m) {
        const aaxFrame f = m ? m->config() : 0;
        return aaxAudioFrameRegisterAudioFrame(_f,f);
    }
    bool add(Sensor* s) {
        const aaxConfig c = s ? s->config() : 0;
        return aaxAudioFrameRegisterSensor(_f,c);
    }
#if 0
    bool add(Emitter* e) {
        const aaxEmitter e = m ? m->config() : 0;
        return aaxAudioFrameRegisterEmitter(_f,e);
    }
#endif
    bool remove(Mixer* m) {
        const aaxConfig c = m ? m->config() : 0;
        return aaxAudioFrameDeregisterAudioFrame(_f,c);
    }
    bool remove(Sensor* s) {
        const aaxConfig c = s ? s->config() : 0;
        return aaxAudioFrameDeregisterSensor(_f,c);
    }
#if 0
    bool remove(Emitter* e) {
        const aaxEmitter e = m ? m->config() : 0;
        return aaxAudioFrameDeregisterEmitter(_f,e);
    }
#endif

    // ** position and orientation ******
    inline bool set(aaxMtx4f m) {
        return aaxAudioFrameSetMatrix(_f,m);
    }
    inline bool get(aaxMtx4f m) {
        return aaxAudioFrameGetMatrix(_f,m);
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
    }    inline aaxBuffer buffer() {
        return aaxAudioFrameGetBuffer(_f);
    }

    // ** support ******
    inline aaxConfig config() const {
        return _f;
    }

private:
    aaxFrame _f;
};

class AeonWave : public Sensor
{
public:
    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Sensor(n,m), _ec(0) { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) : AeonWave(0,m) {}

    ~AeonWave() {
        std::vector<Mixer*>::iterator itm = _mixer.begin();
        while (itm != _mixer.end()) {
            delete (*itm); itm = _mixer.erase(itm);
        } 
        std::vector<Sensor*>::iterator its = _sensor.begin();
        while (its != _sensor.end()) {
            delete (*its); its = _sensor.erase(its);
        }
        _mixer.clear();
        _sensor.clear();
    }

    // ** enumeration ******
    const char* drivers(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        aaxDriverClose(_ec); aaxDriverDestroy(_ec);
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

    const char* device(unsigned d) {
        return aaxDriverGetDeviceNameByPos(_c,d,_m);
    }
    const char* interface(int d, unsigned i) {
        const char* ed = device(d);
        return aaxDriverGetInterfaceNameByPos(_c,ed,i,_m);
    }
    const char* interface(const char* d, unsigned i) {
        return aaxDriverGetInterfaceNameByPos(_c,d,i,_m);
    }
    const char* interface(std::string& d, unsigned i) {
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

    // ** mixing ******
    bool add(Mixer* m) {
        const aaxFrame f = m ? m->config() : 0;
        return aaxMixerRegisterAudioFrame(_c,f);
    }
    bool add(Sensor* s) {
        const aaxConfig c = s ? s->config() : 0;
        return aaxMixerRegisterSensor(_c,c);
    }
#if 0
    bool add(Emitter* h) {
        const aaxEmitter e = h ? h->config() : 0;
        return aaxMixerRegisterEmitter(_c,e);
    }
#endif
    bool remove(Mixer* m) {
        const aaxConfig c = m ? m->config() : 0;
        return aaxMixerDeregisterAudioFrame(_c,c);
    }
    bool remove(Sensor* s) {
         const aaxConfig c = s ? s->config() : 0;
        return aaxMixerDeregisterSensor(_c,c);
    }
#if 0
    bool remove(Emitter* h) {
        const aaxEmitter e = h ? h->config() : 0;
        return aaxMixerDeregisterEmitter(_c,c);
    }
#endif

    // ** scenery ******
#if 0
    inline bool scenery(aaxFilter f) {
        return aaxScenerySetFilter(_c,f);
    }
    inline const aaxFilter scenery(enum aaxFilterType t) {
        return aaxMixerGetFilter(_c,t);
    }
    inline bool scenery(aaxEffect e) {
        return aaxScenerySetEffect(_c,e);
    }
    inline const aaxEffect scenery(enum aaxEffectType t) {
        return aaxMixerGetEffect(_c,t);
    }
#endif

    // ** module management ******
    Mixer* mixer() {
        _mixer.push_back(new Mixer(_c));
        return _mixer.back();
    }
    void destroy(Mixer* m) {
        _mixer.erase(std::remove(_mixer.begin(),_mixer.end(),m),_mixer.end());
    }
    Sensor* mixer(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        _sensor.push_back(new Sensor(n,m));
        return _sensor.back();
    }
    Sensor* mixer(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        return mixer(s.empty() ? 0 : s.c_str(),m);
    }
    Sensor* mixer(enum aaxRenderMode m) {
        return mixer(0,m);
    }
    void destroy(Sensor* m) {
        _sensor.erase(std::remove(_sensor.begin(),_sensor.end(),m),_sensor.end());
    }

private:
    std::vector<Mixer*> _mixer;
    std::vector<Sensor*> _sensor;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

}

#endif

