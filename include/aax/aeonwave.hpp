/*
 * Copyright (C) 2015-2018 by Erik Hofman.
 * Copyright (C) 2015-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef AAX_AEONWAVE_HPP
#define AAX_AEONWAVE_HPP 1

#include <unordered_map>
#include <type_traits>
#include <algorithm>
#include <iostream>
#include <utility>
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

inline enum aaxType type(const char *s) {
    return aaxGetTypeByName(s);
}

inline enum aaxWaveformType waveform_type(const char *s) {
    return aaxGetWaveformTypeByName(s);
}

inline enum aaxFrequencyFilterType frequency_filter_type(const char *s) {
    return aaxGetFrequencyFilterTypeByName(s);
}

inline enum aaxDistanceModel distance_model(const char *s) {
    return aaxGetDistanceModelByName(s);
}

inline bool is_valid(void* c, enum aaxHandleType t=AAX_CONFIG) {
    return aaxIsValid(c,t);
}

inline void free(void *ptr) {
    aaxFree(ptr);
}


union dsptype
{
    dsptype(enum aaxFilterType f=AAX_FILTER_NONE) : filter(f) {};
    dsptype(enum aaxEffectType e) : effect(e) {};
    enum aaxFilterType filter;
    enum aaxEffectType effect;
    int eftype;
};

template <typename T>
class Tieable
{
public:
    Tieable() = default;

    Tieable(T v) : val(v) {}

    Tieable(const Tieable&) = default;

    Tieable(Tieable&&) = default;

    virtual ~Tieable() = default;

    friend void swap(Tieable& t1, Tieable& t2) noexcept {
        t1.val = std::move(t2.val);
        t1.tied = std::move(t2.tied);
        t1.param = std::move(t2.param);
        t1.filter = std::move(t2.filter);
        t1.obj = std::move(t2.obj);
        t1.set = std::move(t2.set);
        t1.get = std::move(t2.get);
        t1.dsptype = std::move(t2.dsptype);
    }

    Tieable& operator=(Tieable&&) = default;

    friend std::ostream& operator<<(std::ostream& os, const Tieable& v) {
        os << v.val;
        return os;
    }

    // type operators
    inline T operator+(T v) { return (val + v); }
    inline T operator-(T v) { return (val - v); }
    inline T operator*(T v) { return (val * v); }
    inline T operator/(T v) { return (val / v); }
    inline T operator=(T v) { val = v; fire(); return val; }
    inline T operator+=(T v) { val += v; fire(); return val; }
    inline T operator-=(T v) { val += v; fire(); return val; }
    inline T operator*=(T v) { val += v; fire(); return val; }
    inline T operator/=(T v) { val += v; fire(); return val; }

    inline bool operator==(T v) { val += v; fire(); return val; }
    inline bool operator!=(T v) { val += v; fire(); return val; }
    inline bool operator<(T v) { return (val < v); }
    inline bool operator>(T v) { return (val > v); }
    inline bool operator<=(T v) { return (val <= v); }
    inline bool operator>=(T v) { return (val >= v); }

    // Tieable operators
    inline Tieable operator-() { return -val; }
    inline Tieable operator+(const Tieable& v) { return (val + v.val); }
    inline Tieable operator-(const Tieable& v) { return (val - v.val); }
    inline Tieable operator*(const Tieable& v) { return (val * v.val); }
    inline Tieable operator/(const Tieable& v) { return (val / v.val); }
    inline Tieable operator+=(const Tieable& v) { val+=v.val; fire(); return val; }
    inline Tieable operator-=(const Tieable& v) { val+=v.val; fire(); return val; }
    inline Tieable operator*=(const Tieable& v) { val+=v.val; fire(); return val; }
    inline Tieable operator/=(const Tieable& v) { val+=v.val; fire(); return val; }

    inline bool operator==(const Tieable& v) { return (val == v.val); }
    inline bool operator!=(const Tieable& v) { return (val != v.val); }
    inline bool operator<(const Tieable& v) { return (val < v.val); }
    inline bool operator>(const Tieable& v) { return (val > v.val); }
    inline bool operator<=(const Tieable& v) { return (val <= v.val); }
    inline bool operator>=(const Tieable& v) { return (val >= v.val); }

    operator const T*() const { return &val; }
    operator T() { return val; }

    typedef int set_filter(void*, aaxFilter);
    typedef int set_effect(void*, aaxEffect);
    typedef aaxFilter get_filter(void*, aaxFilterType);
    typedef aaxEffect get_effect(void*, aaxEffectType);

    bool tie(set_filter sfn, get_filter gfn, void* o, aaxFilterType f, int p) {
        if (!tied) {
            set.filter = sfn; get.filter = gfn;
            obj = o; dsptype.filter = f; param = p;
            filter = true; tied = enabled = true; fire();
            return true;
        }
        return false;
    }
    bool tie(set_effect sfn, get_effect gfn, void* o, aaxEffectType e, int p) {
        if (!tied) {
            set.effect = sfn; get.effect = gfn;
            obj = o; dsptype.effect = e; param = p;
            filter = false; tied = enabled = true; fire();
            return true;
        }
        return false;
    }
    void untie() { tied = false; }

protected:
    void fire() {
        if (val == prev) return;
        if (std::is_same<T,float>::value) {
            if (!tied) return;
            if (filter) {
                aaxFilter flt = get.filter(obj, dsptype.filter);
                if (aaxFilterSetParam(flt, param, AAX_LINEAR, val)) {
                     set.filter(obj, flt);
                }
                aaxFilterDestroy(flt);
            } else {
                aaxEffect eff = get.effect(obj, dsptype.effect);
                if (aaxEffectSetParam(eff, param, AAX_LINEAR, val)) {
                    set.effect(obj, eff);
                }
                aaxEffectDestroy(eff);
            }
        } else if (std::is_same<T,int>::value) {
            if (!enabled) return;
            if (filter) {
                aaxFilter flt = get.filter(obj, dsptype.filter);
                if (aaxFilterSetState(flt, val)) set.filter(obj, flt);
                aaxFilterDestroy(flt);
            } else {
                aaxEffect eff = get.effect(obj, dsptype.effect);
                if (aaxEffectSetState(eff, val)) set.effect(obj, eff);
                aaxEffectDestroy(eff);
            }
            enabled = tied;
        }
        prev = val;
    }

private:
    T val, prev = 0;
    bool tied = 0;
    int param = 0;
    bool filter = false;
    bool enabled = false;
    void* obj = nullptr;

    union setter {
        setter() : filter(nullptr) {}
        set_filter* filter;
        set_effect* effect;
    } set;
    union getter {
        getter() : filter(nullptr) {}
        get_filter* filter;
        get_effect* effect;
    } get;
    union dsptype dsptype;
};
typedef Tieable<float> Param;
typedef Tieable<int> Status;


class Obj
{
public:
    typedef int close_fn(void*);

    Obj() = default;

    Obj(void *p, close_fn* c) : ptr(p), closefn(c) {}

    Obj(const Obj& o) noexcept : ptr(o.ptr), closefn(o.closefn) {
        fties = std::move(o.fties);
        ities = std::move(o.ities);
        o.closefn = nullptr;
    }

    Obj(Obj&& o) noexcept : Obj() {
        swap(*this, o);
    }

    virtual ~Obj() {
        fties.clear(); ities.clear();
        if (!!closefn) closefn(ptr);
    }

    friend void swap(Obj& o1, Obj& o2) noexcept {
        std::swap(o1.ptr, o2.ptr);
        std::swap(o1.closefn, o2.closefn);
        o1.fties = std::move(o2.fties);
        o1.ities = std::move(o2.ities);
    }

    Obj& operator=(Obj o) noexcept {
        swap(*this, o);
        return *this;
    }

    bool close() {
        bool rv = (!!closefn) ? closefn(ptr) : false;
        closefn = nullptr;
        return rv;
    }

    void ties_add(Param& pm) {
        auto pi = std::find(fties.begin(),fties.end(),&pm);
        if (pi == fties.end()) fties.push_back(&pm);
    }

    void ties_add(Status& pm) {
        auto pi = std::find(ities.begin(),ities.end(),&pm);
        if (pi == ities.end()) ities.push_back(&pm);
    }

    void untie(Param& pm) {
        auto pi = std::find(fties.begin(),fties.end(),&pm);
        if (pi != fties.end()) { fties.erase(pi); } pm.untie();
    }

    void untie(Status& pm) {
        auto pi = std::find(ities.begin(),ities.end(),&pm);
        if (pi != ities.end()) { ities.erase(pi); } pm.untie();
    }

    operator void*() const {
        return ptr;
    }

    explicit operator bool() {
        return !!ptr;
    }

protected:
    void* ptr = nullptr;
    mutable close_fn* closefn = nullptr;
    std::vector<Param*> fties;
    std::vector<Status*> ities;
};


class Buffer : public Obj
{
public:
    Buffer() = default;

    Buffer(aaxBuffer b, bool o=true)
        : Obj(b, o ? aaxBufferDestroy : 0) {}

    Buffer(aaxConfig c, unsigned int n, unsigned int t, enum aaxFormat f)
        : Obj(aaxBufferCreate(c,n,t,f), aaxBufferDestroy) {}

    Buffer(aaxConfig c, const char* name, bool o=true, bool s=false)
        : Obj(nullptr, nullptr)
    {
        ptr = aaxBufferReadFromStream(c, name);
        if (!ptr) { aaxGetErrorNo();
            ptr = aaxBufferReadFromStream(c, preset_file(c, name).c_str());
        }
        if (!ptr && !s) { aaxGetErrorNo();
            ptr = aaxBufferCreate(c ,1, 1, AAX_PCM16S);
        }
        if (ptr && o) {
            closefn = aaxBufferDestroy;
        }
    }

    Buffer(aaxConfig c, std::string& name, bool o=true, bool s=false)
        : Buffer(c, name.c_str(), o, s) {}

    Buffer(Buffer&&) = default;

    Buffer& operator=(Buffer&&) = default;

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

private:
    std::string preset_file(aaxConfig c, const char* name) {
        std::string rv = aaxDriverGetSetup(c, AAX_SHARED_DATA_DIR);
        rv.append("/"); rv.append(name); rv.append(".aaxs");
        return rv;
    }
};


class dsp : public Obj
{
public:
    dsp() = default;

    dsp(aaxConfig c, enum aaxFilterType f)
        : Obj(c,aaxFilterDestroy), filter(true), dsptype(f) {
        if (!aaxIsValid(c, AAX_FILTER)) ptr = aaxFilterCreate(c,f);
    }

    dsp(aaxConfig c, enum aaxEffectType e)
        : Obj(c,aaxEffectDestroy), filter(false), dsptype(e) {
        if (!aaxIsValid(c, AAX_EFFECT)) ptr = aaxEffectCreate(c,e);
    }

    dsp(dsp&&) = default;

    friend void swap(dsp& o1, dsp& o2) noexcept {
        std::swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        o1.filter = std::move(o2.filter);
        o1.dsptype = std::move(o2.dsptype);
    }

    dsp& operator=(dsp&&) = default;

    inline bool add(Buffer& b) {
        return (filter) ? aaxFilterAddBuffer(ptr,b) : aaxEffectAddBuffer(ptr,b);
    }

    bool set(int s) {
        return (filter) ? aaxFilterSetState(ptr,s) : aaxEffectSetState(ptr,s);
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
    bool set(unsigned s, aaxVec4f v, enum aaxType t=AAX_LINEAR) {
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
        return (filter) ? aaxFilterGetParam(ptr,p,t)
                        : aaxEffectGetParam(ptr,p,t);
    }

    // ** support ******
    inline int type() {
        return dsptype.eftype;
    }
    inline bool is_filter() {
        return filter;
    }

private:
    bool filter = true;
    union dsptype dsptype;
};


class Emitter : public Obj
{
public:
    Emitter() = default;

    Emitter(enum aaxEmitterMode m) : Obj(aaxEmitterCreate(), aaxEmitterDestroy){
        aaxEmitterSetMode(ptr, AAX_POSITION, m);
    }

    Emitter(Emitter&&) = default;

    Emitter& operator=(Emitter&&) = default;

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

    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxFilterType f, int p=0) { ties_add(pm);
        return pm.tie(aaxEmitterSetFilter, aaxEmitterGetFilter, ptr, f, p);
    }
    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxEffectType e, int p=0) { ties_add(pm);
        return pm.tie(aaxEmitterSetEffect, aaxEmitterGetEffect, ptr, e, p);
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
};


class Sensor : public Obj
{
public:
    Sensor() = default;

    explicit Sensor(aaxConfig c, enum aaxRenderMode m=AAX_MODE_READ)
        : Obj(c, aaxDriverDestroy), mode(m) {}

    explicit Sensor(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO)
        : Sensor(aaxDriverOpenByName(n,m), m) {}

    explicit Sensor(std::string& s, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO)
        : Sensor(s.empty() ? NULL : s.c_str(),m) {}

    Sensor(Sensor&&) = default;

    friend void swap(Sensor& o1, Sensor& o2) noexcept {
        std::swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        o1.mode = std::move(o2.mode);
    }

    Sensor& operator=(Sensor&&) = default;

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
    inline bool set(enum aaxSetupType t, const char* s) {
        return aaxDriverSetSetup(ptr,t,s);
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

    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxFilterType f, int p=0) { ties_add(pm);
        if (scenery_filter(f)) {
            return pm.tie(aaxScenerySetFilter, aaxSceneryGetFilter,ptr,f,p);
        } else {
            return pm.tie(aaxMixerSetFilter, aaxMixerGetFilter, ptr, f, p);
        }
    }
    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxEffectType e, int p=0) { ties_add(pm);
        if (scenery_effect(e)) {
            return pm.tie(aaxScenerySetEffect, aaxSceneryGetEffect,ptr,e,p);
        } else {
            return pm.tie(aaxMixerSetEffect, aaxMixerGetEffect, ptr, e, p);
        }
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

    // ** buffer handling (AAXS only) ******
    inline bool add(Buffer& b) {
        return aaxMixerAddBuffer(ptr,b);
    }

    // ** buffer handling ******
    inline bool wait(float t) {
        return aaxSensorWaitForBuffer(ptr,t);
    }
    inline Buffer buffer() {
        return Buffer(aaxSensorGetBuffer(ptr));
    }
    inline Buffer get_buffer() {
        return Buffer(aaxSensorGetBuffer(ptr));
    }

    // ** enumeration ******
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
    inline const char* version() {
        return aaxGetVersionString(ptr);
    }

    inline enum aaxErrorType error_no() {
        return aaxDriverGetErrorNo(ptr);
    }
    inline  const char* strerror() {
        return aaxGetErrorString(error_no());
    }

    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(ptr,t);
    }

protected:
    enum aaxRenderMode mode = AAX_MODE_READ;

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
    Frame() = default;

    Frame(aaxConfig c) : Obj(aaxAudioFrameCreate(c), aaxAudioFrameDestroy) {}

    Frame(Frame&&) = default;

    virtual ~Frame() {
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

    friend void swap(Frame& o1, Frame& o2) noexcept {
        std::swap(static_cast<Obj&>(o1), static_cast<Obj&>(o2));
        o1.frames = std::move(o2.frames);
        o1.sensors = std::move(o2.sensors);
        o1.emitters = std::move(o2.emitters);
    }

    Frame& operator=(Frame&&) = default;

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

    // ** buffer handling (AAXS only) ******
    inline bool add(Buffer& b) {
        return aaxAudioFrameAddBuffer(ptr,b);
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

    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxFilterType f, int p=0) { ties_add(pm);
        return pm.tie(aaxAudioFrameSetFilter, aaxAudioFrameGetFilter, ptr, f, p);
    }
    template <typename T>
    bool tie(Tieable<T>& pm, enum aaxEffectType e, int p=0) { ties_add(pm);
        return pm.tie(aaxAudioFrameSetEffect, aaxAudioFrameGetEffect, ptr, e, p);
    }

    // ** sub-mixing ******
    bool add(Frame& m) {
        frames.push_back(m);
        return aaxAudioFrameRegisterAudioFrame(ptr,m);
    }
    bool remove(Frame& m) {
        auto fi = std::find(frames.begin(),frames.end(),m);
        if (fi != frames.end()) frames.erase(fi);
        return aaxAudioFrameDeregisterAudioFrame(ptr,m);
    }
    bool add(Sensor& s) {
        sensors.push_back(s);
        return aaxAudioFrameRegisterSensor(ptr,s);
    }
    bool remove(Sensor& s) {
        auto si = std::find(sensors.begin(),sensors.end(),s);
        if (si != sensors.end()) sensors.erase(si);
        return aaxAudioFrameDeregisterSensor(ptr,s);
    }
    bool add(Emitter& e) {
        emitters.push_back(e);
        return aaxAudioFrameRegisterEmitter(ptr,e);
    }
    bool remove(Emitter& e) {
        auto ei = std::find(emitters.begin(),emitters.end(),e);
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

private:
    std::vector<aaxFrame> frames;
    std::vector<aaxConfig> sensors;
    std::vector<aaxEmitter> emitters;
};
typedef Frame Mixer;


class AeonWave : public Sensor
{
public:
   AeonWave() = default;

   explicit  AeonWave(const char* n, enum aaxRenderMode m=AAX_MODE_WRITE_STEREO)
        : Sensor(n,m) {}

    explicit AeonWave(std::string& s,enum aaxRenderMode m=AAX_MODE_WRITE_STEREO)
        : AeonWave(s.empty() ? nullptr : s.c_str(),m) {}

    explicit AeonWave(enum aaxRenderMode m)
        : AeonWave(0,m) {}

    AeonWave(AeonWave&&) = default;

    virtual ~AeonWave() {
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
        for(auto it=buffers.begin(); it!=buffers.end(); ++it) {
            aaxBufferDestroy(it->second.second); it->second.first = 0;
        }
        buffers.clear();
    }

    friend void swap(AeonWave& o1, AeonWave& o2) noexcept {
        std::swap(static_cast<Sensor&>(o1), static_cast<Sensor&>(o2));
        o1.frames = std::move(o2.frames);
        o1.sensors = std::move(o2.sensors);
        o1.emitters = std::move(o2.emitters);
        o1.buffers = std::move(o2.buffers);
        std::swap(o1.play, o2.play);
    }

    AeonWave& operator=(AeonWave&&) = default;

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
        _em = m; _ec = nullptr; _e[1] = 0; _e[2] = 0;
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

    // ** support ******
    inline unsigned long offset(enum aaxType t) {
        return aaxSensorGetOffset(ptr,t);
    }

    // ** mixing ******
    bool add(Frame& m) {
        frames.push_back(m);
        return aaxMixerRegisterAudioFrame(ptr,m);
    }
    bool remove(Frame& m) {
        auto fi = std::find(frames.begin(),frames.end(),m);
        if (fi != frames.end()) frames.erase(fi);
        return aaxMixerDeregisterAudioFrame(ptr,m);
    }
    bool add(Sensor& s) {
        sensors.push_back(s);
        return aaxMixerRegisterSensor(ptr,s);
    }
    bool remove(Sensor& s) {
        auto si = std::find(sensors.begin(),sensors.end(),s);
        if (si != sensors.end()) sensors.erase(si);
        return aaxMixerDeregisterSensor(ptr,s);
    }
    bool add(Emitter& e) {
        emitters.push_back(e);
        return aaxMixerRegisterEmitter(ptr,e);
    }
    bool remove(Emitter& e) {
        auto ei = std::find(emitters.begin(),emitters.end(),e);
        if (ei != emitters.end()) emitters.erase(ei);
        return aaxMixerDeregisterEmitter(ptr,e);
    }

    // ** buffer management ******
    // Get a shared buffer from the buffer cache if it's name is already
    // in the cache. Otherwise create a new one and add it to the cache.
    // The name can be an URL or a path to a file or just a reference-id.
    // In the case of an URL or a path the data is read automatically,
    // otherwise the application should add the audio-data itself.
    Buffer& buffer(std::string name, bool strict=false) {
        auto it = buffers.find(name);
        if (it == buffers.end()) {
            Buffer *b = new Buffer(ptr,name,false,strict);
            if (b) {
                auto ret = buffers.insert({name,{0,b}});
                it = ret.first;
            } else {
                delete b;
                return nullBuffer;
            }
        }
        it->second.first++;
        return *it->second.second;
    }
    void destroy(Buffer& b) {
        for(auto it=buffers.begin(); it!=buffers.end(); ++it)
        {
            if ((it->second.second == &b) && it->second.first && !(--it->second.first)) {
                aaxBufferDestroy(it->second.second);
                buffers.erase(it); break;
            }
        }
    }
    bool buffer_avail(std::string &name) {
        auto it = buffers.find(name);
        if (it == buffers.end()) return false;
        return true;
    }

    bool playback() {
       return aaxPlaySoundLogo(play.info(AAX_RENDERER_STRING));
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
    std::vector<aaxConfig> sensors;
    std::vector<aaxEmitter> emitters;
    std::unordered_map<std::string,std::pair<size_t,Buffer*> > buffers;
    Buffer nullBuffer;

    // background music stream
    Sensor play;

    // enumeration
    enum aaxRenderMode _em = AAX_MODE_WRITE_STEREO;
    unsigned int _e[3] = {0, 0, 0};
    const char* _ed = nullptr;
    aaxConfig _ec = nullptr;
};

} // namespace aax

#endif

