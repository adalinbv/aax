/*
 * Copyright (C) 2015-2017 by Erik Hofman.
 * Copyright (C) 2015-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#ifndef AEONWAVE_MATRIX_HPP
#define AEONWAVE_MATRIX_HPP 1

#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

#include <aax/aax.h>

namespace aax
{

template <typename T>
class VecBase
{
protected:
    typedef T VT[4];

public:
    VecBase() : _v4(true) {
        std::fill(_v, _v+4, (T)0);
    }
    VecBase(aaxVec3f& v) : _v4(false) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
    }
    VecBase(aaxVec3d& v) : _v4(false) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
    }
    VecBase(aaxVec4f& v) : _v4(true) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v[3] = static_cast<T>(v[3]);
    }
    VecBase(aaxVec4d& v) : _v4(true) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v[3] = static_cast<T>(v[3]);
    }
    VecBase(T x, T y, T z) : _v4(false) {
        _v[0] = x; _v[1] = y; _v[2] = z; _v[3] = 0;
    }
    VecBase(T w, T x, T y, T z) : _v4(true) {
        _v[0] = w; _v[1] = x; _v[2] = y; _v[3] = z;
    }
    VecBase(T f) : _v4(true) {
        std::fill(_v, _v+4, f);
    }
    ~VecBase() {}

    inline void set(T x, T y, T z) {
        _v[0] = x; _v[1] = y; _v[2] = z; _v[3] = 0; _v4 = false;
    }

    inline void set(T w, T x, T y, T z) {
        _v[0] = w; _v[1] = x; _v[2] = y; _v[3] = z; _v4 = true;
    }

    inline void set(T f)  {
        std::fill(_v, _v+4, f); _v4 = true;
    }

    inline T magnitude2() {
        return dot_product(*this);
    }
    inline T magnitude() {
        return std::sqrt(dot_product(*this));
    }
    T normalize() {
        float m = magnitude();
        if (m) *this /= m;
        else *this = 0;
        return m;
    }
    VecBase normalized() {
        VecBase r(_v); r.normalize();
        return r;
    }
    T dot_product(const VecBase& v) {
        T d = _v[0]*v[0] + _v[1]*v[1] + _v[2]*v[2];
        if (_v4) d += _v[3]*v[3];
        return d;
    }
    VecBase cross_product(const VecBase& v2) {
        VecBase r;
        if (_v4 == false) {
            r[0] = _v[1]*v2[2] - _v[2]*v2[1];
            r[1] = _v[2]*v2[0] - _v[0]*v2[2];
            r[2] = _v[0]*v2[1] - _v[1]*v2[0];
        }
        return r;
    }

    // ** support ******
    VecBase& operator=(const VecBase& v) {
        std::copy(v._v, v._v+4, _v);
        _v4 = v.is_v4();
        return *this;
    }
    VecBase& operator=(T f) {
        std::fill(_v, _v+4, f);
        return *this;
    }
    VecBase& operator=(aaxVec3f& v) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v4 = false;
        return *this;
    }
    VecBase& operator=(aaxVec3d& v) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v4 = false;
        return *this;
    }
    VecBase& operator=(aaxVec4f& v) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v[3] = static_cast<T>(v[3]);
        _v4 = true;
        return *this;
    }
    VecBase& operator=(aaxVec4d& v) {
        _v[0] = static_cast<T>(v[0]);
        _v[1] = static_cast<T>(v[1]);
        _v[2] = static_cast<T>(v[2]);
        _v[3] = static_cast<T>(v[3]);
        _v4 = true;
        return *this;
    }
    VecBase& operator+=(T f) {
        _v[0] += f; _v[1] += f; _v[2] += f;
        if (_v4) _v[3] += f;
        return *this;
    }
    VecBase& operator+=(const VecBase& v) {
        _v[0] += v[0]; _v[1] += v[1]; _v[2] += v[2];
        if (_v4) _v[3] += v[3];
        return *this;
    }
    VecBase& operator-=(T f) {
        _v[0] -= f; _v[1] -= f; _v[2] -= f;
        if (_v4) _v[3] -= f;
        return *this;
    }
    VecBase& operator-=(const VecBase& v) {
        _v[0] -= v[0]; _v[1] -= v[1]; _v[2] -= v[2];
        if (_v4) _v[3] -= v[3];
        return *this;
    }
    VecBase& operator*=(T f) {
        _v[0] *= f; _v[1] *= f; _v[2] *= f;
        if (_v4) _v[3] *= f;
        return *this;
    }
    VecBase& operator*=(const VecBase& v) {
        _v[0] *= v[0]; _v[1] *= v[1]; _v[2] *= v[2];
        if (_v4) _v[3] *= v[3];
        return *this;
    }
    VecBase& operator/=(T f) {
        T fi = static_cast<T>(1)/f;
        _v[0] *= fi; _v[1] *= fi; _v[2] *= fi;
        if (_v4) _v[3] *= fi;
        return *this;
    }
    VecBase& operator/=(const VecBase& v) {
        _v[0] /= v[0]; _v[1] /= v[1]; _v[2] /= v[2];
        if (_v4) _v[3] /= v[3];
        return *this;
    }
    VecBase operator+(T f) {
        VecBase r(_v); r += f;
        return r;
    }
    VecBase operator+(const VecBase& v) {
       VecBase r(_v); r += v;
       return r;
    }
    VecBase operator-(T f) {
        VecBase r(_v); r -= f;
        return r;
    }
    VecBase operator-(const VecBase& v) {
       VecBase r(_v); r -= v;
       return r;
    }
    VecBase operator*(T f) {
        VecBase r(_v); r *= f;
        return r;
    }
    VecBase operator*(const VecBase& v) {
       VecBase r(_v); r *= v;
       return r;
    }
    VecBase operator/(T f) {
        VecBase r(_v); r /= f;
        return r;
    }
    VecBase operator/(const VecBase& v) {
       VecBase r(_v); r /= v;
       return r;
    }
    VecBase operator-() {
        VecBase v4;
        v4[0] = -_v[0]; v4[1] = -_v[1]; v4[2] = -_v[2];
        if (_v4) v4[3] = -_v[3];
        return v4;
    }
    operator const T*() const {
        return _v;
    }
    operator T*() {
        return _v;
    }
    inline bool is_v4() const {
        return _v4;
    }

private:
    VT _v;
    bool _v4;
};
typedef VecBase<float> Vector;
typedef VecBase<double> Vector64;


template <typename T>
class MtxBase
{
protected:
    typedef T MT[4][4];

public:
    MtxBase() {}
    ~MtxBase() {}

    // ** support ******
    inline bool operator==(MtxBase<T>& m) {
        return mtxcmp(m);
    }
    inline bool operator==(const MT& m) {
        return mtxcmp(m);
    }
    inline bool operator!=(MtxBase<T>& m) {
        return ~mtxcmp(m);
    }
    inline bool operator!=(const MT& m) {
        return ~mtxcmp(m);
    }
    operator const MT&() const {
        return _m;
    }
    operator MT&() {
        return _m;
    }
    friend std::ostream& operator<<(std::ostream& s, MtxBase<T>& mtx) {
        const MT& m = mtx;
        s << std::fixed << std::showpoint << std::setprecision(4);
        for (unsigned j = 0; j<4; ++j) {
            s << "[";
            for (unsigned i=0; i<4; ++i) {
                s << " " << std::setw(12) << m[i][j];
            }
            s << " ]" << std::endl;
        }
        return s;
    }

protected:
    MT _m;

private:
    bool mtxcmp(MtxBase<T>& mtx) {
        const MT& m = mtx;
        T eps = std::numeric_limits<T>::epsilon();
        for(unsigned i=0; i<4; ++i) {
            for(unsigned j=0; j<4; ++j) {
                T x = _m[i][j], y = m[i][j];
                if (std::abs(x-y) > eps*std::max<T>(std::max<T>(static_cast<T>(1),std::abs(x)),std::abs(y))) return false;
            }
        }
        return true;
    }
};

class Matrix64 : public MtxBase<double>
{
public:
    Matrix64() {
        aaxMatrix64SetIdentityMatrix(_m);
    }
    Matrix64(aaxMtx4f& m) {
        aaxMatrixToMatrix64(_m,m);
    }
    Matrix64(aaxMtx4d m) {
        aaxMatrix64CopyMatrix64(_m,m);
    }
    Matrix64(MtxBase<float>& m) {
        aaxMatrixToMatrix64(_m,m);
    }
    Matrix64(MtxBase<double>& m) {
        aaxMatrix64CopyMatrix64(_m,m);
    }
    Matrix64(Vector64& p, Vector& a) {
        set(p,a);
    }
    Matrix64(aaxVec3d& p, aaxVec3f& a) {
        set(p,a);
    }
    Matrix64(Vector64& p, Vector& a, Vector& u) {
        set(p,a,u);
    }
    Matrix64(aaxVec3d& p, aaxVec3f& a, aaxVec3f& u) {
        set(p,a,u);
    }
    ~Matrix64() {}

    inline bool set(Vector64& p, Vector& a) {
        return aaxMatrix64SetDirection(_m,p,a);
    }
    inline bool set(aaxVec3d& p, aaxVec3f& a) {
        return aaxMatrix64SetDirection(_m,p,a);
    }
    inline bool set(Vector64& p, Vector& a, Vector& u) {
        return aaxMatrix64SetOrientation(_m,p,a,u);
    }
    inline bool set(aaxVec3d& p, aaxVec3f& a, aaxVec3f& u) {
        return aaxMatrix64SetOrientation(_m,p,a,u);
    }
    inline bool get(aaxVec3d& p, aaxVec3f& a, aaxVec3f& u) {
        return aaxMatrix64GetOrientation(_m,p,a,u);
    }
    inline bool translate(double dx, double dy, double dz) {
        return aaxMatrix64Translate(_m,dx,dy,dz);
    }
    inline bool translate(Vector64& t) {
        return aaxMatrix64Translate(_m,t[0],t[1],t[2]);
    }
    inline bool translate(aaxVec3d& t) {
        return aaxMatrix64Translate(_m,t[0],t[1],t[2]);
    }
    inline bool rotate(double a, double x, double y, double z) {
        return aaxMatrix64Rotate(_m,a,x,y,z);
    }
    inline bool multiply(MtxBase<double>& m) {
        return aaxMatrix64Multiply(_m,m);
    }
    inline bool inverse() {
        return aaxMatrix64Inverse(_m);
    }

    // ** support ******
    Matrix64& operator*=(MtxBase<double>& m) {
        aaxMatrix64Multiply(_m,m);
        return *this;
    }
    Matrix64& operator/=(MtxBase<double>& m) {
        aaxMtx4d im;
        aaxMatrix64CopyMatrix64(im,m);
        aaxMatrix64Inverse(im);
        aaxMatrix64Multiply(_m,im);
        return *this;
    }
    Matrix64& operator+=(Vector& v) {
        aaxMatrix64Translate(_m,static_cast<double>(v[0]),static_cast<double>(v[1]),static_cast<double>(v[2]));
        return *this;
    }
    Matrix64& operator+=(Vector64& v) {
        aaxMatrix64Translate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix64& operator+=(aaxVec3f& v) {
        aaxMatrix64Translate(_m,static_cast<double>(v[0]),static_cast<double>(v[1]),static_cast<double>(v[2]));
        return *this;
    }
    Matrix64& operator+=(aaxVec3d& v) {
        aaxMatrix64Translate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix64& operator-=(Vector& v) {
        aaxMatrix64Translate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix64& operator-=(Vector64& v) {
        aaxMatrix64Translate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix64& operator-=(aaxVec3f& v) {
        aaxMatrix64Translate(_m,static_cast<double>(-v[0]),static_cast<double>(-v[1]),static_cast<double>(-v[2]));
        return *this;
    }
    Matrix64& operator-=(aaxVec3d& v) {
        aaxMatrix64Translate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix64 operator~() {
        aaxMtx4d im;
        aaxMatrix64CopyMatrix64(im,_m);
        aaxMatrix64Inverse(im);
        return Matrix64(im);
    }
    Matrix64& operator=(aaxMtx4d& m) {
        aaxMatrix64CopyMatrix64(_m,m);
        return *this;
    }
    Matrix64& operator=(MtxBase<float>& m) {
        aaxMatrixToMatrix64(_m, m);
        return *this;
    }
    Matrix64& operator=(MtxBase<double>& m) {
        aaxMatrix64CopyMatrix64(_m,m);
        return *this;
    }
    friend Matrix64 operator*(MtxBase<double>& m1, MtxBase<double>& m2) {
        aaxMtx4d m;
        aaxMatrix64CopyMatrix64(m,m1);
        aaxMatrix64Multiply(m,m2);
        return m;
    }

private:
    void set(unsigned p, const Vector64& v) {
        if (v.is_v4()) std::copy(v+0, v+4, _m[p]);
        else std::copy(v+0, v+3, _m[p]);
    }
    void get(unsigned p, aaxVec3d& v) {
        std::copy(_m[p], _m[p]+4, v+0);
    }
};


class Matrix : public MtxBase<float>
{
public:
    Matrix() {
        aaxMatrixSetIdentityMatrix(_m);
    }
    Matrix(aaxMtx4f& m) {
        aaxMatrixCopyMatrix(_m,m);
    }
    Matrix(MtxBase<float>& m) {
        aaxMatrixCopyMatrix(_m,m);
    }
    Matrix(Vector& p, Vector& a) {
        set(p,a);
    }
    Matrix(aaxVec3f& p, aaxVec3f& a) {
        set(p,a);
    }
    Matrix(Vector& p, Vector& a, Vector& u) {
        set(p,a,u);
    }
    Matrix(aaxVec3f& p, aaxVec3f& a, aaxVec3f& u) {
        set(p,a,u);
    }
    ~Matrix() {}
    
    bool set(Vector& p, Vector& a) {
        float eps = std::numeric_limits<float>::epsilon();
        Vector u(0.0, 1.0, 0.0);
        if (std::abs(a[0])<=eps && std::abs(a[2])<=eps) {
            u[1] = 0.0f; u[2] = (a[2] < 0.0f) ? -1.0f : 1.0f;
        }
        return set(p, a, u);
    }
    bool set(aaxVec3f& p, aaxVec3f& a) {
        float eps = std::numeric_limits<float>::epsilon();
        float u[3] = {0.0, 1.0, 0.0};
        if (std::abs(a[0])<eps && std::abs(a[2])<eps) {
            u[1] = 0.0f; u[2] = (a[2] < 0.0f) ? -1.0f : 1.0f;
        }
        return set(p, a, u);
    }
    bool set(Vector& p, Vector& a, Vector& u) {
        Vector pos(p[0], p[1], p[2], -1.0);
        Vector at(a[0], a[1], a[2]);
        Vector up(u[0], u[1], u[2]);
        Vector side = at.cross_product(up);
        set(0, side.normalized());
        set(1, up.normalized());
        set(2, -at.normalized());
        set(3, -pos);
        return true;
    }
    bool set(aaxVec3d& p, aaxVec3d& a, aaxVec3d& u) {
        Vector pos = p, at = a, up = u;
        return set(pos, at, up);
    }
    bool set(aaxVec3f& p, aaxVec3f& a, aaxVec3f& u) {
        Vector pos(p), at(a), up(u);
        return set(pos, at, up);
    }
    bool get(aaxVec3f& p, aaxVec3f& a, aaxVec3f& u) {
        get(1, u); get(2, a); get(3, p);
        return true;
    }
    inline bool translate(float dx, float dy, float dz) {
        return aaxMatrixTranslate(_m,dx,dy,dz);
    }
    inline bool translate(Vector& t) {
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool translate(aaxVec3f& t) {
        return aaxMatrixTranslate(_m,t[0],t[1],t[2]);
    }
    inline bool rotate(float a, float x, float y, float z) {
        return aaxMatrixRotate(_m,a,x,y,z);
    }
    inline bool multiply(MtxBase<float>& m) {
        return aaxMatrixMultiply(_m,m);
    }
    inline bool inverse() {
        return aaxMatrixInverse(_m);
    }

    Matrix64 toMatrix64() {
        aaxMtx4d m; aaxMatrixToMatrix64(m, _m);
        return Matrix64(m);
    }

    // ** support ******
    Matrix& operator*=(MtxBase<float>& m) {
        aaxMatrixMultiply(_m,m);
        return *this;
    }
    Matrix& operator/=(MtxBase<float>& m) {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im,m);
        aaxMatrixInverse(im);
        aaxMatrixMultiply(_m,im);
        return *this;
    }
    Matrix& operator+=(Vector& v) {
        aaxMatrixTranslate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix& operator+=(aaxVec3f& v) {
        aaxMatrixTranslate(_m,v[0],v[1],v[2]);
        return *this;
    }
    Matrix& operator-=(Vector& v) {
        aaxMatrixTranslate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix& operator-=(aaxVec3f& v) {
        aaxMatrixTranslate(_m,-v[0],-v[1],-v[2]);
        return *this;
    }
    Matrix operator~() {
        aaxMtx4f im;
        aaxMatrixCopyMatrix(im,_m);
        aaxMatrixInverse(im);
        return Matrix(im);
    }
    Matrix& operator=(aaxMtx4f& m) {
        aaxMatrixCopyMatrix(_m,m);
        return *this;
    }
    Matrix& operator=(MtxBase<float>& m) {
        aaxMatrixCopyMatrix(_m, m);
        return *this;
    }
    friend Matrix operator*(MtxBase<float>& m1, MtxBase<float>& m2) {
        aaxMtx4f m;
        aaxMatrixCopyMatrix(m,m1);
        aaxMatrixMultiply(m,m2);
        return Matrix(m);
    }

private:
    void set(unsigned p, const Vector& v) {
        if (v.is_v4()) std::copy(v+0, v+4, _m[p]);
        else std::copy(v+0, v+3, _m[p]);
    }
    void get(unsigned p, aaxVec3f& v) {
        std::copy(_m[p], _m[p]+4, v+0);
    }
};


} // namespace aax

#endif

