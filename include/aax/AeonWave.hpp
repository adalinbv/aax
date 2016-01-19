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

    void* config() {
        return _f ? _f : _e;
    }
    bool is_filter() {
        return _f ? true : false;
    }

private:
    aaxFilter _f;
    aaxEffect _e;
};

class Mixer
{
public:
    Mixer(aaxConfig c) : _c(0), _f(aaxAudioFrameCreate(c)) {}

    Mixer(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        _m(m), _c(aaxDriverOpenByName(n,m)), _f(0) {}

    Mixer(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Mixer(s.empty() ? 0 : s.c_str(),m) {}

    Mixer(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) : Mixer(0, m) {}

    ~Mixer() {
        if (_c) {
            aaxDriverClose(_c); aaxDriverDestroy(_c); _c = 0;
        } else {
            aaxAudioFrameDestroy(_f); _f = 0;
        }
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

    // ** mixer ******
    inline bool set(enum aaxSetupType t, unsigned int s) {
        return _c ? aaxMixerSetSetup(_c,t,s) : aaxAudioFrameSetSetup(_f,t,s);
    }
    inline unsigned int get(enum aaxSetupType t) {
        return _c ? aaxMixerGetSetup(_c,t) : aaxAudioFrameGetSetup(_f,t);
    }

    inline bool set(enum aaxState s) {
        return _c ? aaxMixerSetState(_c,s) : aaxAudioFrameSetState(_f,s);
    }
    inline enum aaxState get() {
        return aaxState(_c ? aaxMixerGetState(_c) : aaxAudioFrameGetState(_f));
    }

    bool set(DSP dsp) {
        return dsp.is_filter() ? 
               (_c ? aaxMixerSetFilter(_c,dsp.config())
                   : aaxAudioFrameSetFilter(_f,dsp.config())) :
               (_c ? aaxMixerSetEffect(_c,dsp.config())
                   : aaxAudioFrameSetEffect(_f,dsp.config()));
    }
    inline DSP get(enum aaxFilterType t) {
        return DSP(_c ? aaxMixerGetFilter(_c,t)
                      : aaxAudioFrameGetFilter(_f,t),t);
    }
    inline DSP get(enum aaxEffectType t) {
        return DSP(_c ? aaxMixerGetEffect(_c,t) 
                      : aaxAudioFrameGetEffect(_f,t),t);
    }

    bool add(Mixer* m) {
        const aaxConfig c = m ? m->config() : 0;
        if (_c) {
          if (aaxIsValid(c, AAX_EMITTER)) aaxMixerRegisterEmitter(_c,c);
          else if (aaxIsValid(c, AAX_CONFIG)) aaxMixerRegisterSensor(_c,c);
          else aaxMixerRegisterAudioFrame(_c,c);
        } else {
          if (aaxIsValid(c, AAX_EMITTER)) aaxAudioFrameRegisterEmitter(_c,c);
          else if (aaxIsValid(c, AAX_CONFIG)) aaxAudioFrameRegisterSensor(_c,c);
          else aaxAudioFrameRegisterAudioFrame(_c,c);
        }
    }
    bool remove(Mixer* m) {
        const aaxConfig c = m ? m->config() : 0;
        if (_c) {
          if (aaxIsValid(c, AAX_EMITTER)) aaxMixerDeregisterEmitter(_c,c);
          else if (aaxIsValid(c, AAX_CONFIG)) aaxMixerDeregisterSensor(_c,c);
          else aaxMixerDeregisterAudioFrame(_c,c);
        } else {
          if (aaxIsValid(c, AAX_EMITTER)) aaxAudioFrameDeregisterEmitter(_c,c);
          else if(aaxIsValid(c,AAX_CONFIG)) aaxAudioFrameDeregisterSensor(_c,c);
          else aaxAudioFrameDeregisterAudioFrame(_c,c);
        }
    }

    // ** sensor ******
    inline bool set(aaxMtx4f m) {
        return _c ? aaxSensorSetMatrix(_c,m) : aaxAudioFrameSetMatrix(_f,m);
    }
    inline bool get(aaxMtx4f m) {
        return _c ? aaxSensorGetMatrix(_c,m) : aaxAudioFrameGetMatrix(_f,m);
    }
    inline bool set(const aaxVec3f v) {
        return _c ? aaxSensorSetVelocity(_c,v) : aaxAudioFrameSetVelocity(_f,v);
    }
    inline bool get(aaxVec3f v) {
        return _c ? aaxSensorGetVelocity(_c,v) : aaxAudioFrameGetVelocity(_f,v);
    }
    inline bool sensor(enum aaxState s) {
        return aaxSensorSetState(_c,s);
    }
    inline bool wait(float t) {
        return _c ? aaxSensorWaitForBuffer(_c,t) : aaxAudioFrameWaitForBuffer(_f,t);
    }
    inline aaxBuffer buffer() {
        return _c ? aaxSensorGetBuffer(_c) : aaxAudioFrameGetBuffer(_f);
    }
    inline float offset(enum aaxType t) {
        return aaxSensorGetOffset(_c,t);
    }

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

    // ** support ******
    inline const char* version() {
        return aaxGetVersionString(_c);
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

    inline aaxConfig config() {
        return _c ? _c : _f;
    }

protected:
    enum aaxRenderMode _m;
    aaxConfig _c;
    aaxFrame _f;
};


class AeonWave : public Mixer
{
public:
    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Mixer(n,m),
        _ec(0)
    { _e[0] = _e[1] = _e[2] = 0; }

    AeonWave(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        AeonWave(s.empty() ? 0 : s.c_str(),m) {}

    AeonWave(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) : AeonWave(0,m) {}

    ~AeonWave() {
        std::vector<Mixer*>::iterator it = _mixer.begin();
        while (it != _mixer.end()) {
            delete (*it); it = _mixer.erase(it);
        } 
        _mixer.clear();
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


    // ** module management ******
    Mixer* mixer(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        _mixer.push_back(new Mixer(n,m));
        return _mixer.back();
    }
    inline Mixer* mixer(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        return mixer(s.c_str(),m);
    }
    void destroy(Mixer* m) {
        _mixer.erase(std::remove(_mixer.begin(),_mixer.end(),m),_mixer.end());
    }

private:
    std::vector<Mixer*> _mixer;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

}

#endif

