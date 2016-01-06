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
#include <algorithm>

#include <aax/aax.h>

namespace AAX
{

class AeonWave;

class DSP
{
public:
    DSP(AeonWave& a, enum aaxFilterType f) :
        _e(0), _f(aaxFilterCreate(a.config(),f)) {}
    DSP(AeonWave& a, enum aaxEffectType e) :
        _f(0), _e(aaxEffectCreate(a.config(),e)) {}
    ~DSP() {
        if (_f) aaxFilterDestroy(_f);
        else aaxEffectDestroy(_e);
    }

    inline bool set(int s) {
        if (_f) return aaxFilterSetState(_f,s);
        else return aaxEffectSetState(_e,s);
    }
    inline int get() {
        if (_f) return aaxFilterGetState(_f);
        else aaxEffectGetState(_e);
    }
    inline bool set(unsigned s, int t, float p1, float p2, float p3, float p4) {
        if (_f) return aaxFilterSetSlot(_f,s,t,p1,p2,p3,p4);
        else return aaxEffectSetSlot(_e,s,t,p1,p2,p3,p4);
    }
    inline bool get(unsigned s, int t, float* p1, float* p2, float* p3, float* p4) {
        if (_f) aaxFilterGetSlot(_f,s,t,p1,p2,p3,p4);
        else aaxEffectGetSlot(_e,s,t,p1,p2,p3,p4);
    }
    inline bool set(unsigned s, int t, aaxVec4f v) {
        if (_f) return aaxFilterSetSlotParams(_f,s,t,v);
        else return aaxEffectSetSlotParams(_e,s,t,v);
    }
    inline bool get(unsigned s, int t, aaxVec4f v) {
        if (_f) return aaxFilterGetSlotParams(_f,s,t,v);
        else return aaxEffectGetSlotParams(_e,s,t,v);
    }
    inline bool set(int p, int t, float v) {
        if (_f) return aaxFilterSetParam(_f,p,t,v);
        else return  aaxEffectSetParam(_e,p,t,v);
    }
    inline float get(int p, int t) {
        if (_f) return aaxFilterGetParam(_f,p,t);
        else return  aaxEffectGetParam(_e,p,t);
    }

private:
    aaxFilter _f;
    aaxEffect _e;
};

class Mixer
{
public:
    Mixer(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        _c(aaxDriverOpenByName(n,m))
    {}

    Mixer(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        Mixer(0, m)
    {}

    ~Mixer() {
        aaxDriverClose(_c); aaxDriverDestroy(_c); _c = 0;
    }

    // ** driver ******
    inline bool close() {
        return aaxDriverClose(_c) ? (((_c=0) == 0) ? false : true) : false;
    }
    inline const char* info(enum aaxSetupType t) {
        return aaxDriverGetSetup(_c,t);
    }
    inline bool get(enum aaxRenderMode m) {
        return aaxDriverGetSupport(_c,m);
    }
    inline bool supports(const char* fe) {
        return aaxIsFilterSupported(_c,fe) ? true : aaxIsEffectSupported(_c,fe);
    }

    // ** mixer ******
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

#if 0
    inline bool set(DSP dsp) {
        return aaxMixerSetFilter(_c,f);
    }
    inline const aaxFilter get(enum aaxFilterType t) {
        return aaxMixerGetFilter(_c,t);
    }
    inline bool set(aaxEffect e) {
        return aaxMixerSetEffect(_c,e);
    }
    inline const aaxEffect get(enum aaxEffectType t) {
        return aaxMixerGetEffect(_c,t);
    }

    inline bool add(const aaxEmitter e) {
        return aaxMixerRegisterEmitter(_c,e);
    }
    inline bool remove(const aaxEmitter e) {
        return aaxMixerDeRegisterEmitter(_c,e);
    }
    inline bool add(const aaxFrame f) {
        return aaxMixerRegisterAudioFrame(_c,f);
    }
    inline bool remove(const aaxFrame f) {
        return aaxMixerDeRegisterAudioFrame(_c,f);
    }
#endif
    inline bool add(const aaxConfig s) {
        return aaxMixerRegisterSensor(_c,s);
    }
    inline bool remove(const aaxConfig s) {
        return aaxMixerDeregisterSensor(_c,s);
    }

    // ** sensor ******
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
        return aaxSensorSetVelocity(_c,v);
    }
    inline bool sensor(enum aaxState s) {
        return aaxSensorSetState(_c,s);
    }
    inline bool wait(float t) {
        return aaxSensorWaitForBuffer(_c, t);
    }
    inline aaxBuffer buffer() {
        return aaxSensorGetBuffer(_c);
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
        return aaxDriverGetSetup(_c,AAX_VERSION_STRING);
    }


    inline aaxConfig config() {
        return _c;
    }

protected:
    aaxConfig _c;
};


class AeonWave
{
public:
    AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) :
        _ec(0)
    {
        mixer.push_back(std::unique_ptr<Mixer>(new Mixer(n,m)));
        _e[0] = _e[1] = _e[2] = 0;
    }

    AeonWave(enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) : AeonWave(0,m) {}

    ~AeonWave() {}

    // ** enumeration ******
    inline const char* drivers(enum aaxRenderMode m=AAX_WRITE_STEREO) {
        aaxDriverClose(_ec); aaxDriverDestroy(_ec); _em = m; _ec = _e[1] = 0;
        if (_e[0] < aaxDriverGetCount(_em)) {
            _ec = aaxDriverGetByPos(_e[0]++,_em);
        }
        return aaxDriverGetSetup(_ec,AAX_DRIVER_STRING);
    }
    inline const char* devices() { _ed  = _e[2] = 0;
        if (_e[1] < aaxDriverGetDeviceCount(_ec,_em)) {
            _ed = aaxDriverGetDeviceNameByPos(_ec,_e[1]++,_em);
        }
        return _ed;
    }
    inline const char* interfaces() {
        const char *ifs = 0;
        if (_e[2] < aaxDriverGetInterfaceCount(_ec,_ed,_em)) {
            ifs = aaxDriverGetInterfaceNameByPos(_ec,_ed,_e[2]++,_em);
        }
        return ifs;
    }

    // ** support ******
    inline const char* version() {
        return *mixer[0]->version();
    }
    static unsigned major_version() {
        return aaxGetMajorversion();
    }
    static unsigned minor_version() {
        return aaxGetMinorversion();
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
    inline Mixer* mixer(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO) {
        mixer.push_back(std::unique_ptr<Mixer>(new Mixer(n,m)));
        return *mixer.back();
    }
    inline destroy(Mixer* m) {
        mixer.erase(std::remove(mixer.begin(),mixer.end(),*m),mixer.end());
    }

private:
    std::vector<std::unique_ptr<Mixer>> mixer;

    // enumeration
    enum aaxRenderMode _em;
    unsigned int _e[3];
    const char* _ed;
    aaxConfig _ec;
};

}

#endif

